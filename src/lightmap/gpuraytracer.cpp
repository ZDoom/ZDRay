
#include "math/mathlib.h"
#include "levelmesh.h"
#include "level/level.h"
#include "gpuraytracer.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include "vulkanbuilders.h"
#include <map>
#include <vector>
#include <algorithm>
#include <limits>
#include "glsl_rgen_bounce.h"
#include "glsl_rgen_light.h"
#include "glsl_rchit_bounce.h"
#include "glsl_rchit_light.h"
#include "glsl_rchit_sun.h"
#include "glsl_rmiss_bounce.h"
#include "glsl_rmiss_light.h"
#include "glsl_rmiss_sun.h"

extern bool VKDebug;

GPURaytracer::GPURaytracer()
{
	device = std::make_unique<VulkanDevice>(0, VKDebug);
	PrintVulkanInfo();
}

GPURaytracer::~GPURaytracer()
{
}

void GPURaytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	printf("Building vulkan acceleration structures\n");

	if (device->renderdoc)
		device->renderdoc->StartFrameCapture(0, 0);

	CreateVulkanObjects();

	std::vector<TraceTask> tasks;
	for (size_t i = 0; i < mesh->lightProbes.size(); i++)
	{
		TraceTask task;
		task.id = -(int)(i + 2);
		task.x = 0;
		task.y = 0;
		tasks.push_back(task);
	}

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();
		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		for (int y = 0; y < sampleHeight; y++)
		{
			for (int x = 0; x < sampleWidth; x++)
			{
				TraceTask task;
				task.id = (int)i;
				task.x = x;
				task.y = y;
				tasks.push_back(task);
			}
		}
	}

	std::vector<vec3> HemisphereVectors;
	HemisphereVectors.reserve(bounceSampleCount);
	for (int i = 0; i < bounceSampleCount; i++)
	{
		vec2 Xi = Hammersley(i, bounceSampleCount);
		vec3 H;
		H.x = Xi.x * 2.0f - 1.0f;
		H.y = Xi.y * 2.0f - 1.0f;
		H.z = RadicalInverse_VdC(i) + 0.01f;
		H = normalize(H);
		HemisphereVectors.push_back(H);
	}

	printf("Ray tracing with %d bounce(s)\n", mesh->map->LightBounce);

	size_t maxTasks = (size_t)rayTraceImageSize * rayTraceImageSize;
	for (size_t startTask = 0; startTask < tasks.size(); startTask += maxTasks)
	{
		size_t numTasks = std::min(tasks.size() - startTask, maxTasks);
		UploadTasks(tasks.data() + startTask, numTasks);

		BeginTracing();

		Uniforms uniforms = {};
		uniforms.SampleDistance = (float)mesh->samples;
		uniforms.LightCount = mesh->map->ThingLights.Size();
		uniforms.SunDir = mesh->map->GetSunDirection();
		uniforms.SunColor = mesh->map->GetSunColor();
		uniforms.SunIntensity = 1.0f;

		uniforms.PassType = 0;
		uniforms.SampleIndex = 0;
		uniforms.SampleCount = bounceSampleCount;
		RunTrace(uniforms, rgenBounceRegion);

		uniforms.SampleCount = coverageSampleCount;
		RunTrace(uniforms, rgenLightRegion);

		for (uint32_t i = 0; i < (uint32_t)bounceSampleCount; i++)
		{
			uniforms.PassType = 1;
			uniforms.SampleIndex = i;
			uniforms.SampleCount = bounceSampleCount;
			uniforms.HemisphereVec = HemisphereVectors[uniforms.SampleIndex];
			RunTrace(uniforms, rgenBounceRegion);

			for (int bounce = 0; bounce < mesh->map->LightBounce; bounce++)
			{
				uniforms.SampleCount = coverageSampleCount;
				RunTrace(uniforms, rgenLightRegion);

				uniforms.PassType = 2;
				uniforms.SampleIndex = (i + bounce) % uniforms.SampleCount;
				uniforms.SampleCount = bounceSampleCount;
				uniforms.HemisphereVec = HemisphereVectors[uniforms.SampleIndex];
				RunTrace(uniforms, rgenBounceRegion);
			}
		}

		EndTracing();
		DownloadTasks(tasks.data() + startTask, numTasks);
	}

	if (device->renderdoc)
		device->renderdoc->EndFrameCapture(0, 0);

	printf("\nRaytrace complete\n");
}

