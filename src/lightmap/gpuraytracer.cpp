
#include "math/mathlib.h"
#include "levelmesh.h"
#include "level/level.h"
#include "gpuraytracer.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include "vulkanbuilders.h"
#include "surfaceclip.h"
#include <map>
#include <vector>
#include <algorithm>
#include <limits>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "glsl_rgen_bounce.h"
#include "glsl_rgen_light.h"
#include "glsl_rgen_ambient.h"
#include "glsl_rchit_bounce.h"
#include "glsl_rchit_light.h"
#include "glsl_rchit_sun.h"
#include "glsl_rchit_ambient.h"
#include "glsl_rmiss_bounce.h"
#include "glsl_rmiss_light.h"
#include "glsl_rmiss_sun.h"
#include "glsl_rmiss_ambient.h"

extern bool VKDebug;

extern int coverageSampleCount;
extern int bounceSampleCount;
extern int ambientSampleCount;

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

	printf("Building Vulkan acceleration structures\n");

	if (device->renderdoc)
		device->renderdoc->StartFrameCapture(0, 0);

	CreateVulkanObjects();

	std::vector<TraceTask> tasks;
	CreateTasks(tasks);

	std::vector<vec3> HemisphereVectors;
	HemisphereVectors.reserve(bounceSampleCount);
	for (int i = 0; i < bounceSampleCount; i++)
	{
		vec2 Xi = Hammersley(i, bounceSampleCount);
		vec3 H;
		H.x = Xi.x * 2.0f - 1.0f;
		H.y = Xi.y * 2.0f - 1.0f;
		H.z = 1.5f - length(Xi);
		H = normalize(H);
		HemisphereVectors.push_back(H);
	}

	//printf("Ray tracing with %d bounce(s)\n", mesh->map->LightBounce);
	printf("Ray tracing in progress...\n");

	size_t maxTasks = (size_t)rayTraceImageSize * rayTraceImageSize;
	for (size_t startTask = 0; startTask < tasks.size(); startTask += maxTasks)
	{
		printf("\r%.1f%%\t%llu/%llu", double(startTask) / double(tasks.size()) * 100, startTask, tasks.size());
		size_t numTasks = std::min(tasks.size() - startTask, maxTasks);
		UploadTasks(tasks.data() + startTask, numTasks);

		BeginTracing();

		Uniforms uniforms = {};
		uniforms.SunDir = mesh->map->GetSunDirection();
		uniforms.SunColor = mesh->map->GetSunColor();
		uniforms.SunIntensity = 1.0f;

		uniforms.PassType = 0;
		uniforms.SampleIndex = 0;
		uniforms.SampleCount = bounceSampleCount;
		RunTrace(uniforms, rgenBounceRegion);

		uniforms.SampleCount = coverageSampleCount;
		RunTrace(uniforms, rgenLightRegion, 0, mesh->map->ThingLights.Size());

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
				RunTrace(uniforms, rgenLightRegion, 0, mesh->map->ThingLights.Size());

				uniforms.PassType = 2;
				uniforms.SampleIndex = (i + bounce) % uniforms.SampleCount;
				uniforms.SampleCount = bounceSampleCount;
				uniforms.HemisphereVec = HemisphereVectors[uniforms.SampleIndex];
				RunTrace(uniforms, rgenBounceRegion);
			}
		}

		uniforms.PassType = 0;
		uniforms.SampleIndex = 0;
		uniforms.SampleCount = ambientSampleCount;
		RunTrace(uniforms, rgenAmbientRegion);

		EndTracing();
		DownloadTasks(tasks.data() + startTask, numTasks);
	}
	printf("\r%.1f%%\t%llu/%llu\n", 100.0, tasks.size(), tasks.size());

	if (device->renderdoc)
		device->renderdoc->EndFrameCapture(0, 0);

	printf("Ray trace complete\n");
}

