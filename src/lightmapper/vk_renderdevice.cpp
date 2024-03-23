
#include "vk_renderdevice.h"
#include "vk_levelmesh.h"
#include "vk_lightmapper.h"
#include "stacktrace.h"
#include "levelmeshviewer.h"
#include "framework/matrix.h"
#include "glsl/vert_viewer.glsl.h"
#include "glsl/frag_viewer.glsl.h"
#include "glsl/binding_viewer.glsl.h"
#include "glsl/polyfill_rayquery.glsl.h"
#include "glsl/montecarlo.glsl.h"
#include "glsl/trace_ambient_occlusion.glsl.h"
#include "glsl/trace_bounce.glsl.h"
#include "glsl/trace_levelmesh.glsl.h"
#include "glsl/trace_light.glsl.h"
#include "glsl/trace_sunlight.glsl.h"
#include <zvulkan/vulkanbuilders.h>
#include <zvulkan/vulkancompatibledevice.h>
#include <zvulkan/vulkanswapchain.h>
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

VulkanRenderDevice::VulkanRenderDevice(LevelMeshViewer* viewer)
{
	if (viewer)
	{
#ifdef WIN32
		auto instance = VulkanInstanceBuilder()
			.RequireSurfaceExtensions()
			.DebugLayer(VKDebug)
			.Create();

		auto surface = VulkanSurfaceBuilder()
			.Win32Window(viewer->GetWindowHandle())
			.Create(instance);

		device = VulkanDeviceBuilder()
			.Surface(surface)
			.OptionalRayQuery()
			.RequireExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.Create(instance);

		swapchain = VulkanSwapChainBuilder()
			.Create(device.get());
#else
		VulkanError("No viewer supported on this platform");
#endif
	}
	else
	{
		auto instance = VulkanInstanceBuilder()
			.DebugLayer(VKDebug)
			.Create();

		device = VulkanDeviceBuilder()
			.OptionalRayQuery()
			.RequireExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.Create(instance);
	}

	useRayQuery = !NoRtx && device->PhysicalDevice.Features.RayQuery.rayQuery;

	commands = std::make_unique<VkCommandBufferManager>(this);
	descriptors = std::make_unique<VkDescriptorSetManager>(this);
	samplers = std::make_unique<VkSamplerManager>(this);
	textures = std::make_unique<VkTextureManager>(this);
	levelmesh = std::make_unique<VkLevelMesh>(this);
	lightmapper = std::make_unique<VkLightmapper>(this);

	descriptors->AddBindlessTextureIndex(GetTextureManager()->GetNullTextureView(), GetSamplerManager()->Get());
	descriptors->UpdateBindlessDescriptorSet();

	CreateViewerObjects();
}

VulkanRenderDevice::~VulkanRenderDevice()
{
	vkDeviceWaitIdle(device->device);
}

int VulkanRenderDevice::GetBindlessTextureIndex(FTextureID textureID)
{
	if (!textureID.isValid())
		return 0;

	FGameTexture* tex = TexMan.GetGameTexture(textureID);
	int& textureIndex = TextureIndexes[tex];
	if (textureIndex != 0)
		return textureIndex;

	// To do: upload image

	VulkanImageView* view = GetTextureManager()->GetNullTextureView();
	VulkanSampler* sampler = GetSamplerManager()->Get();

	textureIndex = GetDescriptorSetManager()->AddBindlessTextureIndex(view, sampler);
	return textureIndex;
}

