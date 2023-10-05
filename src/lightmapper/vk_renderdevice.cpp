
#include "vk_renderdevice.h"
#include "vk_raytrace.h"
#include "vk_lightmap.h"
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
	raytrace = std::make_unique<VkRaytrace>(this);
	lightmap = std::make_unique<VkLightmap>(this);
}

VulkanRenderDevice::~VulkanRenderDevice()
{
	vkDeviceWaitIdle(device->device);
}

/////////////////////////////////////////////////////////////////////////////

VkCommandBufferManager::VkCommandBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
}

void VkCommandBufferManager::SubmitAndWait()
{
}

VulkanCommandBuffer* VkCommandBufferManager::GetTransferCommands()
{
	return TransferCommands.get();
}

VulkanCommandBuffer* VkCommandBufferManager::GetDrawCommands()
{
	return DrawCommands.get();
}

/////////////////////////////////////////////////////////////////////////////

VkTextureManager::VkTextureManager(VulkanRenderDevice* fb) : fb(fb)
{
}

/////////////////////////////////////////////////////////////////////////////

VkDescriptorSetManager::VkDescriptorSetManager(VulkanRenderDevice* fb) : fb(fb)
{
}
