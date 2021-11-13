
#pragma once

#include "vulkandevice.h"
#include "vulkanobjects.h"

class LevelMesh;

struct Uniforms
{
	uint32_t SampleIndex;
	uint32_t SampleCount;
	uint32_t PassType;
	uint32_t LightCount;
	vec3 SunDir;
	float SampleDistance;
	vec3 SunColor;
	float SunIntensity;
	vec3 HemisphereVec;
	float Padding1;
};

struct SurfaceInfo
{
	vec3 Normal;
	float EmissiveDistance;
	vec3 EmissiveColor;
	float EmissiveIntensity;
	float Sky;
	float Padding0, Padding1, Padding2;
};

struct LightInfo
{
	vec3 Origin;
	float Padding0;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	vec3 SpotDir;
	float Padding1;
	vec3 Color;
	float Padding2;
};

struct TraceTask
{
	int id, x, y;
};

class GPURaytracer
{
public:
	GPURaytracer();
	~GPURaytracer();

	void Raytrace(LevelMesh* level);

private:
	void CreateVulkanObjects();
	void CreateVertexAndIndexBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();
	void CreateShaders();
	std::unique_ptr<VulkanShader> CompileRayGenShader(const char* code, const char* name);
	std::unique_ptr<VulkanShader> CompileClosestHitShader(const char* code, const char* name);
	std::unique_ptr<VulkanShader> CompileMissShader(const char* code, const char* name);
	void CreatePipeline();
	void CreateDescriptorSet();

	void UploadTasks(const TraceTask* tasks, size_t size);
	void BeginTracing();
	void RunTrace(const Uniforms& uniforms, const VkStridedDeviceAddressRegionKHR& rgenShader);
	void EndTracing();
	void DownloadTasks(const TraceTask* tasks, size_t size);
	void SubmitCommands();

	void PrintVulkanInfo();

	static float RadicalInverse_VdC(uint32_t bits);
	static vec2 Hammersley(uint32_t i, uint32_t N);

	const int coverageSampleCount = 256;
	const int bounceSampleCount = 2048;
	int rayTraceImageSize = 1024;

	LevelMesh* mesh = nullptr;

	uint8_t* mappedUniforms = nullptr;
	int uniformsIndex = 0;
	int uniformStructs = 256;
	VkDeviceSize uniformStructStride = sizeof(Uniforms);

	std::unique_ptr<VulkanDevice> device;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;
	std::unique_ptr<VulkanBuffer> surfaceIndexBuffer;
	std::unique_ptr<VulkanBuffer> surfaceBuffer;
	std::unique_ptr<VulkanBuffer> lightBuffer;

	std::unique_ptr<VulkanBuffer> blScratchBuffer;
	std::unique_ptr<VulkanBuffer> blAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> blAccelStruct;

	std::unique_ptr<VulkanBuffer> tlTransferBuffer;
	std::unique_ptr<VulkanBuffer> tlScratchBuffer;
	std::unique_ptr<VulkanBuffer> tlInstanceBuffer;
	std::unique_ptr<VulkanBuffer> tlAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> tlAccelStruct;

	std::unique_ptr<VulkanShader> rgenBounce, rgenLight;
	std::unique_ptr<VulkanShader> rmissBounce, rmissLight, rmissSun;
	std::unique_ptr<VulkanShader> rchitBounce, rchitLight, rchitSun;

	std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;

	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipeline> pipeline;
	std::unique_ptr<VulkanBuffer> shaderBindingTable;
	std::unique_ptr<VulkanBuffer> sbtTransferBuffer;

	VkStridedDeviceAddressRegionKHR rgenBounceRegion = {}, rgenLightRegion = {};
	VkStridedDeviceAddressRegionKHR missRegion = {};
	VkStridedDeviceAddressRegionKHR hitRegion = {};
	VkStridedDeviceAddressRegionKHR callRegion = {};

	std::unique_ptr<VulkanImage> startPositionsImage, positionsImage, outputImage;
	std::unique_ptr<VulkanImageView> startPositionsImageView, positionsImageView, outputImageView;
	std::unique_ptr<VulkanBuffer> imageTransferBuffer;

	std::unique_ptr<VulkanBuffer> uniformBuffer;
	std::unique_ptr<VulkanBuffer> uniformTransferBuffer;

	std::unique_ptr<VulkanDescriptorPool> descriptorPool;
	std::unique_ptr<VulkanDescriptorSet> descriptorSet;

	std::unique_ptr<VulkanCommandPool> cmdpool;
	std::unique_ptr<VulkanCommandBuffer> cmdbuffer;

};
