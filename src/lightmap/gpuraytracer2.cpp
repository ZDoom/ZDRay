
#include "math/mathlib.h"
#include "levelmesh.h"
#include "level/level.h"
#include "gpuraytracer2.h"
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
#include "glsl_vert.h"

extern bool VKDebug;

GPURaytracer2::GPURaytracer2()
{
	device = std::make_unique<VulkanDevice>(0, VKDebug);
	PrintVulkanInfo();
}

GPURaytracer2::~GPURaytracer2()
{
}

void GPURaytracer2::Raytrace(LevelMesh* level)
{
	mesh = level;

	printf("Building Vulkan acceleration structures\n");

	if (device->renderdoc)
		device->renderdoc->StartFrameCapture(0, 0);

	CreateVulkanObjects();

	printf("Ray tracing in progress...\n");

	BeginCommands();

	Uniforms2 uniforms = {};
	uniforms.SunDir = mesh->map->GetSunDirection();
	uniforms.SunColor = mesh->map->GetSunColor();
	uniforms.SunIntensity = 1.0f;

	mappedUniforms = (uint8_t*)uniformTransferBuffer->Map(0, uniformStructs * uniformStructStride);
	*reinterpret_cast<Uniforms2*>(mappedUniforms + uniformStructStride * uniformsIndex) = uniforms;
	uniformTransferBuffer->Unmap();

	cmdbuffer->copyBuffer(uniformTransferBuffer.get(), uniformBuffer.get());
	PipelineBarrier()
		.AddBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	const int atlasImageSize = 512;
	RectPacker packer(atlasImageSize, atlasImageSize, RectPacker::Spacing(0));

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();

		auto result = packer.insert(surface->texWidth, surface->texHeight);
		surface->atlasX = result.pos.x;
		surface->atlasY = result.pos.y;
		surface->atlasPageIndex = (int)result.pageIndex;
	}

	std::vector<LightmapImage> atlasImages;
	for (size_t pageIndex = 0; pageIndex < packer.getNumPages(); pageIndex++)
	{
		atlasImages.push_back(CreateImage(atlasImageSize, atlasImageSize));
	}

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		LightmapImage& img = atlasImages[pageIndex];

		RenderPassBegin()
			.RenderPass(renderPass.get())
			.RenderArea(0, 0, atlasImageSize, atlasImageSize)
			.Framebuffer(img.Framebuffer.get())
			.Execute(cmdbuffer.get());

		VkDeviceSize offset = 0;
		cmdbuffer->bindVertexBuffers(0, 1, &sceneVertexBuffer->buffer, &offset);
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, descriptorSet.get());

		for (size_t i = 0; i < mesh->surfaces.size(); i++)
		{
			Surface* surface = mesh->surfaces[i].get();
			if (surface->atlasPageIndex != pageIndex)
				continue;

			int sampleWidth = surface->texWidth;
			int sampleHeight = surface->texHeight;

#if 1
			int firstVertex = sceneVertexPos;
			int vertexCount = 4;
			sceneVertexPos += vertexCount;

			SceneVertex* vertex = &sceneVertices[firstVertex];
			vertex[0].Position = vec2(0.0f, 0.0f);
			vertex[1].Position = vec2(1.0f, 0.0f);
			vertex[2].Position = vec2(1.0f, 1.0f);
			vertex[3].Position = vec2(0.0f, 1.0f);
#else
			int firstVertex = sceneVertexPos;
			int vertexCount = (int)surface->lightUV.size();
			sceneVertexPos += vertexCount;

			SceneVertex* vertex = &sceneVertices[firstVertex];
			for (const vec2& uv : surface->lightUV)
			{
				(vertex++)->Position = vec2(uv.x / sampleWidth, uv.y / sampleHeight);
			}
#endif

			int firstLight = sceneLightPos;
			int lightCount = (int)surface->LightList.size();
			sceneLightPos += lightCount;

			LightInfo2* lightinfo = &sceneLights[firstLight];
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

			VkViewport viewport = {};
			viewport.maxDepth = 1;
			viewport.x = (float)surface->atlasX;
			viewport.y = (float)surface->atlasY;
			viewport.width = (float)sampleWidth;
			viewport.height = (float)sampleHeight;
			cmdbuffer->setViewport(0, 1, &viewport);

			PushConstants2 pc;
			pc.LightStart = firstLight;
			pc.LightEnd = firstLight + lightCount;
			pc.SurfaceIndex = (int32_t)i;
			pc.TileTL = vec2(0.0f);
			pc.TileBR = vec2(1.0f);
			pc.LightmapOrigin = surface->worldOrigin;
			pc.LightmapStepX = surface->worldStepX * (float)sampleWidth;
			pc.LightmapStepY = surface->worldStepY * (float)sampleHeight;
			cmdbuffer->pushConstants(pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants2), &pc);

			cmdbuffer->draw(vertexCount, 1, firstVertex, 0);
		}
		cmdbuffer->endRenderPass();
	}

	for (size_t i = 0; i < atlasImages.size(); i++)
	{
		LightmapImage& img = atlasImages[i];

		PipelineBarrier()
			.AddImage(img.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)
			.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = atlasImageSize;
		region.imageExtent.height = atlasImageSize;
		region.imageExtent.depth = 1;
		cmdbuffer->copyImageToBuffer(img.Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img.Transfer->buffer, 1, &region);
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
		vec4* pixels = (vec4*)atlasImages[pageIndex].Transfer->Map(0, atlasImageSize * atlasImageSize * sizeof(vec4));

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
				vec4* src = &pixels[atlasX + (atlasY + y) * atlasImageSize];
				for (int x = 0; x < sampleWidth; x++)
				{
					dest[x] = src[x].xyz();
				}
			}
		}
		atlasImages[pageIndex].Transfer->Unmap();
	}

	if (device->renderdoc)
		device->renderdoc->EndFrameCapture(0, 0);

	printf("Ray trace complete\n");
}