void GPURaytracer::CreateTasks(std::vector<TraceTask>& tasks)
{
	tasks.reserve(mesh->lightProbes.size());

	for (size_t i = 0; i < mesh->lightProbes.size(); i++)
	{
		TraceTask task;
		task.id = -(int)(i + 2);
		task.x = 0;
		task.y = 0;
		tasks.push_back(task);
	}

	size_t fullTaskCount = mesh->lightProbes.size();

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		if (i % 4096 == 0)
			printf("\rGathering surface trace tasks: %llu / %llu", i, mesh->surfaces.size());

		Surface* surface = mesh->surfaces[i].get();

		if (!surface->bSky)
		{
			int sampleWidth = surface->texWidth;
			int sampleHeight = surface->texHeight;

			fullTaskCount += size_t(sampleHeight) * size_t(sampleWidth);

			SurfaceClip surfaceClip(surface);

			for (int y = 0; y < sampleHeight; y++)
			{
				for (int x = 0; x < sampleWidth; x++)
				{
					if (surfaceClip.SampleIsInBounds(float(x), float(y)))
					{
						TraceTask task;
						task.id = (int)i;
						task.x = x;
						task.y = y;
						tasks.push_back(task);
					}
				}
			}
		}
	}
	printf("\rGathering surface trace tasks: %llu / %llu\n", mesh->surfaces.size(), mesh->surfaces.size());
	printf("\tDiscarded %.3f%% of all tasks\n", (1.0 - double(tasks.size()) / fullTaskCount) * 100.0);
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

	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
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
			vec3 pos = surface->worldOrigin + surface->worldStepX * (task.x + 0.5f) + surface->worldStepY * (task.y + 0.5f);
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

	PipelineBarrier()
		.AddImage(startPositionsImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.imageExtent.width = rayTraceImageSize;
	region.imageExtent.height = rayTraceImageSize;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyBufferToImage(imageTransferBuffer->buffer, startPositionsImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	PipelineBarrier()
		.AddBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.AddImage(startPositionsImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.AddImage(positionsImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
		.AddImage(outputImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}

void GPURaytracer::BeginTracing()
{
	uniformsIndex = 0;
	mappedUniforms = (uint8_t*)uniformTransferBuffer->Map(0, uniformStructs * uniformStructStride);

	cmdbuffer->copyBuffer(uniformTransferBuffer.get(), uniformBuffer.get());
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
}

void GPURaytracer::RunTrace(const Uniforms& uniforms, const VkStridedDeviceAddressRegionKHR& rgenShader, int lightStart, int lightEnd)
{
	if (uniformsIndex == uniformStructs)
	{
		EndTracing();
		BeginTracing();
	}

	*reinterpret_cast<Uniforms*>(mappedUniforms + uniformStructStride * uniformsIndex) = uniforms;

	uint32_t offset = (uint32_t)(uniformsIndex * uniformStructStride);
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0, descriptorSet.get(), 1, &offset);

	bool needbarrier = true;
	if (uniformsIndex == 0)
	{
		PipelineBarrier()
			.AddBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
			.AddImage(positionsImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
			.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		needbarrier = false;
	}
	uniformsIndex++;

	const int maxLights = 50;

	do
	{
		int count = std::min(lightEnd - lightStart, maxLights);

		if (needbarrier)
		{
			PipelineBarrier()
				.AddImage(positionsImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
				.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		}

		PushConstants constants = {};
		constants.LightStart = lightStart;
		constants.LightEnd = lightStart + count;
		cmdbuffer->pushConstants(pipelineLayout.get(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, (uint32_t)sizeof(PushConstants), &constants);
		cmdbuffer->traceRays(&rgenShader, &missRegion, &hitRegion, &callRegion, rayTraceImageSize, rayTraceImageSize, 1);

		needbarrier = true;
		lightStart += count;
	} while (lightStart < lightEnd);
}

void GPURaytracer::EndTracing()
{
	mappedUniforms = nullptr;
	uniformTransferBuffer->Unmap();
	cmdbuffer->end();

	SubmitCommands();

	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();
}

void GPURaytracer::SubmitCommands()
{
	auto submitFence = std::make_unique<VulkanFence>(device.get());

	QueueSubmit()
		.AddCommandBuffer(cmdbuffer.get())
		.Execute(device.get(), device->graphicsQueue, submitFence.get());

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
	PipelineBarrier()
		.AddImage(outputImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);

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
			size_t sampleWidth = surface->texWidth;
			surface->texPixels[task.x + task.y * sampleWidth] = vec3(output[i].x, output[i].y, output[i].z);
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

			if (sector && surface->verts.size() > 0)
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

		info.SamplingDistance = float(surface->sampleDimension);
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

	vertexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(vertexbuffersize)
		.DebugName("vertexBuffer")
		.Create(device.get());

	indexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(indexbuffersize)
		.DebugName("indexBuffer")
		.Create(device.get());

	surfaceIndexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(surfaceindexbuffersize)
		.DebugName("surfaceIndexBuffer")
		.Create(device.get());

	surfaceBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(surfacebuffersize)
		.DebugName("surfaceBuffer")
		.Create(device.get());

	lightBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(lightbuffersize)
		.DebugName("lightBuffer")
		.Create(device.get());

	transferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(transferbuffersize)
		.DebugName("transferBuffer")
		.Create(device.get());

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

	blAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("blAccelStructBuffer")
		.Create(device.get());

	VkAccelerationStructureKHR blAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.buffer = blAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &blAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	blAccelStruct = std::make_unique<VulkanAccelerationStructure>(device.get(), blAccelStructHandle);

	blScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("blScratchBuffer")
		.Create(device.get());

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

	tlTransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(sizeof(VkAccelerationStructureInstanceKHR))
		.DebugName("tlTransferBuffer")
		.Create(device.get());

	auto data = (uint8_t*)tlTransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR));
	memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer->Unmap();

	tlInstanceBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(sizeof(VkAccelerationStructureInstanceKHR))
		.DebugName("tlInstanceBuffer")
		.Create(device.get());

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

	tlAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("tlAccelStructBuffer")
		.Create(device.get());

	VkAccelerationStructureKHR tlAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.buffer = tlAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &tlAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	tlAccelStruct = std::make_unique<VulkanAccelerationStructure>(device.get(), tlAccelStructHandle);

	tlScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("tlScratchBuffer")
		.Create(device.get());

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
	rgenAmbient = CompileRayGenShader(glsl_rgen_ambient, "rgen.ambient");
	rchitBounce = CompileClosestHitShader(glsl_rchit_bounce, "rchit.bounce");
	rchitLight = CompileClosestHitShader(glsl_rchit_light, "rchit.light");
	rchitSun = CompileClosestHitShader(glsl_rchit_sun, "rchit.sun");
	rchitAmbient = CompileClosestHitShader(glsl_rchit_ambient, "rchit.ambient");
	rmissBounce = CompileMissShader(glsl_rmiss_bounce, "rmiss.bounce");
	rmissLight = CompileMissShader(glsl_rmiss_light, "rmiss.light");
	rmissSun = CompileMissShader(glsl_rmiss_sun, "rmiss.sun");
	rmissAmbient = CompileMissShader(glsl_rmiss_ambient, "rmiss.ambient");
}

std::unique_ptr<VulkanShader> GPURaytracer::CompileRayGenShader(const char* code, const char* name)
{
	try
	{
		return ShaderBuilder()
			.RayGenShader(code)
			.DebugName(name)
			.Create(name, device.get());
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
		return ShaderBuilder()
			.ClosestHitShader(code)
			.DebugName(name)
			.Create(name, device.get());
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
		return ShaderBuilder()
			.MissShader(code)
			.DebugName(name)
			.Create(name, device.get());
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error(std::string("Could not compile ") + name + ": " + e.what());
	}
}

void GPURaytracer::CreatePipeline()
{
	descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.AddBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.AddBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.DebugName("descriptorSetLayout")
		.Create(device.get());

	pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(PushConstants))
		.DebugName("pipelineLayout")
		.Create(device.get());

	pipeline = RayTracingPipelineBuilder()
		.Layout(pipelineLayout.get())
		.MaxPipelineRayRecursionDepth(1)
		.AddShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenBounce.get())
		.AddShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenLight.get())
		.AddShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenAmbient.get())
		.AddShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissBounce.get())
		.AddShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissLight.get())
		.AddShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissSun.get())
		.AddShader(VK_SHADER_STAGE_MISS_BIT_KHR, rmissAmbient.get())
		.AddShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitBounce.get())
		.AddShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitLight.get())
		.AddShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitSun.get())
		.AddShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchitAmbient.get())
		.AddRayGenGroup(0)
		.AddRayGenGroup(1)
		.AddRayGenGroup(2)
		.AddMissGroup(3)
		.AddMissGroup(4)
		.AddMissGroup(5)
		.AddMissGroup(6)
		.AddTrianglesHitGroup(7)
		.AddTrianglesHitGroup(8)
		.AddTrianglesHitGroup(9)
		.AddTrianglesHitGroup(10)
		.DebugName("pipeline")
		.Create(device.get());

	const auto& rtProperties = device->physicalDevice.rayTracingProperties;

	auto align_up = [](VkDeviceSize value, VkDeviceSize alignment)
	{
		if (alignment != 0)
			return (value + alignment - 1) / alignment * alignment;
		else
			return value;
	};

	VkDeviceSize raygenCount = 3;
	VkDeviceSize missCount = 4;
	VkDeviceSize hitCount = 4;

	VkDeviceSize handleSize = rtProperties.shaderGroupHandleSize;
	VkDeviceSize handleSizeAligned = align_up(handleSize, rtProperties.shaderGroupHandleAlignment);

	VkDeviceSize rgenStride = align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
	VkDeviceSize rgenSize = rgenStride * raygenCount;

	missRegion.stride = handleSizeAligned;
	missRegion.size = align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

	hitRegion.stride = handleSizeAligned;
	hitRegion.size = align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

	VkDeviceSize rgenOffset = 0;
	VkDeviceSize missOffset = rgenOffset + rgenSize;
	VkDeviceSize hitOffset = missOffset + missRegion.size;

	VkDeviceSize sbtBufferSize = rgenSize + missRegion.size + hitRegion.size;

	shaderBindingTable = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(sbtBufferSize)
		.DebugName("shaderBindingTable")
		.Create(device.get());

	sbtTransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(sbtBufferSize)
		.DebugName("sbtTransferBuffer")
		.Create(device.get());

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

	int i = 0;
	for (VkStridedDeviceAddressRegionKHR* region : { &rgenBounceRegion, &rgenLightRegion, &rgenAmbientRegion })
	{
		region->stride = rgenStride;
		region->size = rgenStride;
		region->deviceAddress = sbtAddress + rgenOffset + rgenStride * i;
		i++;
	}
	missRegion.deviceAddress = sbtAddress + missOffset;
	hitRegion.deviceAddress = sbtAddress + hitOffset;
}