void GPURaytracer::CreateVulkanObjects()
{
	cmdpool = std::make_unique<VulkanCommandPool>(device.get(), device->graphicsFamily);
	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();

	CreateVertexAndIndexBuffers();
	CreateBottomLevelAccelerationStructure();
	CreateTopLevelAccelerationStructure();
	CreateShaders();
	CreatePipeline();
	CreateDescriptorSet();

	PipelineBarrier finishbuildbarrier;
	finishbuildbarrier.addMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
	finishbuildbarrier.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}

void GPURaytracer::UploadTasks(const TraceTask* tasks, size_t size)
{
	if (!cmdbuffer)
	{
		cmdbuffer = cmdpool->createBuffer();
		cmdbuffer->begin();
	}

	size_t maxTasks = (size_t)rayTraceImageSize * rayTraceImageSize;
	if (size > maxTasks)
		throw std::runtime_error("Ray trace task count is too large");

	size_t imageSize = sizeof(vec4) * rayTraceImageSize * rayTraceImageSize;
	uint8_t* imageData = (uint8_t*)imageTransferBuffer->Map(0, imageSize);
	vec4* startPositions = (vec4*)imageData;
	for (size_t i = 0; i < size; i++)
	{
		const TraceTask& task = tasks[i];

		if (task.id >= 0)
		{
			Surface* surface = mesh->surfaces[task.id].get();
			vec3 pos = surface->lightmapOrigin + surface->lightmapSteps[0] * (float)task.x + surface->lightmapSteps[1] * (float)task.y;
			startPositions[i] = vec4(pos, (float)task.id);
		}
		else
		{
			LightProbeSample& probe = mesh->lightProbes[(size_t)(-task.id) - 2];
			startPositions[i] = vec4(probe.Position, (float)-2);
		}

	}
	for (size_t i = size; i < maxTasks; i++)
	{
		startPositions[i] = vec4(0.0f, 0.0f, 0.0f, -1.0f);
	}
	imageTransferBuffer->Unmap();

	PipelineBarrier barrier1;
	barrier1.addImage(startPositionsImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	barrier1.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.imageExtent.width = rayTraceImageSize;
	region.imageExtent.height = rayTraceImageSize;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyBufferToImage(imageTransferBuffer->buffer, startPositionsImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	PipelineBarrier barrier2;
	barrier2.addBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	barrier2.addImage(startPositionsImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	barrier2.addImage(positionsImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	barrier2.addImage(outputImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	barrier2.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}

void GPURaytracer::BeginTracing()
{
	uniformsIndex = 0;
	mappedUniforms = (uint8_t*)uniformTransferBuffer->Map(0, uniformStructs * uniformStructStride);

	cmdbuffer->copyBuffer(uniformTransferBuffer.get(), uniformBuffer.get());
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
}

void GPURaytracer::RunTrace(const Uniforms& uniforms, const VkStridedDeviceAddressRegionKHR& rgenShader)
{
	if (uniformsIndex == uniformStructs)
	{
		EndTracing();
		BeginTracing();
	}

	*reinterpret_cast<Uniforms*>(mappedUniforms + uniformStructStride * uniformsIndex) = uniforms;

	if (uniformsIndex == 0)
	{
		PipelineBarrier barrier;
		barrier.addBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		barrier.addImage(positionsImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		barrier.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	}
	else
	{
		PipelineBarrier barrier;
		barrier.addImage(positionsImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		barrier.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	}

	uint32_t offset = (uint32_t)(uniformsIndex * uniformStructStride);
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0, descriptorSet.get(), 1, &offset);
	cmdbuffer->traceRays(&rgenShader, &missRegion, &hitRegion, &callRegion, rayTraceImageSize, rayTraceImageSize, 1);

	uniformsIndex++;
}

void GPURaytracer::EndTracing()
{
	mappedUniforms = nullptr;
	uniformTransferBuffer->Unmap();
	cmdbuffer->end();

	SubmitCommands();
	printf(".");

	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();
}

void GPURaytracer::SubmitCommands()
{
	auto submitFence = std::make_unique<VulkanFence>(device.get());

	QueueSubmit submit;
	submit.addCommandBuffer(cmdbuffer.get());
	submit.execute(device.get(), device->graphicsQueue, submitFence.get());

	VkResult result = vkWaitForFences(device->device, 1, &submitFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkWaitForFences failed");
	result = vkResetFences(device->device, 1, &submitFence->fence);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkResetFences failed");
	cmdbuffer.reset();
}

void GPURaytracer::DownloadTasks(const TraceTask* tasks, size_t size)
{
	PipelineBarrier barrier4;
	barrier4.addImage(outputImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	barrier4.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.imageExtent.width = rayTraceImageSize;
	region.imageExtent.height = rayTraceImageSize;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyImageToBuffer(outputImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageTransferBuffer->buffer, 1, &region);
	cmdbuffer->end();

	SubmitCommands();

	size_t imageSize = sizeof(vec4) * rayTraceImageSize * rayTraceImageSize;
	uint8_t* imageData = (uint8_t*)imageTransferBuffer->Map(0, imageSize);
	vec4* output = (vec4*)imageData;
	for (size_t i = 0; i < size; i++)
	{
		const TraceTask& task = tasks[i];
		if (task.id >= 0)
		{
			Surface* surface = mesh->surfaces[task.id].get();
			size_t sampleWidth = surface->lightmapDims[0];
			surface->samples[task.x + task.y * sampleWidth] = vec3(output[i].x, output[i].y, output[i].z);
		}
		else
		{
			LightProbeSample& probe = mesh->lightProbes[(size_t)(-task.id) - 2];
			probe.Color = vec3(output[i].x, output[i].y, output[i].z);
		}
	}
	imageTransferBuffer->Unmap();
}

void GPURaytracer::CreateVertexAndIndexBuffers()
{
	std::vector<SurfaceInfo> surfaces;
	surfaces.reserve(mesh->surfaces.size());
	for (const auto& surface : mesh->surfaces)
	{
		SurfaceLightDef* def = nullptr;
		if (surface->type >= ST_MIDDLESIDE && surface->type <= ST_LOWERSIDE)
		{
			int lightdefidx = mesh->map->Sides[surface->typeIndex].lightdef;
			if (lightdefidx != -1)
			{
				def = &mesh->map->SurfaceLights[lightdefidx];
			}
		}
		else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
		{
			MapSubsectorEx* sub = &mesh->map->GLSubsectors[surface->typeIndex];
			IntSector* sector = mesh->map->GetSectorFromSubSector(sub);

			if (sector && surface->numVerts > 0)
			{
				if (sector->floorlightdef != -1 && surface->type == ST_FLOOR)
				{
					def = &mesh->map->SurfaceLights[sector->floorlightdef];
				}
				else if (sector->ceilinglightdef != -1 && surface->type == ST_CEILING)
				{
					def = &mesh->map->SurfaceLights[sector->ceilinglightdef];
				}
			}
		}

		SurfaceInfo info;
		info.Sky = surface->bSky ? 1.0f : 0.0f;
		info.Normal = surface->plane.Normal();
		if (def)
		{
			info.EmissiveDistance = def->distance + def->distance;
			info.EmissiveIntensity = def->intensity;
			info.EmissiveColor = def->rgb;
		}
		else
		{
			info.EmissiveDistance = 0.0f;
			info.EmissiveIntensity = 0.0f;
			info.EmissiveColor = vec3(0.0f, 0.0f, 0.0f);
		}
		surfaces.push_back(info);
	}

	std::vector<LightInfo> lights;
	for (ThingLight& light : mesh->map->ThingLights)
	{
		LightInfo info;
		info.Origin = light.LightOrigin();
		info.Radius = light.LightRadius();
		info.Intensity = light.intensity;
		info.InnerAngleCos = light.innerAngleCos;
		info.OuterAngleCos = light.outerAngleCos;
		info.SpotDir = light.SpotDir();
		info.Color = light.rgb;
		lights.push_back(info);
	}

	if (lights.empty()) // vulkan doesn't support zero byte buffers
		lights.push_back(LightInfo());

	size_t vertexbuffersize = (size_t)mesh->MeshVertices.Size() * sizeof(vec3);
	size_t indexbuffersize = (size_t)mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t surfaceindexbuffersize = (size_t)mesh->MeshSurfaces.Size() * sizeof(uint32_t);
	size_t surfacebuffersize = (size_t)surfaces.size() * sizeof(SurfaceInfo);
	size_t lightbuffersize = (size_t)lights.size() * sizeof(LightInfo);
	size_t transferbuffersize = vertexbuffersize + indexbuffersize + surfaceindexbuffersize + surfacebuffersize + lightbuffersize;
	size_t vertexoffset = 0;
	size_t indexoffset = vertexoffset + vertexbuffersize;
	size_t surfaceindexoffset = indexoffset + indexbuffersize;
	size_t surfaceoffset = surfaceindexoffset + surfaceindexbuffersize;
	size_t lightoffset = surfaceoffset + surfacebuffersize;

	BufferBuilder vbuilder;
	vbuilder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	vbuilder.setSize(vertexbuffersize);
	vertexBuffer = vbuilder.create(device.get());
	vertexBuffer->SetDebugName("vertexBuffer");

	BufferBuilder ibuilder;
	ibuilder.setUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	ibuilder.setSize(indexbuffersize);
	indexBuffer = ibuilder.create(device.get());
	indexBuffer->SetDebugName("indexBuffer");

	BufferBuilder sibuilder;
	sibuilder.setUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	sibuilder.setSize(surfaceindexbuffersize);
	surfaceIndexBuffer = sibuilder.create(device.get());
	surfaceIndexBuffer->SetDebugName("surfaceIndexBuffer");

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	sbuilder.setSize(surfacebuffersize);
	surfaceBuffer = sbuilder.create(device.get());
	surfaceBuffer->SetDebugName("surfaceBuffer");

	BufferBuilder lbuilder;
	lbuilder.setUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	lbuilder.setSize(lightbuffersize);
	lightBuffer = lbuilder.create(device.get());
	lightBuffer->SetDebugName("lightBuffer");

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(transferbuffersize);
	transferBuffer = tbuilder.create(device.get());
	transferBuffer->SetDebugName("transferBuffer");
	uint8_t* data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	memcpy(data + vertexoffset, mesh->MeshVertices.Data(), vertexbuffersize);
	memcpy(data + indexoffset, mesh->MeshElements.Data(), indexbuffersize);
	memcpy(data + surfaceindexoffset, mesh->MeshSurfaces.Data(), surfaceindexbuffersize);
	memcpy(data + surfaceoffset, surfaces.data(), surfacebuffersize);
	memcpy(data + lightoffset, lights.data(), lightbuffersize);
	transferBuffer->Unmap();

	cmdbuffer->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), surfaceIndexBuffer.get(), surfaceindexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), surfaceBuffer.get(), surfaceoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), lightBuffer.get(), lightoffset);

	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	cmdbuffer->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void GPURaytracer::CreateBottomLevelAccelerationStructure()
{
	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = vertexBuffer->buffer;
	VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device->device, &info);

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = indexBuffer->buffer;
	VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device->device, &info);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = sizeof(vec3);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.maxVertex = mesh->MeshVertices.Size();

	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = mesh->MeshElements.Size() / 3;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = accelStructBLDesc.flags | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructBLDesc;

	uint32_t maxPrimitiveCount = rangeInfo.primitiveCount;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	BufferBuilder blbufbuilder;
	blbufbuilder.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	blbufbuilder.setSize(sizeInfo.accelerationStructureSize);
	blAccelStructBuffer = blbufbuilder.create(device.get());
	blAccelStructBuffer->SetDebugName("blAccelStructBuffer");

	VkAccelerationStructureKHR blAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.buffer = blAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &blAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	blAccelStruct = std::make_unique<VulkanAccelerationStructure>(device.get(), blAccelStructHandle);

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	sbuilder.setSize(sizeInfo.buildScratchSize);
	blScratchBuffer = sbuilder.create(device.get());
	blScratchBuffer->SetDebugName("blScratchBuffer");

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = blScratchBuffer->buffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device->device, &info);

	buildInfo.dstAccelerationStructure = blAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	cmdbuffer->buildAccelerationStructures(1, &buildInfo, rangeInfos);
}

void GPURaytracer::CreateTopLevelAccelerationStructure()
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	addressInfo.accelerationStructure = blAccelStruct->accelstruct;
	VkDeviceAddress blAccelStructAddress = vkGetAccelerationStructureDeviceAddressKHR(device->device, &addressInfo);

	VkAccelerationStructureInstanceKHR instance = {};
	instance.transform.matrix[0][0] = 1.0f;
	instance.transform.matrix[1][1] = 1.0f;
	instance.transform.matrix[2][2] = 1.0f;
	instance.instanceCustomIndex = 0;
	instance.accelerationStructureReference = blAccelStructAddress;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.mask = 0xff;
	instance.instanceShaderBindingTableRecordOffset = 0;

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer = tbuilder.create(device.get());
	tlTransferBuffer->SetDebugName("tlTransferBuffer");
	auto data = (uint8_t*)tlTransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR));
	memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer->Unmap();

	BufferBuilder instbufbuilder;
	instbufbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	instbufbuilder.setSize(sizeof(VkAccelerationStructureInstanceKHR));
	tlInstanceBuffer = instbufbuilder.create(device.get());
	tlInstanceBuffer->SetDebugName("tlInstanceBuffer");

	cmdbuffer->copyBuffer(tlTransferBuffer.get(), tlInstanceBuffer.get());

	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	cmdbuffer->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = tlInstanceBuffer->buffer;
	VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(device->device, &info);

	VkAccelerationStructureGeometryInstancesDataKHR instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instances.data.deviceAddress = instanceBufferAddress;

	VkAccelerationStructureGeometryKHR accelStructTLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	accelStructTLDesc.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelStructTLDesc.geometry.instances = instances;

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = 1;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructTLDesc;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	uint32_t maxInstanceCount = 1;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxInstanceCount, &sizeInfo);

	BufferBuilder tlbufbuilder;
	tlbufbuilder.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	tlbufbuilder.setSize(sizeInfo.accelerationStructureSize);
	tlAccelStructBuffer = tlbufbuilder.create(device.get());
	tlAccelStructBuffer->SetDebugName("tlAccelStructBuffer");

	VkAccelerationStructureKHR tlAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.buffer = tlAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &tlAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	tlAccelStruct = std::make_unique<VulkanAccelerationStructure>(device.get(), tlAccelStructHandle);

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	sbuilder.setSize(sizeInfo.buildScratchSize);
	tlScratchBuffer = sbuilder.create(device.get());
	tlScratchBuffer->SetDebugName("tlScratchBuffer");

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = tlScratchBuffer->buffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device->device, &info);

	buildInfo.dstAccelerationStructure = tlAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = scratchAddress;

	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	cmdbuffer->buildAccelerationStructures(1, &buildInfo, rangeInfos);
}

