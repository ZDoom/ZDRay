
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
#include <condition_variable>
#include <mutex>
#include <thread>
#include "glsl_frag.h"
#include "glsl_frag_resolve.h"
#include "glsl_vert.h"

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

	printf("Building Vulkan acceleration structures\n");

	if (device->renderdoc)
		device->renderdoc->StartFrameCapture(0, 0);

	CreateVulkanObjects();

	printf("Ray tracing in progress...\n");

	CreateAtlasImages();

	BeginCommands();
	UploadUniforms();

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		RenderAtlasImage(pageIndex);
	}

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		ResolveAtlasImage(pageIndex);
	}

#ifdef WIN32
	LARGE_INTEGER s;
	QueryPerformanceCounter(&s);
#endif

	FinishCommands();

#ifdef WIN32
	LARGE_INTEGER e, f;
	QueryPerformanceCounter(&e);
	QueryPerformanceFrequency(&f);
	printf("GPU ray tracing time was %.3f seconds.\n", double(e.QuadPart - s.QuadPart) / double(f.QuadPart));
#endif

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		DownloadAtlasImage(pageIndex);
	}

	if (device->renderdoc)
		device->renderdoc->EndFrameCapture(0, 0);

	printf("Ray trace complete\n");
}

void GPURaytracer::RenderAtlasImage(size_t pageIndex)
{
	LightmapImage& img = atlasImages[pageIndex];

	RenderPassBegin()
		.RenderPass(raytrace.renderPass.get())
		.RenderArea(0, 0, atlasImageSize, atlasImageSize)
		.Framebuffer(img.raytrace.Framebuffer.get())
		.Execute(cmdbuffer.get());

	VkDeviceSize offset = 0;
	cmdbuffer->bindVertexBuffers(0, 1, &sceneVertexBuffer->buffer, &offset);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipelineLayout.get(), 0, raytrace.descriptorSet.get());

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* targetSurface = mesh->surfaces[i].get();
		if (targetSurface->atlasPageIndex != pageIndex)
			continue;

		VkViewport viewport = {};
		viewport.maxDepth = 1;
		viewport.x = (float)targetSurface->atlasX - 1;
		viewport.y = (float)targetSurface->atlasY - 1;
		viewport.width = (float)(targetSurface->texWidth + 2);
		viewport.height = (float)(targetSurface->texHeight + 2);
		cmdbuffer->setViewport(0, 1, &viewport);

		// Paint all surfaces part of the smoothing group into the surface
		for (const auto& surface : mesh->surfaces)
		{
			if (surface->smoothingGroupIndex != targetSurface->smoothingGroupIndex)
				continue;

			vec2 minUV = ToUV(surface->bounds.min, targetSurface);
			vec2 maxUV = ToUV(surface->bounds.max, targetSurface);
			if (surface.get() != targetSurface && (maxUV.x < 0.0f || maxUV.y < 0.0f || minUV.x > 1.0f || minUV.y > 1.0f))
				continue; // Bounding box not visible

			int firstLight = sceneLightPos;
			int lightCount = (int)surface->LightList.size();
			if (sceneLightPos + lightCount > SceneLightBufferSize)
				throw std::runtime_error("SceneLightBuffer is too small!");
			sceneLightPos += lightCount;

			LightInfo* lightinfo = &sceneLights[firstLight];
			for (ThingLight* light : surface->LightList)
			{
				lightinfo->Origin = light->LightOrigin();
				lightinfo->Radius = light->LightRadius();
				lightinfo->Intensity = light->intensity;
				lightinfo->InnerAngleCos = light->innerAngleCos;
				lightinfo->OuterAngleCos = light->outerAngleCos;
				lightinfo->SpotDir = light->SpotDir();
				lightinfo->Color = light->rgb;
				lightinfo++;
			}

			PushConstants pc;
			pc.LightStart = firstLight;
			pc.LightEnd = firstLight + lightCount;
			pc.SurfaceIndex = (int32_t)i;
			pc.LightmapOrigin = targetSurface->worldOrigin - targetSurface->worldStepX - targetSurface->worldStepY;
			pc.LightmapStepX = targetSurface->worldStepX * viewport.width;
			pc.LightmapStepY = targetSurface->worldStepY * viewport.height;
			cmdbuffer->pushConstants(raytrace.pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

			int firstVertex = sceneVertexPos;
			int vertexCount = (int)surface->verts.size();
			if (sceneVertexPos + vertexCount > SceneVertexBufferSize)
				throw std::runtime_error("SceneVertexBuffer is too small!");
			sceneVertexPos += vertexCount;
			SceneVertex* vertex = &sceneVertices[firstVertex];

			if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
			{
				for (int idx = 0; idx < vertexCount; idx++)
				{
					(vertex++)->Position = ToUV(surface->verts[idx], targetSurface);
				}
			}
			else
			{
				(vertex++)->Position = ToUV(surface->verts[0], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[2], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[3], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[1], targetSurface);
			}

			cmdbuffer->draw(vertexCount, 1, firstVertex, 0);
		}
	}

	cmdbuffer->endRenderPass();
}

