#pragma once

#include "framework/zstring.h"
#include "zvulkan/vulkanobjects.h"
#include "textureid.h"
#include <stdexcept>

class VkRaytrace;
class VkLightmap;
class VkCommandBufferManager;
class VkDescriptorSetManager;
class VkTextureManager;

class VulkanRenderDevice
{
public:
	VulkanRenderDevice();
	~VulkanRenderDevice();

	VulkanDevice* GetDevice() { return nullptr; }
	VkCommandBufferManager* GetCommands() { return nullptr; }
	VkDescriptorSetManager* GetDescriptorSetManager() { return nullptr; }
	VkTextureManager* GetTextureManager() { return nullptr; }
	VkRaytrace* GetRaytrace() { return nullptr; }
	VkLightmap* GetLightmap() { return nullptr; }

	int GetBindlessTextureIndex(FTextureID texture) { return -1; }
};

class VkCommandBufferManager
{
public:
	VulkanCommandBuffer* GetTransferCommands() { return nullptr; }
	VulkanCommandBuffer* GetDrawCommands() { return nullptr; }

	void PushGroup(VulkanCommandBuffer* cmdbuffer, const FString& name) { }
	void PopGroup(VulkanCommandBuffer* cmdbuffer) { }

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
	VkTextureImage Lightmap;
	int LMTextureSize = 1024;
};

class VkDescriptorSetManager
{
public:
	VulkanDescriptorSetLayout* GetBindlessSetLayout() { return nullptr; }
	VulkanDescriptorSet* GetBindlessDescriptorSet() { return nullptr; }
};

inline void I_FatalError(const char* reason) { throw std::runtime_error(reason); }