void VulkanRenderDevice::DrawViewer(const FVector3& cameraPos, const VSMatrix& viewToWorld, float fovy, float aspect, const FVector3& sundir, const FVector3& suncolor, float sunintensity)
{
	int imageIndex = GetCommands()->AcquireImage();
	if (imageIndex < 0)
	{
		return;
	}

	WriteDescriptors write;
	write.AddBuffer(Viewer.DescriptorSet.get(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetSurfaceIndexBuffer());
	write.AddBuffer(Viewer.DescriptorSet.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetSurfaceBuffer());
	write.AddBuffer(Viewer.DescriptorSet.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetLightBuffer());
	write.AddBuffer(Viewer.DescriptorSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetLightIndexBuffer());
	write.AddBuffer(Viewer.DescriptorSet.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetPortalBuffer());
	if (useRayQuery)
	{
		write.AddAccelerationStructure(Viewer.DescriptorSet.get(), 5, GetLevelMesh()->GetAccelStruct());
	}
	else
	{
		write.AddBuffer(Viewer.DescriptorSet.get(), 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetNodeBuffer());
	}
	write.AddBuffer(Viewer.DescriptorSet.get(), 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetVertexBuffer());
	write.AddBuffer(Viewer.DescriptorSet.get(), 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GetLevelMesh()->GetIndexBuffer());
	write.Execute(device.get());

	auto commands = GetCommands()->GetDrawCommands();

	RenderPassBegin()
		.RenderPass(Viewer.RenderPass.get())
		.Framebuffer(Framebuffers[imageIndex].get())
		.RenderArea(0, 0, CurrentWidth, CurrentHeight)
		.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
		.AddClearDepth(1.0f)
		.Execute(commands);

	VkViewport viewport = {};
	viewport.width = (float)CurrentWidth;
	viewport.height = (float)CurrentHeight;
	viewport.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent.width = CurrentWidth;
	scissor.extent.height = CurrentHeight;
	commands->setScissor(0, 1, &scissor);

	float f = 1.0f / std::tan(fovy * (pi::pif() / 360.0f));

	ViewerPushConstants pushconstants;
	pushconstants.ViewToWorld = viewToWorld;
	pushconstants.CameraPos = cameraPos;
	pushconstants.ProjX = f / aspect;
	pushconstants.ProjY = f;
	pushconstants.SunDir = sundir;
	pushconstants.SunColor = suncolor;
	pushconstants.SunIntensity = sunintensity;

	commands->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, Viewer.PipelineLayout.get(), 0, Viewer.DescriptorSet.get());
	commands->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, Viewer.PipelineLayout.get(), 1, descriptors->GetBindlessSet());
	commands->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Viewer.Pipeline.get());
	commands->pushConstants(Viewer.PipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ViewerPushConstants), &pushconstants);
	commands->draw(4, 1, 0, 0);
	commands->endRenderPass();

	GetCommands()->SubmitAndWait(imageIndex);
}

static FString LoadPrivateShaderLump(const char* lumpname)
{
	static std::map<FString, FString> sources =
	{
		{ "shaders/lightmap/binding_viewer.glsl", binding_viewer_glsl },
		{ "shaders/lightmap/montecarlo.glsl", montecarlo_glsl },
		{ "shaders/lightmap/polyfill_rayquery.glsl", polyfill_rayquery_glsl },
		{ "shaders/lightmap/trace_ambient_occlusion.glsl", trace_ambient_occlusion_glsl },
		{ "shaders/lightmap/trace_bounce.glsl", trace_bounce_glsl },
		{ "shaders/lightmap/trace_levelmesh.glsl", trace_levelmesh_glsl },
		{ "shaders/lightmap/trace_light.glsl", trace_light_glsl },
		{ "shaders/lightmap/trace_sunlight.glsl", trace_sunlight_glsl },
	};

	auto it = sources.find(lumpname);
	if (it != sources.end())
		return it->second;
	else
		return FString();
}

static FString LoadPublicShaderLump(const char* lumpname)
{
	return LoadPrivateShaderLump(lumpname);
}

