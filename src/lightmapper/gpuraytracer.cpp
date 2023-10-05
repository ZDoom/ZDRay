
#include "gpuraytracer.h"
#include "vk_renderdevice.h"
#include "renderdoc_app.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

static RENDERDOC_API_1_4_2* rdoc_api;

GPURaytracer::GPURaytracer()
{
	LoadRenderDoc();
	mDevice = std::make_unique<VulkanRenderDevice>();
	PrintVulkanInfo();
}

GPURaytracer::~GPURaytracer()
{
}

void GPURaytracer::Raytrace(DoomLevelMesh* levelMesh)
{
	if (rdoc_api) rdoc_api->StartFrameCapture(nullptr, nullptr);

	// To do raytrace stuff here!

	if (rdoc_api) rdoc_api->EndFrameCapture(nullptr, nullptr);
}

void GPURaytracer::PrintVulkanInfo()
{
	const auto& props = mDevice->GetDevice()->PhysicalDevice.Properties.Properties;

	std::string deviceType;
	switch (props.deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
	default: deviceType = std::to_string(props.deviceType); break;
	}

	std::string apiVersion = std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.apiVersion));
	std::string driverVersion = std::to_string(VK_VERSION_MAJOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.driverVersion));

	printf("   Vulkan device: %s\n", props.deviceName);
	printf("   Vulkan device type: %s\n", deviceType.c_str());
	printf("   Vulkan version: %s (api) %s (driver)\n", apiVersion.c_str(), driverVersion.c_str());
}

void GPURaytracer::LoadRenderDoc()
{
	if (rdoc_api)
		return;

#ifdef _WIN32
	if (auto mod = GetModuleHandleA("renderdoc.dll"))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_2, (void**)&rdoc_api);
		assert(ret == 1);

		if (ret != 1)
		{
			printf("   RENDERDOC_GetAPI returned %d\n", ret);
		}
	}
#else
	if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_2, (void**)&rdoc_api);
		assert(ret == 1);

		if (ret != 1)
		{
			printf("   RENDERDOC_GetAPI returned %d\n", ret);
		}
	}
#endif

	if (rdoc_api)
	{
		printf("   RenderDoc enabled\n");
	}
}
