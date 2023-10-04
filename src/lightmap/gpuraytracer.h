
#pragma once

#include <zvulkan/vulkandevice.h>
#include <zvulkan/vulkanobjects.h>

class LevelMesh;

struct Uniforms
{
	vec3 SunDir;
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
};

struct PushConstants
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

struct SurfaceInfo
{
	vec3 Normal;
	float Sky;
	float SamplingDistance;
	uint32_t PortalIndex;
	float Padding1, Padding2;
};

static_assert(sizeof(SurfaceInfo) == sizeof(float) * 8);

struct PortalInfo
{
	mat4 Transformation;
};

static_assert(sizeof(PortalInfo) == sizeof(float) * 16);

struct LightInfo
{
	vec3 Origin;
	float Padding0;
	vec3 RelativeOrigin;
	float Padding1;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	vec3 SpotDir;
	float Padding2;
	vec3 Color;
	float Padding3;
};

static_assert(sizeof(LightInfo) == sizeof(float) * 20);

struct CollisionNodeBufferHeader
{
	int root;
	int padding1;
	int padding2;
	int padding3;
};

struct CollisionNode
{
	vec3 center;
	float padding1;
	vec3 extents;
	float padding2;
	int left;
	int right;
	int element_index;
	int padding3;
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

	void LoadRenderDoc();
	void PrintVulkanInfo();

	std::vector<SurfaceInfo> CreateSurfaceInfo();
	std::vector<PortalInfo> CreatePortalInfo();
	std::vector<CollisionNode> CreateCollisionNodes();

	static vec2 ToUV(const vec3& vert, const Surface* targetSurface);

	LevelMesh* mesh = nullptr;

	uint8_t* mappedUniforms = nullptr;
	int uniformsIndex = 0;
	int uniformStructs = 256;
	VkDeviceSize uniformStructStride = sizeof(Uniforms);

	std::shared_ptr<VulkanDevice> device;

	bool useRayQuery = true;

	static const int SceneVertexBufferSize = 1 * 1024 * 1024;
	std::unique_ptr<VulkanBuffer> sceneVertexBuffer;
	SceneVertex* sceneVertices = nullptr;
	int sceneVertexPos = 0;

	static const int SceneLightBufferSize = 2 * 1024 * 1024;
	std::unique_ptr<VulkanBuffer> sceneLightBuffer;
	LightInfo* sceneLights = nullptr;
	int sceneLightPos = 0;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;
	std::unique_ptr<VulkanBuffer> surfaceIndexBuffer;
	std::unique_ptr<VulkanBuffer> surfaceBuffer;
	std::unique_ptr<VulkanBuffer> portalBuffer;
	std::unique_ptr<VulkanBuffer> nodesBuffer;

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
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout0;
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout1;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPassBegin;
		std::unique_ptr<VulkanRenderPass> renderPassContinue;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool0;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool1;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet0;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet1;
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
