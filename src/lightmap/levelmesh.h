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
#include "lightmaptexture.h"
#include "math/mathlib.h"

#include "dp_rect_pack/dp_rect_pack.h"

typedef dp::rect_pack::RectPacker<int> RectPacker;

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
	std::vector<vec2> lightmapCoords;
	//std::vector<bool> coveragemask;
	std::vector<vec3> samples;
	SurfaceType type;
	int typeIndex;
	IntSector *controlSector;
	bool bSky;
	std::vector<vec2> uvs;
	std::string material;
	int sampleDimension;
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

	int defaultSamples = 16;
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
	void FinishSurface(RectPacker& packer, Surface* surface);

	static bool IsDegenerate(const vec3 &v0, const vec3 &v1, const vec3 &v2);
};
