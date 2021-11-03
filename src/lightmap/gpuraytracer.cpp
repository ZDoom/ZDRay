
#include "math/mathlib.h"
#include "surfaces.h"
#include "level/level.h"
#include "gpuraytracer.h"
#include "surfacelight.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include "vulkanbuilders.h"
#include "stacktrace.h"
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>

extern int Multisample;
extern int LightBounce;
extern float GridSize;

GPURaytracer::GPURaytracer()
{
	auto printLog = [](const char* typestr, const std::string& msg)
	{
		printf("\n[%s] %s\n", typestr, msg.c_str());

		std::string callstack = CaptureStackTraceText(0);
		if (!callstack.empty())
			printf("%s\n", callstack.c_str());
	};
	device = std::make_unique<VulkanDevice>(0, true, printLog);

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
	/*
	printf("Vulkan extensions:");
	for (const VkExtensionProperties& p : device->physicalDevice.extensions)
	{
		printf(" %s", p.extensionName);
	}
	printf("\n");
	*/
}

GPURaytracer::~GPURaytracer()
{
}

void GPURaytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	cmdpool = std::make_unique<VulkanCommandPool>(device.get(), device->graphicsFamily);
	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();

	printf("Creating vertex and index buffers\n");
	CreateVertexAndIndexBuffers();

	printf("Creating bottom level acceleration structure\n");
	CreateBottomLevelAccelerationStructure();

	printf("Creating top level acceleration structure\n");
	CreateTopLevelAccelerationStructure();

	printf("Creating shaders\n");
	CreateShaders();

	printf("Creating pipeline\n");
	CreatePipeline();

	printf("Creating descriptor set\n");
	CreateDescriptorSet();

	printf("Creating render setup\n");

	Uniforms uniforms;
	uniforms.viewInverse.Identity();
	uniforms.projInverse.Identity();

	auto data = uniformTransferBuffer->Map(0, sizeof(Uniforms));
	memcpy(data, &uniforms, sizeof(Uniforms));
	uniformTransferBuffer->Unmap();
	cmdbuffer->copyBuffer(uniformTransferBuffer.get(), uniformBuffer.get());

	PipelineBarrier barrier;
	barrier.addBuffer(uniformBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	barrier.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	printf("Starting render\n");

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0, descriptorSet.get());
	cmdbuffer->traceRays(&rgenRegion, &missRegion, &hitRegion, &callRegion, 1024, 1024, 1);
	cmdbuffer->end();

	auto submitFence = std::make_unique<VulkanFence>(device.get());

	QueueSubmit submit;
	submit.addCommandBuffer(cmdbuffer.get());
	submit.execute(device.get(), device->graphicsQueue, submitFence.get());

	vkWaitForFences(device->device, 1, &submitFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device->device, 1, &submitFence->fence);

	printf("Render complete\n");


#if 0
	printf("Tracing light probes\n");

	Worker::RunJob((int)mesh->lightProbes.size(), [=](int id) {
		RaytraceProbeSample(&mesh->lightProbes[id]);
	});

	printf("Tracing surfaces (%d bounces)\n", LightBounce);

	struct SurfaceTask
	{
		int surf, x, y;
	};
	std::vector<SurfaceTask> tasks;

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();
		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		for (int y = 0; y < sampleHeight; y++)
		{
			for (int x = 0; x < sampleWidth; x++)
			{
				SurfaceTask task;
				task.surf = (int)i;
				task.x = x;
				task.y = y;
				tasks.push_back(task);
			}
		}
	}

	Worker::RunJob((int)tasks.size(), [=](int id) {
		const SurfaceTask& task = tasks[id];
		RaytraceSurfaceSample(mesh->surfaces[task.surf].get(), task.x, task.y);
	});
#endif

	printf("Raytrace complete\n");
}

void GPURaytracer::RaytraceProbeSample(LightProbeSample* probe)
{
	Vec3 color(0.0f, 0.0f, 0.0f);

	for (ThingLight& light : mesh->map->ThingLights)
	{
		Vec3 lightOrigin = light.LightOrigin();
		float lightRadius = light.LightRadius();

		if (probe->Position.DistanceSq(lightOrigin) > (lightRadius * lightRadius))
			continue;

		if (mesh->TraceAnyHit(lightOrigin, probe->Position))
			continue; // this light is occluded by something

		Vec3 dir = (lightOrigin - probe->Position);
		float dist = dir.Unit();
		dir.Normalize();

		color += light.rgb * (light.SpotAttenuation(dir) * light.DistAttenuation(dist) * light.intensity);
	}

	probe->Color = color;
}