void GPURaytracer::CreateDescriptorSet()
{
	VkDeviceSize align = device->physicalDevice.properties.limits.minUniformBufferOffsetAlignment;
	uniformStructStride = (sizeof(Uniforms) + align - 1) / align * align;

	uniformBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(uniformStructs * uniformStructStride)
		.DebugName("uniformBuffer")
		.Create(device.get());

	uniformTransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)
		.Size(uniformStructs * uniformStructStride)
		.DebugName("uniformTransferBuffer")
		.Create(device.get());

	imageTransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(2 * sizeof(vec4) * rayTraceImageSize * rayTraceImageSize)
		.DebugName("imageTransferBuffer")
		.Create(device.get());

	startPositionsImage = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.Format(VK_FORMAT_R32G32B32A32_SFLOAT)
		.Size(rayTraceImageSize, rayTraceImageSize)
		.DebugName("startPositionsImage")
		.Create(device.get());

	positionsImage = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.Format(VK_FORMAT_R32G32B32A32_SFLOAT)
		.Size(rayTraceImageSize, rayTraceImageSize)
		.DebugName("positionsImage")
		.Create(device.get());

	outputImage = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.Format(VK_FORMAT_R32G32B32A32_SFLOAT)
		.Size(rayTraceImageSize, rayTraceImageSize)
		.DebugName("outputImage")
		.Create(device.get());

	startPositionsImageView = ImageViewBuilder()
		.Image(startPositionsImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT)
		.DebugName("startPositionsImageView")
		.Create(device.get());

	positionsImageView = ImageViewBuilder()
		.Image(positionsImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT)
		.DebugName("positionsImageView")
		.Create(device.get());

	outputImageView = ImageViewBuilder()
		.Image(outputImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT)
		.DebugName("outputImageView")
		.Create(device.get());

	descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
		.MaxSets(1)
		.DebugName("descriptorPool")
		.Create(device.get());

	descriptorSet = descriptorPool->allocate(descriptorSetLayout.get());
	descriptorSet->SetDebugName("descriptorSet");

	WriteDescriptors()
		.AddAccelerationStructure(descriptorSet.get(), 0, tlAccelStruct.get())
		.AddStorageImage(descriptorSet.get(), 1, startPositionsImageView.get(), VK_IMAGE_LAYOUT_GENERAL)
		.AddStorageImage(descriptorSet.get(), 2, positionsImageView.get(), VK_IMAGE_LAYOUT_GENERAL)
		.AddStorageImage(descriptorSet.get(), 3, outputImageView.get(), VK_IMAGE_LAYOUT_GENERAL)
		.AddBuffer(descriptorSet.get(), 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, uniformBuffer.get(), 0, sizeof(Uniforms))
		.AddBuffer(descriptorSet.get(), 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceIndexBuffer.get())
		.AddBuffer(descriptorSet.get(), 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceBuffer.get())
		.AddBuffer(descriptorSet.get(), 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lightBuffer.get())
		.Execute(device.get());
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