void GPURaytracer::CreateAtlasImages()
{
	const int spacing = 3; // Note: the spacing is here to avoid that the resolve sampler finds data from other surface tiles
	RectPacker packer(atlasImageSize, atlasImageSize, RectPacker::Spacing(spacing));

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();

		auto result = packer.insert(surface->texWidth + 2, surface->texHeight + 2);
		surface->atlasX = result.pos.x + 1;
		surface->atlasY = result.pos.y + 1;
		surface->atlasPageIndex = (int)result.pageIndex;
	}

	for (size_t pageIndex = 0; pageIndex < packer.getNumPages(); pageIndex++)
	{
		atlasImages.push_back(CreateImage(atlasImageSize, atlasImageSize));
	}
}

void GPURaytracer::UploadUniforms()
{
	Uniforms uniforms = {};
	uniforms.SunDir = mesh->map->GetSunDirection();
	uniforms.SunColor = mesh->map->GetSunColor();
	uniforms.SunIntensity = 1.0f;

	mappedUniforms = (uint8_t*)uniformTransferBuffer->Map(0, uniformStructs * uniformStructStride);
	*reinterpret_cast<Uniforms*>(mappedUniforms + uniformStructStride * uniformsIndex) = uniforms;
	uniformTransferBuffer->Unmap();

	cmdbuffer->copyBuffer(uniformTransferBuffer.get(), uniformBuffer.get());
	PipelineBarrier()
		.AddBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void GPURaytracer::ResolveAtlasImage(size_t i)
{
	LightmapImage& img = atlasImages[i];

	PipelineBarrier()
		.AddImage(img.raytrace.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	RenderPassBegin()
		.RenderPass(resolve.renderPass.get())
		.RenderArea(0, 0, atlasImageSize, atlasImageSize)
		.Framebuffer(img.resolve.Framebuffer.get())
		.Execute(cmdbuffer.get());

	VkDeviceSize offset = 0;
	cmdbuffer->bindVertexBuffers(0, 1, &sceneVertexBuffer->buffer, &offset);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipeline.get());

	auto descriptorSet = resolve.descriptorPool->allocate(resolve.descriptorSetLayout.get());
	descriptorSet->SetDebugName("resolve.descriptorSet");
	WriteDescriptors()
		.AddCombinedImageSampler(descriptorSet.get(), 0, img.raytrace.View.get(), resolve.sampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.Execute(device.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipelineLayout.get(), 0, descriptorSet.get());
	resolve.descriptorSets.push_back(std::move(descriptorSet));

	VkViewport viewport = {};
	viewport.maxDepth = 1;
	viewport.width = (float)atlasImageSize;
	viewport.height = (float)atlasImageSize;
	cmdbuffer->setViewport(0, 1, &viewport);

	PushConstants pc;
	pc.LightStart = 0;
	pc.LightEnd = 0;
	pc.SurfaceIndex = 0;
	pc.LightmapOrigin = vec3(0.0f);
	pc.LightmapStepX = vec3(0.0f);
	pc.LightmapStepY = vec3(0.0f);
	cmdbuffer->pushConstants(resolve.pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

	int firstVertex = sceneVertexPos;
	int vertexCount = 4;
	sceneVertexPos += vertexCount;
	SceneVertex* vertex = &sceneVertices[firstVertex];
	vertex[0].Position = vec2(0.0f, 0.0f);
	vertex[1].Position = vec2(1.0f, 0.0f);
	vertex[2].Position = vec2(1.0f, 1.0f);
	vertex[3].Position = vec2(0.0f, 1.0f);
	cmdbuffer->draw(vertexCount, 1, firstVertex, 0);

	cmdbuffer->endRenderPass();

	PipelineBarrier()
		.AddImage(img.resolve.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = atlasImageSize;
	region.imageExtent.height = atlasImageSize;
	region.imageExtent.depth = 1;
	cmdbuffer->copyImageToBuffer(img.resolve.Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img.Transfer->buffer, 1, &region);
}

void GPURaytracer::DownloadAtlasImage(size_t pageIndex)
{
	struct hvec4
	{
		unsigned short x, y, z, w;
		vec3 xyz() { return vec3(halfToFloat(x), halfToFloat(y), halfToFloat(z)); }
	};

	hvec4* pixels = (hvec4*)atlasImages[pageIndex].Transfer->Map(0, atlasImageSize * atlasImageSize * sizeof(hvec4));

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();
		if (surface->atlasPageIndex != pageIndex)
			continue;

		int atlasX = surface->atlasX;
		int atlasY = surface->atlasY;
		int sampleWidth = surface->texWidth;
		int sampleHeight = surface->texHeight;

		for (int y = 0; y < sampleHeight; y++)
		{
			vec3* dest = &surface->texPixels[y * sampleWidth];
			hvec4* src = &pixels[atlasX + (atlasY + y) * atlasImageSize];
			for (int x = 0; x < sampleWidth; x++)
			{
				dest[x] = src[x].xyz();
			}
		}
	}
	atlasImages[pageIndex].Transfer->Unmap();
}

vec2 GPURaytracer::ToUV(const vec3& vert, const Surface* targetSurface)
{
	vec3 localPos = vert - targetSurface->translateWorldToLocal;
	float u = (1.0f + dot(localPos, targetSurface->projLocalToU)) / (targetSurface->texWidth + 2);
	float v = (1.0f + dot(localPos, targetSurface->projLocalToV)) / (targetSurface->texHeight + 2);
	return vec2(u, v);
}

void GPURaytracer::CreateVulkanObjects()
{
	submitFence = std::make_unique<VulkanFence>(device.get());
	cmdpool = std::make_unique<VulkanCommandPool>(device.get(), device->graphicsFamily);

	BeginCommands();

	CreateSceneVertexBuffer();
	CreateSceneLightBuffer();
	CreateVertexAndIndexBuffers();
	CreateUniformBuffer();
	CreateBottomLevelAccelerationStructure();
	CreateTopLevelAccelerationStructure();
	CreateShaders();
	CreateRaytracePipeline();
	CreateResolvePipeline();

	FinishCommands();
}

void GPURaytracer::CreateSceneVertexBuffer()
{
	size_t size = sizeof(SceneVertex) * SceneVertexBufferSize;

	sceneVertexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("SceneVertexBuffer")
		.Create(device.get());

	sceneVertices = (SceneVertex*)sceneVertexBuffer->Map(0, size);
	sceneVertexPos = 0;
}

void GPURaytracer::CreateSceneLightBuffer()
{
	size_t size = sizeof(LightInfo) * SceneLightBufferSize;

	sceneLightBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("SceneLightBuffer")
		.Create(device.get());

	sceneLights = (LightInfo*)sceneLightBuffer->Map(0, size);
	sceneLightPos = 0;
}

void GPURaytracer::BeginCommands()
{
	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();
}

void GPURaytracer::FinishCommands()
{
	cmdbuffer->end();

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

void GPURaytracer::CreateVertexAndIndexBuffers()
{
	std::vector<SurfaceInfo> surfaces = CreateSurfaceInfo();

	if (surfaces.empty()) // vulkan doesn't support zero byte buffers
		surfaces.push_back(SurfaceInfo());

	size_t vertexbuffersize = (size_t)mesh->MeshVertices.Size() * sizeof(vec3);
	size_t indexbuffersize = (size_t)mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t surfaceindexbuffersize = (size_t)mesh->MeshSurfaces.Size() * sizeof(uint32_t);
	size_t surfacebuffersize = (size_t)surfaces.size() * sizeof(SurfaceInfo);
	size_t transferbuffersize = vertexbuffersize + indexbuffersize + surfaceindexbuffersize + surfacebuffersize;
	size_t vertexoffset = 0;
	size_t indexoffset = vertexoffset + vertexbuffersize;
	size_t surfaceindexoffset = indexoffset + indexbuffersize;
	size_t surfaceoffset = surfaceindexoffset + surfaceindexbuffersize;
	size_t lightoffset = surfaceoffset + surfacebuffersize;

	vertexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(vertexbuffersize)
		.DebugName("vertexBuffer")
		.Create(device.get());

	indexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
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
	transferBuffer->Unmap();

	cmdbuffer->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), surfaceIndexBuffer.get(), surfaceindexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), surfaceBuffer.get(), surfaceoffset);

	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
}

void GPURaytracer::CreateBottomLevelAccelerationStructure()
{
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructBLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	accelStructBLDesc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelStructBLDesc.geometry.triangles.vertexData.deviceAddress = vertexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.vertexStride = sizeof(vec3);
	accelStructBLDesc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelStructBLDesc.geometry.triangles.indexData.deviceAddress = indexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.maxVertex = mesh->MeshVertices.Size() - 1;

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructBLDesc;

	uint32_t maxPrimitiveCount = mesh->MeshElements.Size() / 3;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	blAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("blAccelStructBuffer")
		.Create(device.get());

	blAccelStruct = AccelerationStructureBuilder()
		.Type(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
		.Buffer(blAccelStructBuffer.get(), sizeInfo.accelerationStructureSize)
		.Create(device.get());

	blScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("blScratchBuffer")
		.Create(device.get());

	buildInfo.dstAccelerationStructure = blAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = blScratchBuffer->GetDeviceAddress();
	rangeInfo.primitiveCount = maxPrimitiveCount;

	cmdbuffer->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building before using it as input to a toplevel accel structure
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
}

void GPURaytracer::CreateTopLevelAccelerationStructure()
{
	VkAccelerationStructureInstanceKHR instance = {};
	instance.transform.matrix[0][0] = 1.0f;
	instance.transform.matrix[1][1] = 1.0f;
	instance.transform.matrix[2][2] = 1.0f;
	instance.mask = 0xff;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = blAccelStruct->GetDeviceAddress();

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

	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

	VkAccelerationStructureGeometryKHR accelStructTLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	accelStructTLDesc.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelStructTLDesc.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	accelStructTLDesc.geometry.instances.data.deviceAddress = tlInstanceBuffer->GetDeviceAddress();

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructTLDesc;

	uint32_t maxInstanceCount = 1;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxInstanceCount, &sizeInfo);

	tlAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("tlAccelStructBuffer")
		.Create(device.get());

	tlAccelStruct = AccelerationStructureBuilder()
		.Type(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
		.Buffer(tlAccelStructBuffer.get(), sizeInfo.accelerationStructureSize)
		.DebugName("tlAccelStruct")
		.Create(device.get());

	tlScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("tlScratchBuffer")
		.Create(device.get());

	buildInfo.dstAccelerationStructure = tlAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = tlScratchBuffer->GetDeviceAddress();
	rangeInfo.primitiveCount = maxInstanceCount;

	cmdbuffer->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void GPURaytracer::CreateShaders()
{
	vertShader = ShaderBuilder()
		.VertexShader(glsl_vert)
		.DebugName("vertShader")
		.Create("vertShader", device.get());

	fragShader = ShaderBuilder()
		.FragmentShader(glsl_frag)
		.DebugName("fragShader")
		.Create("fragShader", device.get());

	fragResolveShader = ShaderBuilder()
		.FragmentShader(glsl_frag_resolve)
		.DebugName("fragResolveShader")
		.Create("fragResolveShader", device.get());
}

void GPURaytracer::CreateRaytracePipeline()
{
	raytrace.descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("raytrace.descriptorSetLayout")
		.Create(device.get());

	raytrace.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(raytrace.descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants))
		.DebugName("raytrace.pipelineLayout")
		.Create(device.get());

	raytrace.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_4_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.DebugName("raytrace.renderpass")
		.Create(device.get());

	raytrace.pipeline = GraphicsPipelineBuilder()
		.Layout(raytrace.pipelineLayout.get())
		.RenderPass(raytrace.renderPass.get())
		.AddVertexShader(vertShader.get())
		.AddFragmentShader(fragShader.get())
		.AddVertexBufferBinding(0, sizeof(SceneVertex))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, Position))
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.RasterizationSamples(VK_SAMPLE_COUNT_4_BIT)
		.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
		.Scissor(0, 0, 4096, 4096)
		.DebugName("raytrace.pipeline")
		.Create(device.get());

	raytrace.descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
		.MaxSets(1)
		.DebugName("raytrace.descriptorPool")
		.Create(device.get());

	raytrace.descriptorSet = raytrace.descriptorPool->allocate(raytrace.descriptorSetLayout.get());
	raytrace.descriptorSet->SetDebugName("raytrace.descriptorSet");

	WriteDescriptors()
		.AddAccelerationStructure(raytrace.descriptorSet.get(), 0, tlAccelStruct.get())
		.AddBuffer(raytrace.descriptorSet.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffer.get(), 0, sizeof(Uniforms))
		.AddBuffer(raytrace.descriptorSet.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceIndexBuffer.get())
		.AddBuffer(raytrace.descriptorSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceBuffer.get())
		.AddBuffer(raytrace.descriptorSet.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sceneLightBuffer.get())
		.Execute(device.get());
}

