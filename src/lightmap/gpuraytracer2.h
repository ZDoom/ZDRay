
#pragma once

#include "vulkandevice.h"
#include "vulkanobjects.h"

class LevelMesh;

struct Uniforms2
{
	vec3 SunDir;
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
};

struct PushConstants2
{
	uint32_t LightStart;
	uint32_t LightEnd;
	int32_t SurfaceIndex;
	int32_t PushPadding1;
	vec3 LightmapOrigin;
	float PushPadding2;
	vec3 LightmapStepX;
	float PushPadding3;
	vec3 LightmapStepY;
	float PushPadding4;
};

struct SurfaceInfo2
{
	vec3 Normal;
	float EmissiveDistance;
	vec3 EmissiveColor;
	float EmissiveIntensity;
	float Sky;
	float SamplingDistance;
	float Padding1, Padding2;
};

struct LightInfo2
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

struct LightmapImage
{
	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
	} resolve;

	std::unique_ptr<VulkanBuffer> Transfer;
};

struct SceneVertex
{
	vec2 Position;
};

class GPURaytracer2
{
public:
	GPURaytracer2();
	~GPURaytracer2();

	void Raytrace(LevelMesh* level);

private:
	void CreateVulkanObjects();
	void CreateVertexAndIndexBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();
	void CreateShaders();
	void CreateRaytracePipeline();
	void CreateResolvePipeline();
	void CreateUniformBuffer();
	void CreateSceneVertexBuffer();
	void CreateSceneLightBuffer();

	void UploadUniforms();
	void CreateAtlasImages();
	void RenderAtlasImage(size_t pageIndex);
	void ResolveAtlasImage(size_t pageIndex);
	void DownloadAtlasImage(size_t pageIndex);

	LightmapImage CreateImage(int width, int height);

	void BeginCommands();
	void FinishCommands();

	void PrintVulkanInfo();

	std::vector<SurfaceInfo2> CreateSurfaceInfo();

	static vec2 ToUV(const vec3& vert, const Surface* targetSurface);
	static bool IsNegativelyOriented(const vec2& v1, const vec2& v2, const vec2& v3);

	LevelMesh* mesh = nullptr;

	uint8_t* mappedUniforms = nullptr;
	int uniformsIndex = 0;
	int uniformStructs = 256;
	VkDeviceSize uniformStructStride = sizeof(Uniforms2);

	std::unique_ptr<VulkanDevice> device;

	static const int SceneVertexBufferSize = 1 * 1024 * 1024;
	std::unique_ptr<VulkanBuffer> sceneVertexBuffer;
	SceneVertex* sceneVertices = nullptr;
	int sceneVertexPos = 0;

	static const int SceneLightBufferSize = 2 * 1024 * 1024;
	std::unique_ptr<VulkanBuffer> sceneLightBuffer;
	LightInfo2* sceneLights = nullptr;
	int sceneLightPos = 0;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;
	std::unique_ptr<VulkanBuffer> surfaceIndexBuffer;
	std::unique_ptr<VulkanBuffer> surfaceBuffer;

	std::unique_ptr<VulkanBuffer> blScratchBuffer;
	std::unique_ptr<VulkanBuffer> blAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> blAccelStruct;

	std::unique_ptr<VulkanBuffer> tlTransferBuffer;
	std::unique_ptr<VulkanBuffer> tlScratchBuffer;
	std::unique_ptr<VulkanBuffer> tlInstanceBuffer;
	std::unique_ptr<VulkanBuffer> tlAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> tlAccelStruct;

	std::unique_ptr<VulkanShader> vertShader;
	std::unique_ptr<VulkanShader> fragShader;
	std::unique_ptr<VulkanShader> fragResolveShader;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> descriptorSets;
		std::unique_ptr<VulkanSampler> sampler;
	} resolve;

	std::unique_ptr<VulkanBuffer> uniformBuffer;
	std::unique_ptr<VulkanBuffer> uniformTransferBuffer;

	std::unique_ptr<VulkanFence> submitFence;
	std::unique_ptr<VulkanCommandPool> cmdpool;
	std::unique_ptr<VulkanCommandBuffer> cmdbuffer;

	std::vector<LightmapImage> atlasImages;
	static const int atlasImageSize = 2048;
};
