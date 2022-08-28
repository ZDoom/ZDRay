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
#include "collision.h"

#include "dp_rect_pack/dp_rect_pack.h"

typedef dp::rect_pack::RectPacker<int> RectPacker;

struct MapSubsectorEx;
struct IntSector;
struct IntSideDef;
struct FLevel;
struct ThingLight;
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
	// Surface geometry
	SurfaceType type = ST_UNKNOWN;
	std::vector<vec3> verts;
	Plane plane;
	BBox bounds;

	// Surface material
	std::string material;
	std::vector<vec2> texUV;

	// Surface properties
	int typeIndex = 0;
	IntSector* controlSector = nullptr;
	int sampleDimension = 0;
	bool bSky = false;

	// Touching light sources
	std::vector<ThingLight*> LightList;

	// Lightmap world coordinates for the texture
	vec3 worldOrigin = { 0.0f };
	vec3 worldStepX = { 0.0f };
	vec3 worldStepY = { 0.0f };

	// Calculate world coordinates to UV coordinates
	vec3 translateWorldToLocal = { 0.0f };
	vec3 projLocalToU = { 0.0f };
	vec3 projLocalToV = { 0.0f };

	// Output lightmap for the surface
	int texWidth = 0;
	int texHeight = 0;
	std::vector<vec3> texPixels;

	// UV coordinates for the vertices
	std::vector<vec2> lightUV;

	// Placement in final texture atlas
	int atlasPageIndex = -1;
	int atlasX = 0;
	int atlasY = 0;

	// Smoothing group surface is to be rendered with
	int smoothingGroupIndex = -1;
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

	std::vector<Plane> smoothingGroups;

	int defaultSamples = 16;
	int textureWidth = 128;
	int textureHeight = 128;

	TArray<vec3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;

	std::unique_ptr<TriangleMeshShape> Collision;

private:
	void CreateSubsectorSurfaces(FLevel &doomMap);
	void CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side);
	void CreateLightProbes(FLevel& doomMap);

	void BuildSurfaceParams(Surface* surface);
	BBox GetBoundsFromSurface(const Surface* surface);
	void BlurSurfaces();
	void FinishSurface(RectPacker& packer, Surface* surface);

	static bool IsDegenerate(const vec3 &v0, const vec3 &v1, const vec3 &v2);
};