void GPURaytracer::CreateShaders()
{
	rgenBounce = CompileRayGenShader(glsl_rgen_bounce, "rgen.bounce");
	rgenLight = CompileRayGenShader(glsl_rgen_light, "rgen.light");
	rchitBounce = CompileClosestHitShader(glsl_rchit_bounce, "rchit.bounce");
	rchitLight = CompileClosestHitShader(glsl_rchit_light, "rchit.light");
	rchitSun = CompileClosestHitShader(glsl_rchit_sun, "rchit.sun");
	rmissBounce = CompileMissShader(glsl_rmiss_bounce, "rmiss.bounce");
	rmissLight = CompileMissShader(glsl_rmiss_light, "rmiss.light");
	rmissSun = CompileMissShader(glsl_rmiss_sun, "rmiss.sun");
}

std::unique_ptr<VulkanShader> GPURaytracer::CompileRayGenShader(const char* code, const char* name)
{
	try
	{
		ShaderBuilder builder;
		builder.setRayGenShader(code);
		auto shader = builder.create(device.get());
		shader->SetDebugName(name);
		return shader;
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error(std::string("Could not compile ") + name + ": " + e.what());
	}
}

std::unique_ptr<VulkanShader> GPURaytracer::CompileClosestHitShader(const char* code, const char* name)
{
	try
	{
		ShaderBuilder builder;
		builder.setClosestHitShader(code);
		auto shader = builder.create(device.get());
		shader->SetDebugName(name);
		return shader;
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error(std::string("Could not compile ") + name + ": " + e.what());
	}
}

