//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//

#pragma once

#include "surfaces.h"
#include "framework/tarray.h"
#include <mutex>

#define LIGHTCELL_SIZE 64
#define LIGHTCELL_BLOCK_SIZE 16

class FWadWriter;
class SurfaceLight;

class LightCellBlock
{
public:
	int z;
	int layers;
	TArray<Vec3> cells;
};

class LightCellGrid
{
public:
	int x, y;
	int width, height;
	std::vector<LightCellBlock> blocks;
};

class LightmapTexture
{
public:
	LightmapTexture(int width, int height)
	{
		mPixels.resize(width * height * 3);
	}

	uint16_t *Pixels() { return mPixels.data(); }

private:
	std::vector<uint16_t> mPixels;
};

class LightmapBuilder
{
public:
	LightmapBuilder();
	~LightmapBuilder();

	void CreateLightmaps(FLevel &doomMap, int sampleDistance, int textureSize);
	void AddLightmapLump(FWadWriter &wadFile);

private:
	void NewTexture();
	bool MakeRoomForBlock(const int width, const int height, int *x, int *y, int *num);
	BBox GetBoundsFromSurface(const Surface *surface);
	Vec3 LightTexelSample(const Vec3 &origin, Surface *surface);
	bool EmitFromCeiling(const Surface *surface, const Vec3 &origin, const Vec3 &normal, Vec3 &color);

	void BuildSurfaceParams(Surface *surface);
	void TraceSurface(Surface *surface);
	void TraceIndirectLight(Surface *surface);
	void FinishSurface(Surface *surface);
	void SetupLightCellGrid();
	void LightBlock(int blockid);
	void LightSurface(const int surfid);
	void LightIndirect(const int surfid);

	void CreateSurfaceLights();

	FLevel *map;
	int samples = 16;
	int textureWidth = 128;
	int textureHeight = 128;

	std::unique_ptr<LevelMesh> mesh;
	std::vector<std::unique_ptr<SurfaceLight>> surfaceLights;
	std::vector<std::unique_ptr<LightmapTexture>> textures;
	std::vector<std::vector<int>> allocBlocks;
	int numTextures = 0;
	int extraSamples = 2;
	int tracedTexels = 0;

	LightCellGrid grid;

	std::mutex mutex;
	int processed = 0;
};
