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
	Vec3 lightmapOrigin;
	Vec3 lightmapSteps[2];
	Vec3 textureCoords[2];
	BBox bounds;
	int numVerts;
	std::vector<Vec3> verts;
	std::vector<float> lightmapCoords;
	SurfaceType type;
	int typeIndex;
	IntSector *controlSector;
	bool bSky;
};

struct LevelTraceHit
{
	Vec3 start;
	Vec3 end;
	float fraction;

	Surface *hitSurface;
	int indices[3];
	float b, c;
};

class LevelMesh
{
public:
	LevelMesh(FLevel &doomMap);

	LevelTraceHit Trace(const Vec3 &startVec, const Vec3 &endVec);
	bool TraceAnyHit(const Vec3 &startVec, const Vec3 &endVec);

	void WriteMeshToOBJ();

	std::vector<std::unique_ptr<Surface>> surfaces;

	TArray<Vec3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;
	std::unique_ptr<TriangleMeshShape> CollisionMesh;

private:
	void CreateSubsectorSurfaces(FLevel &doomMap);
	void CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);

	void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side);

	static bool IsDegenerate(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2);
};
