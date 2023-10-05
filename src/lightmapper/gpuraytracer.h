#pragma once

#include "vk_renderdevice.h"

class DoomLevelMesh;

class GPURaytracer
{
public:
	GPURaytracer();
	~GPURaytracer();

	void Raytrace(DoomLevelMesh* levelMesh);

private:
	void PrintVulkanInfo();
	void LoadRenderDoc();

	std::unique_ptr<VulkanRenderDevice> mDevice;
};