std::unique_ptr<VulkanShader> GPURaytracer::CompileMissShader(const char* code, const char* name)
{
	try
	{
		ShaderBuilder builder;
		builder.setMissShader(code);
		auto shader = builder.create(device.get());
		shader->SetDebugName(name);
		return shader;
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error(std::string("Could not compile ") + name + ": " + e.what());
	}
}

void GPURaytracer::CreatePipeline()
{
	DescriptorSetLayoutBuilder setbuilder;
	setbuilder.addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	setbuilder.addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	setbuilder.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	descriptorSetLayout = setbuilder.create(device.get());
	descriptorSetLayout->SetDebugName("descriptorSetLayout");

	PipelineLayoutBuilder layoutbuilder;
	layoutbuilder.addSetLayout(descriptorSetLayout.get());
	pipelineLayout = layoutbuilder.create(device.get());
	pipelineLayout->SetDebugName("pipelineLayout");

	RayTracingPipelineBuilder builder;
	builder.setLayout(pipelineLayout.get());
	builder.setMaxPipelineRayRecursionDepth(1);
	builder.addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenBounce.get());
	builder.addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenLight.get());
	builder.addShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissBounce.get());
	builder.addShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissLight.get());
	builder.addShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissSun.get());
	builder.addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitBounce.get());
	builder.addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitLight.get());
	builder.addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitSun.get());
	builder.addRayGenGroup(0);
	builder.addRayGenGroup(1);
	builder.addMissGroup(2);
	builder.addMissGroup(3);
	builder.addMissGroup(4);
	builder.addTrianglesHitGroup(5);
	builder.addTrianglesHitGroup(6);
	builder.addTrianglesHitGroup(7);
	pipeline = builder.create(device.get());
	pipeline->SetDebugName("pipeline");

	const auto& rtProperties = device->physicalDevice.rayTracingProperties;

	auto align_up = [](VkDeviceSize value, VkDeviceSize alignment)
	{
		if (alignment != 0)
			return (value + alignment - 1) / alignment * alignment;
		else
			return value;
	};

	VkDeviceSize raygenCount = 2;
	VkDeviceSize missCount = 3;
	VkDeviceSize hitCount = 3;

	VkDeviceSize handleSize = rtProperties.shaderGroupHandleSize;
	VkDeviceSize handleSizeAligned = align_up(handleSize, rtProperties.shaderGroupHandleAlignment);

	VkDeviceSize rgenStride = align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
	VkDeviceSize rgenSize = rgenStride * raygenCount;

	rgenBounceRegion.stride = rgenStride;
	rgenBounceRegion.size = rgenStride;
	rgenLightRegion.stride = rgenStride;
	rgenLightRegion.size = rgenStride;

	missRegion.stride = handleSizeAligned;
	missRegion.size = align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

	hitRegion.stride = handleSizeAligned;
	hitRegion.size = align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

	VkDeviceSize rgenOffset = 0;
	VkDeviceSize missOffset = rgenOffset + rgenSize;
	VkDeviceSize hitOffset = missOffset + missRegion.size;

	VkDeviceSize sbtBufferSize = rgenSize + missRegion.size + hitRegion.size;

	BufferBuilder bufbuilder;
	bufbuilder.setUsage(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	bufbuilder.setSize(sbtBufferSize);
	shaderBindingTable = bufbuilder.create(device.get());
	shaderBindingTable->SetDebugName("shaderBindingTable");

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(sbtBufferSize);
	sbtTransferBuffer = tbuilder.create(device.get());
	sbtTransferBuffer->SetDebugName("sbtTransferBuffer");
	uint8_t* src = (uint8_t*)pipeline->shaderGroupHandles.data();
	uint8_t* dest = (uint8_t*)sbtTransferBuffer->Map(0, sbtBufferSize);
	for (VkDeviceSize i = 0; i < raygenCount; i++)
	{
		memcpy(dest + rgenOffset + i * rgenStride, src, handleSize);
		src += handleSize;
	}
	for (VkDeviceSize i = 0; i < missCount; i++)
	{
		memcpy(dest + missOffset + i * missRegion.stride, src, handleSize);
		src += handleSize;
	}
	for (VkDeviceSize i = 0; i < hitCount; i++)
	{
		memcpy(dest + hitOffset + i * hitRegion.stride, src, handleSize);
		src += handleSize;
	}
	sbtTransferBuffer->Unmap();

	cmdbuffer->copyBuffer(sbtTransferBuffer.get(), shaderBindingTable.get());

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = shaderBindingTable->buffer;
	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device->device, &info);

	rgenBounceRegion.deviceAddress = sbtAddress + rgenOffset;
	rgenLightRegion.deviceAddress = sbtAddress + rgenOffset + rgenStride;
	missRegion.deviceAddress = sbtAddress + missOffset;
	hitRegion.deviceAddress = sbtAddress + hitOffset;
}

