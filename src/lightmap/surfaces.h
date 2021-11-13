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

#include <vector>
#include <memory>
#include <string>
#include <cstring>

#include "framework/tarray.h"
#include "framework/halffloat.h"
#include "lightmap/collision.h"

struct MapSubsectorEx;
struct IntSector;
struct IntSideDef;
struct FLevel;
class FWadWriter;

enum SurfaceType
{
	ST_UNKNOWN,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

struct Surface
{
	Plane plane;
	int lightmapNum;
	int lightmapOffs[2];
	int lightmapDims[2];
	vec3 lightmapOrigin;
	vec3 lightmapSteps[2];
	vec3 textureCoords[2];
	BBox bounds;
	int numVerts;
	std::vector<vec3> verts;
	std::vector<float> lightmapCoords;
	std::vector<vec3> samples;
	std::vector<vec3> indirect;
	SurfaceType type;
	int typeIndex;
	IntSector *controlSector;
	bool bSky;
	std::vector<vec2> uvs;
	std::string material;
};

class LightmapTexture
{
public:
	LightmapTexture(int width, int height) : textureWidth(width), textureHeight(height)
	{
#ifdef _DEBUG
		mPixels.resize(width * height * 3, floatToHalf(0.5f));
#else
		mPixels.resize(width * height * 3, 0);
#endif
		allocBlocks.resize(height);
	}

	bool MakeRoomForBlock(const int width, const int height, int* x, int* y)
	{
		int startY = 0;
		int startX = 0;
		for (int i = 0; i < textureHeight; i++)
		{
			startX = std::max(startX, allocBlocks[i]);
			int available = textureWidth - startX;
			if (available < width)
			{
				startY = i + 1;
				startX = 0;
			}
			else if (i - startY + 1 == height)
			{
				for (int yy = 0; yy < height; yy++)
				{
					allocBlocks[startY + yy] = startX + width;
				}
				*x = startX;
				*y = startY;
				return true;
			}
		}
		return false;
	}

	int Width() const { return textureWidth; }
	int Height() const { return textureHeight; }
	uint16_t* Pixels() { return mPixels.data(); }

private:
	int textureWidth;
	int textureHeight;
	std::vector<uint16_t> mPixels;
	std::vector<int> allocBlocks;
};

class LightProbeSample
{
public:
	vec3 Position = vec3(0.0f, 0.0f, 0.0f);
	vec3 Color = vec3(0.0f, 0.0f, 0.0f);
};

class LevelMesh
{
public:
	LevelMesh(FLevel &doomMap, int sampleDistance, int textureSize);

	void CreateTextures();
	void AddLightmapLump(FWadWriter& wadFile);
	void Export(std::string filename);

	FLevel* map = nullptr;

	std::vector<std::unique_ptr<Surface>> surfaces;
	std::vector<LightProbeSample> lightProbes;

	std::vector<std::unique_ptr<LightmapTexture>> textures;

	int samples = 16;
	int textureWidth = 128;
	int textureHeight = 128;

	TArray<vec3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;

private:
	void CreateSubsectorSurfaces(FLevel &doomMap);
	void CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side);
	void CreateLightProbes(FLevel& doomMap);

	void BuildSurfaceParams(Surface* surface);
	BBox GetBoundsFromSurface(const Surface* surface);
	void FinishSurface(Surface* surface);
	uint16_t* AllocTextureRoom(Surface* surface, int* x, int* y);

	static bool IsDegenerate(const vec3 &v0, const vec3 &v1, const vec3 &v2);
};