void GPURaytracer::RaytraceSurfaceSample(Surface* surface, int x, int y)
{
	Vec3 normal = surface->plane.Normal();
	Vec3 pos = surface->lightmapOrigin + normal + surface->lightmapSteps[0] * (float)x + surface->lightmapSteps[1] * (float)y;

	Vec3 incoming(0.0f, 0.0f, 0.0f);
	if (LightBounce > 0)
	{
		float totalWeight = 0.0f;
		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			Vec2 Xi = Hammersley(i, SAMPLE_COUNT);
			Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
			Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);
			float NdotL = std::max(Vec3::Dot(normal, L), 0.0f);
			if (NdotL > 0.0f)
			{
				incoming += TracePath(pos, L, i) * NdotL;
				totalWeight += NdotL;
			}
		}
		incoming = incoming / totalWeight / (float)LightBounce;
	}

	incoming = incoming + GetSurfaceEmittance(surface, 0.0f) + GetLightEmittance(surface, pos);

	size_t sampleWidth = surface->lightmapDims[0];
	surface->samples[x + y * sampleWidth] = incoming;
}

Vec3 GPURaytracer::GetLightEmittance(Surface* surface, const Vec3& pos)
{
	Vec3 emittance = Vec3(0.0f);
	for (ThingLight& light : mesh->map->ThingLights)
	{
		Vec3 lightOrigin = light.LightOrigin();
		float lightRadius = light.LightRadius();

		if (surface->plane.Distance(lightOrigin) - surface->plane.d < 0)
			continue; // completely behind the plane

		if (pos.DistanceSq(lightOrigin) > (lightRadius * lightRadius))
			continue; // light too far away

		Vec3 dir = (lightOrigin - pos);
		float dist = dir.Unit();
		dir.Normalize();

		float attenuation = light.SpotAttenuation(dir) * light.DistAttenuation(dist) * surface->plane.Normal().Dot(dir);
		if (attenuation <= 0.0f)
			continue;

		if (mesh->TraceAnyHit(lightOrigin, pos))
			continue; // this light is occluded by something

		emittance += light.rgb * (attenuation * light.intensity);
	}
	return emittance;
}

Vec3 GPURaytracer::TracePath(const Vec3& pos, const Vec3& dir, int sampleIndex, int depth)
{
	if (depth >= LightBounce)
		return Vec3(0.0f);

	LevelTraceHit hit = mesh->Trace(pos, pos + dir * 1000.0f);
	if (hit.fraction == 1.0f)
		return Vec3(0.0f);

	Vec3 normal = hit.hitSurface->plane.Normal();
	Vec3 hitpos = hit.start * (1.0f - hit.fraction) + hit.end * hit.fraction + normal * 0.1f;

	Vec3 emittance = GetSurfaceEmittance(hit.hitSurface, pos.Distance(hitpos)) + GetLightEmittance(hit.hitSurface, hitpos) * 0.5f;

	Vec2 Xi = Hammersley(sampleIndex, SAMPLE_COUNT);
	Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
	Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);

	float NdotL = std::max(Vec3::Dot(normal, L), 0.0f);
	if (NdotL <= 0.0f)
		return emittance;

	const float p = 1 / (2 * M_PI);
	Vec3 incoming = TracePath(hitpos, normal, (sampleIndex + depth + 1) % SAMPLE_COUNT, depth + 1);

	return emittance + incoming * NdotL / p;
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

Vec2 GPURaytracer::Hammersley(uint32_t i, uint32_t N)
{
	return Vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

Vec3 GPURaytracer::ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * M_PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	Vec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

	// from tangent-space vector to world-space sample vector
	Vec3 up = std::abs(N.z) < 0.999f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
	Vec3 tangent = Vec3::Normalize(Vec3::Cross(up, N));
	Vec3 bitangent = Vec3::Cross(N, tangent);

	Vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return Vec3::Normalize(sampleVec);
}

Vec3 GPURaytracer::GetSurfaceEmittance(Surface* surface, float distance)
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

	if (def)
	{
		float attenuation = std::max(1.0f - (distance / def->distance), 0.0f);
		return def->rgb * (attenuation * def->intensity);
	}
	else
	{
		return Vec3(0.0f);
	}
}