static ShaderIncludeResult OnInclude(FString headerName, FString includerName, size_t depth, bool system)
{
	if (depth > 8)
	{
		return ShaderIncludeResult("Too much include recursion!");
	}

	FString includeguardname;
	includeguardname << "_HEADERGUARD_" << headerName.GetChars();
	includeguardname.ReplaceChars("/\\.", '_');

	FString code;
	code << "#ifndef " << includeguardname.GetChars() << "\n";
	code << "#define " << includeguardname.GetChars() << "\n";
	code << "#line 1\n";

	if (system)
		code << LoadPrivateShaderLump(headerName.GetChars()).GetChars() << "\n";
	else
		code << LoadPublicShaderLump(headerName.GetChars()).GetChars() << "\n";

	code << "#endif\n";

	return ShaderIncludeResult(headerName.GetChars(), code.GetChars());
}

void VulkanRenderDevice::CreateViewerObjects()
{
	DescriptorSetLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (useRayQuery)
	{
		builder.AddBinding(5, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	else
	{
		builder.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.DebugName("Viewer.DescriptorSetLayout");
	Viewer.DescriptorSetLayout = builder.Create(device.get());

	Viewer.DescriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6)
		.MaxSets(1)
		.DebugName("Viewer.DescriptorPool")
		.Create(device.get());

	Viewer.DescriptorSet = Viewer.DescriptorPool->allocate(Viewer.DescriptorSetLayout.get());
	Viewer.DescriptorSet->SetDebugName("raytrace.descriptorSet1");

	std::string versionBlock = R"(
			#version 460
			#extension GL_GOOGLE_include_directive : enable
			#extension GL_EXT_nonuniform_qualifier : enable
		)";

	if (useRayQuery)
	{
		versionBlock += "#extension GL_EXT_ray_query : require\r\n";
		versionBlock += "#define USE_RAYQUERY\r\n";
	}

	auto onIncludeLocal = [](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, false); };
	auto onIncludeSystem = [](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, true); };

	Viewer.VertexShader = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("versionblock", versionBlock)
		.AddSource("vert_viewer.glsl", vert_viewer_glsl)
		.OnIncludeLocal(onIncludeLocal)
		.OnIncludeSystem(onIncludeSystem)
		.DebugName("Viewer.VertexShader")
		.Create("vertex", device.get());

	Viewer.FragmentShader = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("versionblock", versionBlock)
		.AddSource("frag_viewer.glsl", frag_viewer_glsl)
		.OnIncludeLocal(onIncludeLocal)
		.OnIncludeSystem(onIncludeSystem)
		.DebugName("Viewer.FragmentShader")
		.Create("vertex", device.get());

	Viewer.PipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(Viewer.DescriptorSetLayout.get())
		.AddSetLayout(descriptors->GetBindlessLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ViewerPushConstants))
		.DebugName("Viewer.PipelineLayout")
		.Create(device.get());

	Viewer.RenderPass = RenderPassBuilder()
		.AddAttachment(VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.Create(device.get());

	Viewer.Pipeline = GraphicsPipelineBuilder()
		.RenderPass(Viewer.RenderPass.get())
		.Layout(Viewer.PipelineLayout.get())
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		.AddVertexShader(Viewer.VertexShader.get())
		.AddFragmentShader(Viewer.FragmentShader.get())
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.DebugName("Viewer.Pipeline")
		.Create(device.get());
}

void VulkanRenderDevice::ResizeSwapChain(int width, int height)
{
	if (width <= 0 || height <= 0 || (width == CurrentWidth && height == CurrentHeight && swapchain->Lost()))
		return;

	CurrentWidth = width;
	CurrentHeight = height;
	Framebuffers.clear();

	swapchain->Create(width, height, 1, true, false, false);

	for (int imageIndex = 0; imageIndex < swapchain->ImageCount(); imageIndex++)
	{
		Framebuffers.push_back(FramebufferBuilder()
			.RenderPass(Viewer.RenderPass.get())
			.AddAttachment(swapchain->GetImageView(imageIndex))
			.Size(width, height)
			.DebugName("framebuffer")
			.Create(GetDevice()));
	}
}

/////////////////////////////////////////////////////////////////////////////

