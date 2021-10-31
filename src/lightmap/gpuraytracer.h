
#pragma once

#include "vulkandevice.h"
#include "vulkanobjects.h"

class LevelMesh;

class GPURaytracer
{
public:
	GPURaytracer();
	~GPURaytracer();

	void Raytrace(LevelMesh* level);

private:
	void CreateVertexAndIndexBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();
	void CreateShaders();

	void RaytraceProbeSample(LightProbeSample* probe);
	void RaytraceSurfaceSample(Surface* surface, int x, int y);
	Vec3 TracePath(const Vec3& pos, const Vec3& dir, int sampleIndex, int depth = 0);

	Vec3 GetLightEmittance(Surface* surface, const Vec3& pos);
	Vec3 GetSurfaceEmittance(Surface* surface, float distance);

	static float RadicalInverse_VdC(uint32_t bits);
	static Vec2 Hammersley(uint32_t i, uint32_t N);
	static Vec3 ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness);

	int SAMPLE_COUNT = 1024;// 128;// 1024;

	LevelMesh* mesh = nullptr;

	std::unique_ptr<VulkanDevice> device;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;

	std::unique_ptr<VulkanBuffer> blScratchBuffer;
	std::unique_ptr<VulkanBuffer> blAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> blAccelStruct;

	std::unique_ptr<VulkanBuffer> tlTransferBuffer;
	std::unique_ptr<VulkanBuffer> tlScratchBuffer;
	std::unique_ptr<VulkanBuffer> tlInstanceBuffer;
	std::unique_ptr<VulkanBuffer> tlAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> tlAccelStruct;

	std::unique_ptr<VulkanShader> shaderRayGen;

	std::unique_ptr<VulkanCommandPool> cmdpool;
	std::unique_ptr<VulkanCommandBuffer> cmdbuffer;
};