void GPURaytracer::CreateDescriptorSet()
{
	VkDeviceSize align = device->physicalDevice.properties.limits.minUniformBufferOffsetAlignment;
	uniformStructStride = (sizeof(Uniforms) + align - 1) / align * align;

	BufferBuilder uniformbuilder;
	uniformbuilder.setUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	uniformbuilder.setSize(uniformStructs * uniformStructStride);
	uniformBuffer = uniformbuilder.create(device.get());
	uniformBuffer->SetDebugName("uniformBuffer");

	BufferBuilder uniformtransferbuilder;
	uniformtransferbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uniformtransferbuilder.setSize(uniformStructs * uniformStructStride);
	uniformTransferBuffer = uniformtransferbuilder.create(device.get());
	uniformTransferBuffer->SetDebugName("uniformTransferBuffer");

	BufferBuilder itbuilder;
	itbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	itbuilder.setSize(2 * sizeof(vec4) * rayTraceImageSize * rayTraceImageSize);
	imageTransferBuffer = itbuilder.create(device.get());
	imageTransferBuffer->SetDebugName("imageTransferBuffer");

	ImageBuilder imgbuilder1;
	imgbuilder1.setUsage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imgbuilder1.setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
	imgbuilder1.setSize(rayTraceImageSize, rayTraceImageSize);
	startPositionsImage = imgbuilder1.create(device.get());
	startPositionsImage->SetDebugName("startPositionsImage");

	ImageBuilder imgbuilder2;
	imgbuilder2.setUsage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imgbuilder2.setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
	imgbuilder2.setSize(rayTraceImageSize, rayTraceImageSize);
	positionsImage = imgbuilder2.create(device.get());
	positionsImage->SetDebugName("positionsImage");

	ImageBuilder imgbuilder3;
	imgbuilder3.setUsage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	imgbuilder3.setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
	imgbuilder3.setSize(rayTraceImageSize, rayTraceImageSize);
	outputImage = imgbuilder3.create(device.get());
	outputImage->SetDebugName("outputImage");

	ImageViewBuilder viewbuilder1;
	viewbuilder1.setImage(startPositionsImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT);
	startPositionsImageView = viewbuilder1.create(device.get());
	startPositionsImageView->SetDebugName("startPositionsImageView");

	ImageViewBuilder viewbuilder2;
	viewbuilder2.setImage(positionsImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT);
	positionsImageView = viewbuilder2.create(device.get());
	positionsImageView->SetDebugName("positionsImageView");

	ImageViewBuilder viewbuilder3;
	viewbuilder3.setImage(outputImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT);
	outputImageView = viewbuilder3.create(device.get());
	outputImageView->SetDebugName("outputImageView");

	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3);
	poolbuilder.setMaxSets(1);
	descriptorPool = poolbuilder.create(device.get());
	descriptorPool->SetDebugName("descriptorPool");

	descriptorSet = descriptorPool->allocate(descriptorSetLayout.get());
	descriptorSet->SetDebugName("descriptorSet");

	WriteDescriptors write;
	write.addAccelerationStructure(descriptorSet.get(), 0, tlAccelStruct.get());
	write.addStorageImage(descriptorSet.get(), 1, startPositionsImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
	write.addStorageImage(descriptorSet.get(), 2, positionsImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
	write.addStorageImage(descriptorSet.get(), 3, outputImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
	write.addBuffer(descriptorSet.get(), 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, uniformBuffer.get(), 0, sizeof(Uniforms));
	write.addBuffer(descriptorSet.get(), 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceIndexBuffer.get());
	write.addBuffer(descriptorSet.get(), 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceBuffer.get());
	write.addBuffer(descriptorSet.get(), 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lightBuffer.get());
	write.updateSets(device.get());
}

void GPURaytracer::PrintVulkanInfo()
{
	const auto& props = device->physicalDevice.properties;

	std::string deviceType;
	switch (props.deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
	default: deviceType = std::to_string(props.deviceType); break;
	}

	std::string apiVersion = std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.apiVersion));
	std::string driverVersion = std::to_string(VK_VERSION_MAJOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.driverVersion));

	printf("Vulkan device: %s\n", props.deviceName);
	printf("Vulkan device type: %s\n", deviceType.c_str());
	printf("Vulkan version: %s (api) %s (driver)\n", apiVersion.c_str(), driverVersion.c_str());
}

vec2 GPURaytracer::Hammersley(uint32_t i, uint32_t N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

float GPURaytracer::RadicalInverse_VdC(uint32_t bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}
