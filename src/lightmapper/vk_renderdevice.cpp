
#include "vk_renderdevice.h"
#include "vk_levelmesh.h"
#include "vk_lightmapper.h"
#include "stacktrace.h"
#include <zvulkan/vulkanbuilders.h>
#include <zvulkan/vulkancompatibledevice.h>
#include <stdexcept>

extern bool VKDebug;
extern bool NoRtx;

void VulkanError(const char* text)
{
	throw std::runtime_error(text);
}

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	printf("   [%s] %s\n", typestr, msg.c_str());
	printf("   %s\n", CaptureStackTraceText(2).c_str());
}

VulkanRenderDevice::VulkanRenderDevice()
{
	auto instance = VulkanInstanceBuilder()
		.DebugLayer(VKDebug)
		.Create();

	device = VulkanDeviceBuilder()
		.OptionalRayQuery()
		.RequireExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
		.Create(instance);

	useRayQuery = !NoRtx && device->PhysicalDevice.Features.RayQuery.rayQuery;

	commands = std::make_unique<VkCommandBufferManager>(this);
	descriptors = std::make_unique<VkDescriptorSetManager>(this);
	textures = std::make_unique<VkTextureManager>(this);
	levelmesh = std::make_unique<VkLevelMesh>(this);
	lightmapper = std::make_unique<VkLightmapper>(this);
}

VulkanRenderDevice::~VulkanRenderDevice()
{
	vkDeviceWaitIdle(device->device);
}

/////////////////////////////////////////////////////////////////////////////

VkCommandBufferManager::VkCommandBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
	mCommandPool = CommandPoolBuilder()
		.QueueFamily(fb->GetDevice()->GraphicsFamily)
		.DebugName("mCommandPool")
		.Create(fb->GetDevice());
}

void VkCommandBufferManager::SubmitAndWait()
{
	if (mTransferCommands)
	{
		mTransferCommands->end();

		QueueSubmit()
			.AddCommandBuffer(mTransferCommands.get())
			.Execute(fb->GetDevice(), fb->GetDevice()->GraphicsQueue);

		TransferDeleteList->Add(std::move(mTransferCommands));

		vkDeviceWaitIdle(fb->GetDevice()->device);
	}

	TransferDeleteList = std::make_unique<DeleteList>();
	DrawDeleteList = std::make_unique<DeleteList>();
}

VulkanCommandBuffer* VkCommandBufferManager::GetTransferCommands()
{
	if (!mTransferCommands)
	{
		mTransferCommands = mCommandPool->createBuffer();
		mTransferCommands->begin();
	}
	return mTransferCommands.get();
}

/////////////////////////////////////////////////////////////////////////////

VkTextureManager::VkTextureManager(VulkanRenderDevice* fb) : fb(fb)
{
}

void VkTextureManager::CreateLightmap(int newLMTextureSize, int newLMTextureCount)
{
	if (LMTextureSize == newLMTextureSize && LMTextureCount == newLMTextureCount + 1)
		return;

	LMTextureSize = newLMTextureSize;
	LMTextureCount = newLMTextureCount + 1; // the extra texture is for the dynamic lightmap

	int w = newLMTextureSize;
	int h = newLMTextureSize;
	int count = newLMTextureCount;
	int pixelsize = 8;

	Lightmap.Reset(fb);

	Lightmap.Image = ImageBuilder()
		.Size(w, h, 1, LMTextureCount)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		.DebugName("VkRenderBuffers.Lightmap")
		.Create(fb->GetDevice());

	PipelineBarrier()
		.AddImage(Lightmap.Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, LMTextureCount)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkTextureManager::DownloadLightmap(int arrayIndex, uint16_t* buffer)
{
	unsigned int totalSize = LMTextureSize * LMTextureSize * 4;

	auto stagingBuffer = BufferBuilder()
		.Size(totalSize * sizeof(uint16_t))
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("DownloadLightmap")
		.Create(fb->GetDevice());

	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	PipelineBarrier()
		.AddImage(Lightmap.Image.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, arrayIndex, 1)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = arrayIndex;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;
	region.imageExtent.width = LMTextureSize;
	region.imageExtent.height = LMTextureSize;
	region.imageExtent.depth = 1;
	cmdbuffer->copyImageToBuffer(Lightmap.Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer->buffer, 1, &region);

	PipelineBarrier()
		.AddImage(Lightmap.Image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, arrayIndex, 1)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	fb->GetCommands()->SubmitAndWait();

	uint16_t* srcdata = (uint16_t*)stagingBuffer->Map(0, totalSize * sizeof(uint16_t));
	memcpy(buffer, srcdata, totalSize * sizeof(uint16_t));
	stagingBuffer->Unmap();
}

/////////////////////////////////////////////////////////////////////////////

VkDescriptorSetManager::VkDescriptorSetManager(VulkanRenderDevice* fb) : fb(fb)
{
	CreateBindlessDescriptorSet();
}

void VkDescriptorSetManager::CreateBindlessDescriptorSet()
{
	BindlessDescriptorPool = DescriptorPoolBuilder()
		.Flags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxBindlessTextures)
		.MaxSets(MaxBindlessTextures)
		.DebugName("BindlessDescriptorPool")
		.Create(fb->GetDevice());

	BindlessDescriptorSetLayout = DescriptorSetLayoutBuilder()
		.Flags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT)
		.AddBinding(
			0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			MaxBindlessTextures,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
		.DebugName("BindlessDescriptorSetLayout")
		.Create(fb->GetDevice());

	BindlessDescriptorSet = BindlessDescriptorPool->allocate(BindlessDescriptorSetLayout.get(), MaxBindlessTextures);
}

void VkDescriptorSetManager::UpdateBindlessDescriptorSet()
{
	WriteBindless.Execute(fb->GetDevice());
	WriteBindless = WriteDescriptors();
}

int VkDescriptorSetManager::AddBindlessTextureIndex(VulkanImageView* imageview, VulkanSampler* sampler)
{
	int index = NextBindlessIndex++;
	WriteBindless.AddCombinedImageSampler(BindlessDescriptorSet.get(), 0, index, imageview, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return index;
}
