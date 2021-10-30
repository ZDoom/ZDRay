
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
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>

extern int Multisample;
extern int LightBounce;
extern float GridSize;

GPURaytracer::GPURaytracer()
{
	device = std::make_unique<VulkanDevice>();
}

GPURaytracer::~GPURaytracer()
{
}

void GPURaytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	std::unique_ptr<VulkanCommandPool> cmdpool = std::make_unique<VulkanCommandPool>(device.get(), device->graphicsFamily);
	std::unique_ptr<VulkanCommandBuffer> cmdbuffer = cmdpool->createBuffer();

	printf("Creating vertex and index buffers\n");

	size_t vertexbuffersize = (size_t)mesh->MeshVertices.Size() * sizeof(Vec3);
	size_t indexbuffersize = (size_t)mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t transferbuffersize = vertexbuffersize + indexbuffersize;
	size_t vertexoffset = 0;
	size_t indexoffset = vertexoffset + vertexbuffersize;

	BufferBuilder vbuilder;
	vbuilder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	vbuilder.setSize(vertexbuffersize);
	auto vertexBuffer = vbuilder.create(device.get());

	BufferBuilder ibuilder;
	ibuilder.setUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	ibuilder.setSize(indexbuffersize);
	auto indexBuffer = ibuilder.create(device.get());

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(transferbuffersize);
	auto transferBuffer = tbuilder.create(device.get());
	auto data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	memcpy(data + vertexoffset, mesh->MeshVertices.Data(), vertexbuffersize);
	memcpy(data + indexoffset, mesh->MeshElements.Data(), indexbuffersize);
	transferBuffer->Unmap();

	cmdbuffer->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	cmdbuffer->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);

	printf("Creating bottom level acceleration structure\n");

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
	rangeInfo.firstVertex = 0;
	rangeInfo.primitiveCount = mesh->MeshElements.Size() / 3;
	rangeInfo.primitiveOffset = 0;
	rangeInfo.transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = accelStructBLDesc.flags | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructBLDesc;

	uint32_t maxPrimitiveCount = rangeInfo.primitiveCount;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	sbuilder.setSize(sizeInfo.buildScratchSize);
	auto scratchBuffer = sbuilder.create(device.get());

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = scratchBuffer->buffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device->device, &info);

	BufferBuilder blbufbuilder;
	blbufbuilder.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	blbufbuilder.setSize(sizeInfo.accelerationStructureSize);
	auto blAccelStructBuffer = blbufbuilder.create(device.get());

	VkAccelerationStructureKHR blAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.buffer = blAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &blAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	auto blAccelStruct = std::make_unique<VulkanAccelerationStructure>(device.get(), blAccelStructHandle);

	buildInfo.dstAccelerationStructure = blAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	vkCmdBuildAccelerationStructuresKHR(cmdbuffer->buffer, 1, &buildInfo, rangeInfos);

	printf("Creating top level acceleration structure\n");

	/*
	Vec3 aabbs[2] = { mesh->MeshVertices[0], mesh->MeshVertices[1] };
	for (Vec3& v : mesh->MeshVertices)
	{
		aabbs[0].x = std::min(aabbs[0].x, v.x);
		aabbs[0].y = std::min(aabbs[0].y, v.y);
		aabbs[0].z = std::min(aabbs[0].z, v.z);
		aabbs[1].x = std::max(aabbs[1].x, v.x);
		aabbs[1].y = std::max(aabbs[1].y, v.y);
		aabbs[1].z = std::max(aabbs[1].z, v.z);
	}
	*/

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