void GPURaytracer::CreateResolvePipeline()
{
	resolve.descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("resolve.descriptorSetLayout")
		.Create(device.get());

	resolve.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(resolve.descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants))
		.DebugName("resolve.pipelineLayout")
		.Create(device.get());

	resolve.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.DebugName("resolve.renderpass")
		.Create(device.get());

	resolve.pipeline = GraphicsPipelineBuilder()
		.Layout(resolve.pipelineLayout.get())
		.RenderPass(resolve.renderPass.get())
		.AddVertexShader(vertShader.get())
		.AddFragmentShader(fragResolveShader.get())
		.AddVertexBufferBinding(0, sizeof(SceneVertex))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, Position))
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
		.Scissor(0, 0, 4096, 4096)
		.DebugName("resolve.pipeline")
		.Create(device.get());

	resolve.descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256)
		.MaxSets(256)
		.DebugName("resolve.descriptorPool")
		.Create(device.get());

	resolve.sampler = SamplerBuilder()
		.DebugName("resolve.Sampler")
		.Create(device.get());
}

LightmapImage GPURaytracer::CreateImage(int width, int height)
{
	LightmapImage img;

	img.raytrace.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.Samples(VK_SAMPLE_COUNT_4_BIT)
		.DebugName("LightmapImage.raytrace.Image")
		.Create(device.get());

	img.raytrace.View = ImageViewBuilder()
		.Image(img.raytrace.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.raytrace.View")
		.Create(device.get());

	img.raytrace.Framebuffer = FramebufferBuilder()
		.RenderPass(raytrace.renderPass.get())
		.Size(width, height)
		.AddAttachment(img.raytrace.View.get())
		.DebugName("LightmapImage.raytrace.Framebuffer")
		.Create(device.get());

	img.resolve.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.DebugName("LightmapImage.resolve.Image")
		.Create(device.get());

	img.resolve.View = ImageViewBuilder()
		.Image(img.resolve.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.resolve.View")
		.Create(device.get());

	img.resolve.Framebuffer = FramebufferBuilder()
		.RenderPass(resolve.renderPass.get())
		.Size(width, height)
		.AddAttachment(img.resolve.View.get())
		.DebugName("LightmapImage.resolve.Framebuffer")
		.Create(device.get());

	img.Transfer = BufferBuilder()
		.Size(width * height * sizeof(vec4))
		.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("LightmapImage.Transfer")
		.Create(device.get());

	return img;
}

void GPURaytracer::CreateUniformBuffer()
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
}

std::vector<SurfaceInfo> GPURaytracer::CreateSurfaceInfo()
{
	std::vector<SurfaceInfo> surfaces;
	surfaces.reserve(mesh->surfaces.size());
	for (const auto& surface : mesh->surfaces)
	{
		SurfaceInfo info;
		info.Sky = surface->bSky ? 1.0f : 0.0f;
		info.Normal = surface->plane.Normal();
		info.SamplingDistance = float(surface->sampleDimension);
		surfaces.push_back(info);
	}
	return surfaces;
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
