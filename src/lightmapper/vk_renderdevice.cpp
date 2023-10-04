
#include "vk_renderdevice.h"
#include <stdexcept>

void VulkanError(const char* text)
{
	throw std::runtime_error(text);
}

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	printf("[%s] ", typestr);
	printf("%s\n", msg.c_str());
}

VulkanRenderDevice::VulkanRenderDevice()
{
}

VulkanRenderDevice::~VulkanRenderDevice()
{
}
