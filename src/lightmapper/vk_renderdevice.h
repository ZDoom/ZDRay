#pragma once

#include "framework/zstring.h"
#include "framework/textureid.h"
#include "zvulkan/vulkanobjects.h"
#include "zvulkan/vulkanbuilders.h"
#include "framework/matrix.h"
#include <stdexcept>
#include <unordered_map>

class VkLevelMesh;
class VkLightmapper;
class VkCommandBufferManager;
class VkDescriptorSetManager;
class VkTextureManager;
class VkSamplerManager;
class VulkanSwapChain;
class LevelMeshViewer;
class VSMatrix;

class VulkanRenderDevice
{
public:
	VulkanRenderDevice(LevelMeshViewer* viewer);
	~VulkanRenderDevice();

	VulkanDevice* GetDevice() { return device.get(); }
	VulkanSwapChain* GetSwapChain() { return swapchain.get(); }
	VkCommandBufferManager* GetCommands() { return commands.get(); }
	VkDescriptorSetManager* GetDescriptorSetManager() { return descriptors.get(); }
	VkTextureManager* GetTextureManager() { return textures.get(); }
	VkSamplerManager* GetSamplerManager() { return samplers.get(); }
	VkLevelMesh* GetLevelMesh() { return levelmesh.get(); }
	VkLightmapper* GetLightmapper() { return lightmapper.get(); }

	int GetBindlessTextureIndex(FTextureID texture);

	bool IsRayQueryEnabled() const { return useRayQuery; }

	void ResizeSwapChain(int width, int height);
	void DrawViewer(const FVector3& cameraPos, const VSMatrix& viewToWorld, float fovy, float aspect, const FVector3& sundir, const FVector3& suncolor, float sunintensity);

	bool useRayQuery = false;

private:
	void CreateViewerObjects();

	std::shared_ptr<VulkanDevice> device;
	std::shared_ptr<VulkanSwapChain> swapchain;
	std::unique_ptr<VkCommandBufferManager> commands;
	std::unique_ptr<VkDescriptorSetManager> descriptors;
	std::unique_ptr<VkTextureManager> textures;
	std::unique_ptr<VkSamplerManager> samplers;
	std::unique_ptr<VkLevelMesh> levelmesh;
	std::unique_ptr<VkLightmapper> lightmapper;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> DescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorPool> DescriptorPool;
		std::unique_ptr<VulkanDescriptorSet> DescriptorSet;
		std::unique_ptr<VulkanShader> VertexShader;
		std::unique_ptr<VulkanShader> FragmentShader;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanPipeline> Pipeline;
	} Viewer;

	int CurrentWidth = 0;
	int CurrentHeight = 0;
	std::vector<std::unique_ptr<VulkanFramebuffer>> Framebuffers;
	std::unordered_map<FGameTexture*, int> TextureIndexes;
};

class VkCommandBufferManager
{
public:
	VkCommandBufferManager(VulkanRenderDevice* fb);

	void SubmitAndWait(int imageIndex = -1);

	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();

	void PushGroup(VulkanCommandBuffer* cmdbuffer, const FString& name) { }
	void PopGroup(VulkanCommandBuffer* cmdbuffer) { }

	int AcquireImage();

	class DeleteList
	{
	public:
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		std::vector<std::unique_ptr<VulkanSampler>> Samplers;
		std::vector<std::unique_ptr<VulkanImage>> Images;
		std::vector<std::unique_ptr<VulkanImageView>> ImageViews;
		std::vector<std::unique_ptr<VulkanFramebuffer>> Framebuffers;
		std::vector<std::unique_ptr<VulkanAccelerationStructure>> AccelStructs;
		std::vector<std::unique_ptr<VulkanDescriptorPool>> DescriptorPools;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> Descriptors;
		std::vector<std::unique_ptr<VulkanShader>> Shaders;
		std::vector<std::unique_ptr<VulkanCommandBuffer>> CommandBuffers;
		size_t TotalSize = 0;

