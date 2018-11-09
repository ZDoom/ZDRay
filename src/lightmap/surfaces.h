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

#include "framework/tarray.h"
#include "lightmap/collision.h"

struct MapSubsectorEx;
struct IntSector;
struct IntSideDef;
struct FLevel;

enum surfaceType_t
{
	ST_UNKNOWN,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

// convert from fixed point(FRACUNIT) to floating point
#define F(x)  (((float)(x))/65536.0f)

struct surface_t
{
	kexPlane plane;
	int lightmapNum;
	int lightmapOffs[2];
	int lightmapDims[2];
	kexVec3 lightmapOrigin;
	kexVec3 lightmapSteps[2];
	kexVec3 textureCoords[2];
	kexBBox bounds;
	int numVerts;
	std::vector<kexVec3> verts;
	std::vector<float> lightmapCoords;
	surfaceType_t type;
	int typeIndex;
	IntSector *controlSector;
	bool bSky;
};

struct LevelTraceHit
{
	kexVec3 start;
	kexVec3 end;
	float fraction;

	surface_t *hitSurface;
	int indices[3];
	float b, c;
};

class LevelMesh
{
public:
	LevelMesh(FLevel &doomMap);

	LevelTraceHit Trace(const kexVec3 &startVec, const kexVec3 &endVec);
	bool TraceAnyHit(const kexVec3 &startVec, const kexVec3 &endVec);

	void WriteMeshToOBJ();

	std::vector<std::unique_ptr<surface_t>> surfaces;

	TArray<kexVec3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;
	std::unique_ptr<TriangleMeshShape> CollisionMesh;

private:
	void CreateSubsectorSurfaces(FLevel &doomMap);
	void CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);

	void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side);

	static bool IsDegenerate(const kexVec3 &v0, const kexVec3 &v1, const kexVec3 &v2);
};