void GPURaytracer2::CreateVulkanObjects()
{
	submitFence = std::make_unique<VulkanFence>(device.get());
	cmdpool = std::make_unique<VulkanCommandPool>(device.get(), device->graphicsFamily);

	BeginCommands();

	CreateSceneVertexBuffer();
	CreateSceneLightBuffer();
	CreateVertexAndIndexBuffers();
	CreateBottomLevelAccelerationStructure();
	CreateTopLevelAccelerationStructure();
	CreateShaders();
	CreatePipeline();
	CreateDescriptorSet();

	FinishCommands();
}

void GPURaytracer2::CreateSceneVertexBuffer()
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

void GPURaytracer2::CreateSceneLightBuffer()
{
	size_t size = sizeof(LightInfo2) * SceneLightBufferSize;

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

	sceneLights = (LightInfo2*)sceneLightBuffer->Map(0, size);
	sceneLightPos = 0;
}

void GPURaytracer2::BeginCommands()
{
	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();
}

void GPURaytracer2::FinishCommands()
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

void GPURaytracer2::CreateVertexAndIndexBuffers()
{
	std::vector<SurfaceInfo2> surfaces = CreateSurfaceInfo();

	if (surfaces.empty()) // vulkan doesn't support zero byte buffers
		surfaces.push_back(SurfaceInfo2());

	size_t vertexbuffersize = (size_t)mesh->MeshVertices.Size() * sizeof(vec3);
	size_t indexbuffersize = (size_t)mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t surfaceindexbuffersize = (size_t)mesh->MeshSurfaces.Size() * sizeof(uint32_t);
	size_t surfacebuffersize = (size_t)surfaces.size() * sizeof(SurfaceInfo2);
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

void GPURaytracer2::CreateBottomLevelAccelerationStructure()
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

void GPURaytracer2::CreateTopLevelAccelerationStructure()
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

void GPURaytracer2::CreateShaders()
{
	vertShader = ShaderBuilder()
		.VertexShader(glsl_vert)
		.DebugName("vertShader")
		.Create("vertShader", device.get());

	fragShader = ShaderBuilder()
		.FragmentShader(glsl_frag)
		.DebugName("fragShader")
		.Create("fragShader", device.get());
}

void GPURaytracer2::CreatePipeline()
{
	descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("descriptorSetLayout")
		.Create(device.get());

	pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants2))
		.DebugName("pipelineLayout")
		.Create(device.get());

	renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R32G32B32A32_SFLOAT,
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
		.DebugName("renderpass")
		.Create(device.get());

	pipeline = GraphicsPipelineBuilder()
		.Layout(pipelineLayout.get())
		.RenderPass(renderPass.get())
		.AddVertexShader(vertShader.get())
		.AddFragmentShader(fragShader.get())
		.AddVertexBufferBinding(0, sizeof(SceneVertex))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, Position))
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
		.Scissor(0, 0, 4096, 4096)
		.DebugName("pipeline")
		.Create(device.get());
}

LightmapImage GPURaytracer2::CreateImage(int width, int height)
{
	LightmapImage img;

	img.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.Format(VK_FORMAT_R32G32B32A32_SFLOAT)
		.Size(width, height)
		.DebugName("LightmapImage.Image")
		.Create(device.get());

	img.View = ImageViewBuilder()
		.Image(img.Image.get(), VK_FORMAT_R32G32B32A32_SFLOAT)
		.DebugName("LightmapImage.View")
		.Create(device.get());

	img.Framebuffer = FramebufferBuilder()
		.RenderPass(renderPass.get())
		.Size(width, height)
		.AddAttachment(img.View.get())
		.DebugName("LightmapImage.Framebuffer")
		.Create(device.get());

	img.Transfer = BufferBuilder()
		.Size(width * height * sizeof(vec4))
		.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("LightmapImage.Transfer")
		.Create(device.get());

	return img;
}

void GPURaytracer2::CreateDescriptorSet()
{
	VkDeviceSize align = device->physicalDevice.properties.limits.minUniformBufferOffsetAlignment;
	uniformStructStride = (sizeof(Uniforms2) + align - 1) / align * align;

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

	descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
		.MaxSets(1)
		.DebugName("descriptorPool")
		.Create(device.get());

	descriptorSet = descriptorPool->allocate(descriptorSetLayout.get());
	descriptorSet->SetDebugName("descriptorSet");

	WriteDescriptors()
		.AddAccelerationStructure(descriptorSet.get(), 0, tlAccelStruct.get())
		.AddBuffer(descriptorSet.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffer.get(), 0, sizeof(Uniforms2))
		.AddBuffer(descriptorSet.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceIndexBuffer.get())
		.AddBuffer(descriptorSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceBuffer.get())
		.AddBuffer(descriptorSet.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sceneLightBuffer.get())
		.Execute(device.get());
}

std::vector<SurfaceInfo2> GPURaytracer2::CreateSurfaceInfo()
{
	std::vector<SurfaceInfo2> surfaces;
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

		SurfaceInfo2 info;
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
	return surfaces;
}

void GPURaytracer2::PrintVulkanInfo()
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