		void Add(std::unique_ptr<VulkanBuffer> obj) { if (obj) { TotalSize += obj->size; Buffers.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanSampler> obj) { if (obj) { Samplers.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanImage> obj) { if (obj) { Images.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanImageView> obj) { if (obj) { ImageViews.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanFramebuffer> obj) { if (obj) { Framebuffers.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanAccelerationStructure> obj) { if (obj) { AccelStructs.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanDescriptorPool> obj) { if (obj) { DescriptorPools.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanDescriptorSet> obj) { if (obj) { Descriptors.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanCommandBuffer> obj) { if (obj) { CommandBuffers.push_back(std::move(obj)); } }
		void Add(std::unique_ptr<VulkanShader> obj) { if (obj) { Shaders.push_back(std::move(obj)); } }
	};

	std::unique_ptr<DeleteList> TransferDeleteList = std::make_unique<DeleteList>();
	std::unique_ptr<DeleteList> DrawDeleteList = std::make_unique<DeleteList>();

private:
	VulkanRenderDevice* fb = nullptr;
	std::unique_ptr<VulkanCommandPool> mCommandPool;
	std::unique_ptr<VulkanCommandBuffer> mTransferCommands;
	std::unique_ptr<VulkanCommandBuffer> mDrawCommands;
	std::unique_ptr<VulkanSemaphore> mImageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> mRenderFinishedSemaphore;
	std::unique_ptr<VulkanFence> mPresentFinishedFence;
};

struct ViewerPushConstants
{
	VSMatrix ViewToWorld;
	FVector3 CameraPos;
	float ProjX;
	FVector3 SunDir;
	float ProjY;
	FVector3 SunColor;
	float SunIntensity;
};

class VkTextureImage
{
public:
	void Reset(VulkanRenderDevice* fb)
	{
		auto deletelist = fb->GetCommands()->DrawDeleteList.get();
		for (auto& framebuffer : LMFramebuffers)
			deletelist->Add(std::move(framebuffer));
		LMFramebuffers.clear();
		for (auto& view : LMViews)
			deletelist->Add(std::move(view));
		LMViews.clear();
		deletelist->Add(std::move(Image));
	}

	std::unique_ptr<VulkanImage> Image;
	std::vector<std::unique_ptr<VulkanImageView>> LMViews;
	std::vector<std::unique_ptr<VulkanFramebuffer>> LMFramebuffers;
};

class VkTextureManager
{
public:
	VkTextureManager(VulkanRenderDevice* fb);

	void CreateLightmap(int newLMTextureSize, int newLMTextureCount);
	void DownloadLightmap(int arrayIndex, uint16_t* buffer);

	int CreateGameTexture(int width, int height, const void* pixels);

	VulkanImage* GetGameTexture(int index) { return GameTextures[index].get(); }
	VulkanImageView* GetGameTextureView(int index) { return GameTextureViews[index].get(); }

	VulkanImage* GetNullTexture() { return NullTexture.get(); }
	VulkanImageView* GetNullTextureView() { return NullTextureView.get(); }

	VkTextureImage Lightmap;
	int LMTextureSize = 0;
	int LMTextureCount = 0;

private:
	void CreateNullTexture();

	VulkanRenderDevice* fb = nullptr;

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;

	std::vector<std::unique_ptr<VulkanImage>> GameTextures;
	std::vector<std::unique_ptr<VulkanImageView>> GameTextureViews;
};

class VkSamplerManager
{
public:
	VkSamplerManager(VulkanRenderDevice* fb);

	VulkanSampler* Get() { return Sampler.get(); }

private:
	VulkanRenderDevice* fb = nullptr;
	std::unique_ptr<VulkanSampler> Sampler;
};

class VkDescriptorSetManager
{
public:
	VkDescriptorSetManager(VulkanRenderDevice* fb);

	VulkanDescriptorSetLayout* GetBindlessLayout() { return BindlessDescriptorSetLayout.get(); }
	VulkanDescriptorSet* GetBindlessSet() { return BindlessDescriptorSet.get(); }

	void UpdateBindlessDescriptorSet();
	int AddBindlessTextureIndex(VulkanImageView* imageview, VulkanSampler* sampler);

private:
	void CreateBindlessDescriptorSet();

	VulkanRenderDevice* fb = nullptr;

	std::unique_ptr<VulkanDescriptorPool> BindlessDescriptorPool;
	std::unique_ptr<VulkanDescriptorSet> BindlessDescriptorSet;
	std::unique_ptr<VulkanDescriptorSetLayout> BindlessDescriptorSetLayout;
	WriteDescriptors WriteBindless;
	int NextBindlessIndex = 0;

	static const int MaxBindlessTextures = 16536;
};

inline void I_FatalError(const char* reason) { throw std::runtime_error(reason); }
