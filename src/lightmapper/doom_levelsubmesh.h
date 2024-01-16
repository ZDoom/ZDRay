
#pragma once

#include "hw_levelmesh.h"
#include "framework/tarray.h"
#include "framework/vectors.h"
#include "framework/bounds.h"
#include "level/level.h"
#include <dp_rect_pack/dp_rect_pack.h>
#include <set>
#include <map>

typedef dp::rect_pack::RectPacker<int> RectPacker;

struct FLevel;
struct FPolyObj;
struct HWWallDispatcher;
class DoomLevelMesh;
class MeshBuilder;

enum DoomLevelMeshSurfaceType
{
	ST_NONE,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

struct DoomLevelMeshSurface : public LevelMeshSurface
{
	DoomLevelMeshSurfaceType Type = ST_NONE;
	int TypeIndex = 0;

	MapSubsectorEx* Subsector = nullptr;
	IntSideDef* Side = nullptr;
	IntSector* ControlSector = nullptr;

	int PipelineID = 0;

	std::vector<ThingLight*> LightList;
};

class DoomLevelSubmesh : public LevelSubmesh
{
public:
	DoomLevelSubmesh(DoomLevelMesh* mesh, FLevel& doomMap, bool staticMesh);

	void Update(FLevel& doomMap, int lightmapStartIndex);

	LevelMeshSurface* GetSurface(int index) override { return &Surfaces[index]; }
	unsigned int GetSurfaceIndex(const LevelMeshSurface* surface) const override { return (unsigned int)(ptrdiff_t)(static_cast<const DoomLevelMeshSurface*>(surface) - Surfaces.Data()); }
	int GetSurfaceCount() override { return Surfaces.Size(); }

	TArray<DoomLevelMeshSurface> Surfaces;

private:
	void Reset();

	void CreateStaticSurfaces(FLevel& doomMap);
	void CreateDynamicSurfaces(FLevel& doomMap);

	void CreateSideSurfaces(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void CreateLineHorizonSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void CreateFrontWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void CreateMidWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void Create3DFloorWallSurfaces(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void CreateTopWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void CreateBottomWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side);
	void SetSideTextureUVs(DoomLevelMeshSurface& surface, IntSideDef* side, WallPart texpart, float v1TopZ, float v1BottomZ, float v2TopZ, float v2BottomZ);
	void CreateFloorSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex);
	void CreateCeilingSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex);

	void AddWallVertices(DoomLevelMeshSurface& surf, FFlatVertex* verts);

	static bool IsTopSideSky(IntSector* frontsector, IntSector* backsector, IntSideDef* side);
	static bool IsTopSideVisible(IntSideDef* side);
	static bool IsBottomSideVisible(IntSideDef* side);
	static bool IsSkySector(IntSector* sector, SecPlaneType plane);
	static bool IsDegenerate(const FVector3& v0, const FVector3& v1, const FVector3& v2);

	static FVector4 ToPlane(const FFlatVertex& pt1, const FFlatVertex& pt2, const FFlatVertex& pt3)
	{
		return ToPlane(FVector3(pt1.x, pt1.y, pt1.z), FVector3(pt2.x, pt2.y, pt2.z), FVector3(pt3.x, pt3.y, pt3.z));
	}

	static FVector4 ToPlane(const FFlatVertex& pt1, const FFlatVertex& pt2, const FFlatVertex& pt3, const FFlatVertex& pt4)
	{
		return ToPlane(FVector3(pt1.x, pt1.y, pt1.z), FVector3(pt2.x, pt2.y, pt2.z), FVector3(pt3.x, pt3.y, pt3.z), FVector3(pt4.x, pt4.y, pt4.z));
	}

	static FVector4 ToPlane(const FVector3& pt1, const FVector3& pt2, const FVector3& pt3)
	{
		FVector3 n = ((pt2 - pt1) ^ (pt3 - pt2)).Unit();
		float d = pt1 | n;
		return FVector4(n.X, n.Y, n.Z, d);
	}

	static FVector4 ToPlane(const FVector3& pt1, const FVector3& pt2, const FVector3& pt3, const FVector3& pt4)
	{
		if (pt1.ApproximatelyEquals(pt3))
		{
			return ToPlane(pt1, pt2, pt4);
		}
		else if (pt1.ApproximatelyEquals(pt2) || pt2.ApproximatelyEquals(pt3))
		{
			return ToPlane(pt1, pt3, pt4);
		}

		return ToPlane(pt1, pt2, pt3);
	}

	void SortIndexes();

	void PackLightmapAtlas(FLevel& doomMap, int lightmapStartIndex);

	enum PlaneAxis
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	static PlaneAxis BestAxis(const FVector4& p);
	BBox GetBoundsFromSurface(const LevelMeshSurface& surface) const;

	void SetupTileTransform(int lightMapTextureWidth, int lightMapTextureHeight, LightmapTile& tile);
	void AddSurfaceToTile(DoomLevelMeshSurface& surf, std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, uint16_t sampleDimension);
	int GetSampleDimension(const DoomLevelMeshSurface& surf, uint16_t sampleDimension);

	DoomLevelMesh* LevelMesh = nullptr;
	bool StaticMesh = true;
};

static_assert(alignof(FVector2) == alignof(float[2]) && sizeof(FVector2) == sizeof(float) * 2);
