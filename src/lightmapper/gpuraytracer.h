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
	std::unique_ptr<VulkanRenderDevice> mDevice;
};