void GPURaytracer::CreateVertexAndIndexBuffers()
{
	size_t vertexbuffersize = (size_t)mesh->MeshVertices.Size() * sizeof(Vec3);
	size_t indexbuffersize = (size_t)mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t transferbuffersize = vertexbuffersize + indexbuffersize;
	size_t vertexoffset = 0;
	size_t indexoffset = vertexoffset + vertexbuffersize;

	BufferBuilder vbuilder;
	vbuilder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	vbuilder.setSize(vertexbuffersize);
	vertexBuffer = vbuilder.create(device.get());

	BufferBuilder ibuilder;
	ibuilder.setUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	ibuilder.setSize(indexbuffersize);
	indexBuffer = ibuilder.create(device.get());

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(transferbuffersize);
	transferBuffer = tbuilder.create(device.get());
	uint8_t* data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	memcpy(data + vertexoffset, mesh->MeshVertices.Data(), vertexbuffersize);
	memcpy(data + indexoffset, mesh->MeshElements.Data(), indexbuffersize);
	transferBuffer->Unmap();

	cmdbuffer->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);

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
	triangles.vertexStride = sizeof(Vec3);
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
	auto data = (uint8_t*)tlTransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR));
	memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer->Unmap();

	BufferBuilder instbufbuilder;
	instbufbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	instbufbuilder.setSize(sizeof(VkAccelerationStructureInstanceKHR));
	tlInstanceBuffer = instbufbuilder.create(device.get());

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
	static bool firstCall = true;
	if (firstCall)
	{
		ShaderBuilder::init();
		firstCall = false;
	}

	{
		std::string code = R"(
			#version 460
			#extension GL_EXT_ray_tracing : require

			struct hitPayload
			{
				vec3 hitValue;
			};

			layout(location = 0) rayPayloadEXT hitPayload payload;

			layout(set = 0, binding = 0) uniform accelerationStructureEXT acc;
			layout(set = 0, binding = 1, rgba32f) uniform image2D image;

			layout(set = 0, binding = 2) uniform Uniforms
			{
				mat4 viewInverse;
				mat4 projInverse;
			};

			void main()
			{
				const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
				const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
				vec2 d = inUV * 2.0 - 1.0;

				vec4 origin = viewInverse * vec4(0, 0, 0, 1);
				vec4 target = projInverse * vec4(d.x, d.y, 1, 1);
				vec4 direction = viewInverse * vec4(normalize(target.xyz), 0);

				traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin.xyz, 0.001, direction.xyz, 10000.0, 0);

				imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(payload.hitValue, 1.0));
			}
		)";

		ShaderBuilder builder;
		builder.setRayGenShader(code);
		shaderRayGen = builder.create(device.get());
	}

	{
		std::string code = R"(
			#version 460
			#extension GL_EXT_ray_tracing : require

			struct hitPayload
			{
				vec3 hitValue;
			};

			layout(location = 0) rayPayloadEXT hitPayload payload;

			void main()
			{
				payload.hitValue = vec3(1.0);
			}
		)";

		ShaderBuilder builder;
		builder.setMissShader(code);
		shaderMiss = builder.create(device.get());
	}

	{
		std::string code = R"(
			#version 460
			#extension GL_EXT_ray_tracing : require

			struct hitPayload
			{
				vec3 hitValue;
			};

			layout(location = 0) rayPayloadEXT hitPayload payload;

			void main()
			{
				payload.hitValue = vec3(0.0);
			}
		)";

		ShaderBuilder builder;
		builder.setClosestHitShader(code);
		shaderClosestHit = builder.create(device.get());
	}
}

