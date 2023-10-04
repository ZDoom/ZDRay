
#include "gpuraytracer.h"
#include "vk_renderdevice.h"

GPURaytracer::GPURaytracer()
{
	mDevice = std::make_unique<VulkanRenderDevice>();
}

GPURaytracer::~GPURaytracer()
{
}

void GPURaytracer::Raytrace(DoomLevelMesh* levelMesh)
{
}
