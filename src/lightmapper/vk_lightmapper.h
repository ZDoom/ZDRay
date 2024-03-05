#pragma once

#include "hw_levelmesh.h"
#include "zvulkan/vulkanobjects.h"
#include <dp_rect_pack/dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

class VulkanRenderDevice;
class FString;
class ShaderIncludeResult;

struct Uniforms
{
	FVector3 SunDir;
	float Padding1;
	FVector3 SunColor;
	float SunIntensity;
};

struct LightmapRaytracePC
{
	int32_t SurfaceIndex;
	int32_t Padding0;
	int32_t Padding1;
	int32_t Padding2;
	FVector3 WorldToLocal;
	float TextureSize;
	FVector3 ProjLocalToU;
	float Padding3;
	FVector3 ProjLocalToV;
	float Padding4;
	float TileX;
	float TileY;
	float TileWidth;
	float TileHeight;
};

struct LightmapCopyPC
{
	int SrcTexSize;
	int DestTexSize;
	int Padding1;
	int Padding2;
};

struct LightmapBakeImage
{
	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
		std::unique_ptr<VulkanDescriptorSet> DescriptorSet;
	} resolve;

	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
		std::unique_ptr<VulkanDescriptorSet> DescriptorSet[2];
	} blur;

	struct
	{
		std::unique_ptr<VulkanDescriptorSet> DescriptorSet;
	} copy;

	// how much of the image is used for the baking
	uint16_t maxX = 0;
	uint16_t maxY = 0;
};

struct SelectedTile
{
	LightmapTile* Tile = nullptr;
	int X = -1;
	int Y = -1;
	bool Rendered = false;
};

struct CopyTileInfo
{
	int SrcPosX;
	int SrcPosY;
	int DestPosX;
	int DestPosY;
	int TileWidth;
	int TileHeight;
	int Padding1;
	int Padding2;
};

static_assert(sizeof(CopyTileInfo) == sizeof(int32_t) * 8);

class VkLightmapper
{
public:
	VkLightmapper(VulkanRenderDevice* fb);
	~VkLightmapper();

	void BeginFrame();
	void Raytrace(const TArray<LightmapTile*>& surfaces);
	void SetLevelMesh(LevelMesh* level);

private:
	void ReleaseResources();

	void SelectTiles(const TArray<LightmapTile*>& surfaces);
	void UploadUniforms();
	void Render();
	void Resolve();
	void Blur();
	void CopyResult();

	void UpdateAccelStructDescriptors();

	void CreateShaders();
	void CreateRaytracePipeline();
	void CreateResolvePipeline();
	void CreateBlurPipeline();
	void CreateCopyPipeline();
	void CreateUniformBuffer();
	void CreateTileBuffer();
	void CreateDrawIndexedBuffer();
	void CreateBakeImage();

	int GetRaytracePipelineIndex();

	static FString LoadPrivateShaderLump(const char* lumpname);
	static FString LoadPublicShaderLump(const char* lumpname);
	static ShaderIncludeResult OnInclude(FString headerName, FString includerName, size_t depth, bool system);

	FVector3 SwapYZ(const FVector3& v) { return FVector3(v.X, v.Z, v.Y); }

	VulkanRenderDevice* fb = nullptr;
	LevelMesh* mesh = nullptr;

	bool useRayQuery = true;

	TArray<SelectedTile> selectedTiles;
	TArray<TArray<SelectedTile*>> copylists;

	struct
	{
		std::unique_ptr<VulkanBuffer> Buffer;
		std::unique_ptr<VulkanBuffer> TransferBuffer;

		uint8_t* Uniforms = nullptr;
		int Index = 0;
		int NumStructs = 256;
		VkDeviceSize StructStride = sizeof(Uniforms);
	} uniforms;

	struct
	{
		const int BufferSize = 100'000;
		std::unique_ptr<VulkanBuffer> Buffer;
		CopyTileInfo* Tiles = nullptr;
	} copytiles;

	struct
	{
		const int BufferSize = 100'000;
		std::unique_ptr<VulkanBuffer> CommandsBuffer;
		std::unique_ptr<VulkanBuffer> ConstantsBuffer;
		VkDrawIndexedIndirectCommand* Commands = nullptr;
		LightmapRaytracePC* Constants = nullptr;
		int Pos = 0;
	} drawindexed;

	struct
	{
		std::unique_ptr<VulkanShader> vertRaytrace;
		std::unique_ptr<VulkanShader> vertScreenquad;
		std::unique_ptr<VulkanShader> vertCopy;
		std::unique_ptr<VulkanShader> fragRaytrace[16];
		std::unique_ptr<VulkanShader> fragResolve;
		std::unique_ptr<VulkanShader> fragBlur[2];
		std::unique_ptr<VulkanShader> fragCopy;
	} shaders;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout0;
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout1;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline[16];
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool0;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool1;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet0;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet1;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::unique_ptr<VulkanSampler> sampler;
	} resolve;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline[2];
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::unique_ptr<VulkanSampler> sampler;
	} blur;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::unique_ptr<VulkanSampler> sampler;
	} copy;

	LightmapBakeImage bakeImage;
	static const int bakeImageSize = 2048;
};