void GPURaytracer::CreatePipeline()
{
	DescriptorSetLayoutBuilder setbuilder;
	setbuilder.addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	setbuilder.addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	descriptorSetLayout = setbuilder.create(device.get());

	PipelineLayoutBuilder layoutbuilder;
	layoutbuilder.addSetLayout(descriptorSetLayout.get());
	pipelineLayout = layoutbuilder.create(device.get());

	RayTracingPipelineBuilder builder;
	builder.setLayout(pipelineLayout.get());
	builder.setMaxPipelineRayRecursionDepth(1);
	builder.addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, shaderRayGen.get());
	builder.addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderMiss.get());
	builder.addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderClosestHit.get());
	builder.addRayGenGroup(0);
	builder.addMissGroup(1);
	builder.addTrianglesHitGroup(2);
	pipeline = builder.create(device.get());

	// OK, this is by far the WORST idea I've seen in vulkan yet. And that's saying a lot.
	// 
	// Each shader type has its own table, which needs to be aligned.
	// That means we can't just copy the shader table pointers directly. Each group needs to be sized up and copied individually.

	const auto& rtProperties = device->physicalDevice.rayTracingProperties;

	auto align_up = [](VkDeviceSize value, VkDeviceSize alignment)
	{
		if (alignment != 0)
			return (value + alignment - 1) / alignment * alignment;
		else
			return value;
	};

	VkDeviceSize missCount = 1;
	VkDeviceSize hitCount = 1;

	VkDeviceSize handleSize = rtProperties.shaderGroupHandleSize;
	VkDeviceSize handleSizeAligned = align_up(handleSize, rtProperties.shaderGroupHandleAlignment);

	rgenRegion.stride = align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
	missRegion.stride = handleSizeAligned;
	hitRegion.stride = handleSizeAligned;

	rgenRegion.size = align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
	missRegion.size = align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
	hitRegion.size = align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

	VkDeviceSize rgenOffset = 0;
	VkDeviceSize missOffset = rgenOffset + rgenRegion.size;
	VkDeviceSize hitOffset = missOffset + missRegion.size;

	VkDeviceSize sbtBufferSize = rgenRegion.size + missRegion.size + hitRegion.size;

	BufferBuilder bufbuilder;
	bufbuilder.setUsage(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	bufbuilder.setSize(sbtBufferSize);
	shaderBindingTable = bufbuilder.create(device.get());

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(sbtBufferSize);
	sbtTransferBuffer = tbuilder.create(device.get());
	uint8_t* src = (uint8_t*)pipeline->shaderGroupHandles.data();
	uint8_t* dest = (uint8_t*)sbtTransferBuffer->Map(0, sbtBufferSize);
	memcpy(dest + rgenOffset, src, handleSize);
	for (VkDeviceSize i = 0; i < missCount; i++)
		memcpy(dest + missOffset + i * missRegion.stride, src + (1 + i) * handleSize, handleSize);
	for (VkDeviceSize i = 0; i < hitCount; i++)
		memcpy(dest + hitOffset, src + (1 + missCount + i) * handleSize, handleSize);
	sbtTransferBuffer->Unmap();

	cmdbuffer->copyBuffer(sbtTransferBuffer.get(), shaderBindingTable.get());

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = shaderBindingTable->buffer;
	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device->device, &info);

	rgenRegion.deviceAddress = sbtAddress + rgenOffset;
	missRegion.deviceAddress = sbtAddress + missOffset;
	hitRegion.deviceAddress = sbtAddress + hitOffset;
}

void GPURaytracer::CreateDescriptorSet()
{
	BufferBuilder uniformbuilder;
	uniformbuilder.setUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	uniformbuilder.setSize(sizeof(Uniforms));
	uniformBuffer = uniformbuilder.create(device.get());

	BufferBuilder uniformtransferbuilder;
	uniformtransferbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uniformtransferbuilder.setSize(sizeof(Uniforms));
	uniformTransferBuffer = uniformtransferbuilder.create(device.get());

	ImageBuilder builder;
	builder.setUsage(VK_IMAGE_USAGE_STORAGE_BIT);
	builder.setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
	builder.setSize(1024, 1024);
	outputImage = builder.create(device.get());

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(outputImage.get(), VK_FORMAT_R32G32B32A32_SFLOAT);
	outputImageView = viewbuilder.create(device.get());

	PipelineBarrier barrier;
	barrier.addImage(outputImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	barrier.execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	poolbuilder.setMaxSets(1);
	descriptorPool = poolbuilder.create(device.get());

	descriptorSet = descriptorPool->allocate(descriptorSetLayout.get());

	WriteDescriptors write;
	write.addAccelerationStructure(descriptorSet.get(), 0, tlAccelStruct.get());
	write.addStorageImage(descriptorSet.get(), 1, outputImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
	write.addBuffer(descriptorSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffer.get());
	write.updateSets(device.get());
}