VkCommandBufferManager::VkCommandBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
	mCommandPool = CommandPoolBuilder()
		.QueueFamily(fb->GetDevice()->GraphicsFamily)
		.DebugName("mCommandPool")
		.Create(fb->GetDevice());

	mImageAvailableSemaphore = SemaphoreBuilder()
		.DebugName("mImageAvailableSemaphore")
		.Create(fb->GetDevice());

	mRenderFinishedSemaphore = SemaphoreBuilder()
		.DebugName("mRenderFinishedSemaphore")
		.Create(fb->GetDevice());

	mPresentFinishedFence = FenceBuilder()
		.DebugName("mPresentFinishedFence")
		.Create(fb->GetDevice());
}

int VkCommandBufferManager::AcquireImage()
{
	return fb->GetSwapChain()->AcquireImage(mImageAvailableSemaphore.get());
}

void VkCommandBufferManager::SubmitAndWait(int imageIndex)
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

	if (imageIndex >= 0 && mDrawCommands)
	{
		mDrawCommands->end();

		QueueSubmit()
			.AddCommandBuffer(mDrawCommands.get())
			.AddWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, mImageAvailableSemaphore.get())
			.AddSignal(mRenderFinishedSemaphore.get())
			.Execute(fb->GetDevice(), fb->GetDevice()->GraphicsQueue, mPresentFinishedFence.get());

		DrawDeleteList->Add(std::move(mDrawCommands));

		fb->GetSwapChain()->QueuePresent(imageIndex, mRenderFinishedSemaphore.get());

		vkWaitForFences(fb->GetDevice()->device, 1, &mPresentFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(fb->GetDevice()->device, 1, &mPresentFinishedFence->fence);
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

VulkanCommandBuffer* VkCommandBufferManager::GetDrawCommands()
{
	if (!mDrawCommands)
	{
		mDrawCommands = mCommandPool->createBuffer();
		mDrawCommands->begin();
	}
	return mDrawCommands.get();
}

/////////////////////////////////////////////////////////////////////////////

VkTextureManager::VkTextureManager(VulkanRenderDevice* fb) : fb(fb)
{
	CreateNullTexture();
}

void VkTextureManager::CreateNullTexture()
{
	NullTexture = ImageBuilder()
		.Format(VK_FORMAT_R8G8B8A8_UNORM)
		.Size(1, 1)
		.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.DebugName("VkTextureManager.NullTexture")
		.Create(fb->GetDevice());

	NullTextureView = ImageViewBuilder()
		.Image(NullTexture.get(), VK_FORMAT_R8G8B8A8_UNORM)
		.DebugName("VkTextureManager.NullTextureView")
		.Create(fb->GetDevice());

	auto stagingBuffer = BufferBuilder()
		.Size(4)
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("VkTextureManager.NullTextureStaging")
		.Create(fb->GetDevice());

	// Put white in the texture
	uint32_t* data = (uint32_t*)stagingBuffer->Map(0, 4);
	*data = 0xffffffff;
	stagingBuffer->Unmap();

	PipelineBarrier()
		.AddImage(NullTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.depth = 1;
	region.imageExtent.width = 1;
	region.imageExtent.height = 1;
	fb->GetCommands()->GetTransferCommands()->copyBufferToImage(stagingBuffer->buffer, NullTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	fb->GetCommands()->TransferDeleteList->Add(std::move(stagingBuffer));

	PipelineBarrier()
		.AddImage(NullTexture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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

VkSamplerManager::VkSamplerManager(VulkanRenderDevice* fb) : fb(fb)
{
	Sampler = SamplerBuilder()
		.MagFilter(VK_FILTER_NEAREST)
		.MinFilter(VK_FILTER_NEAREST)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.MaxLod(0.25f)
		.DebugName("VkSamplerManager.Sampler")
		.Create(fb->GetDevice());
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

/////////////////////////////////////////////////////////////////////////////

