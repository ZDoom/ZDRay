#pragma once

#include "framework/zstring.h"
#include "hw_levelmesh.h"
#include "hw_lightmaptile.h"
#include "level/doomdata.h"
#include <map>

struct FLevel;
class FWadWriter;
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
};

struct SideSurfaceRange
{
	int StartSurface = 0;
	int SurfaceCount = 0;
};

struct FlatSurfaceRange
{
	int StartSurface = 0;
	int SurfaceCount = 0;
};

class DoomLevelMesh : public LevelMesh
{
public:
	DoomLevelMesh(FLevel& doomMap);

	LevelMeshSurface* GetSurface(int index) override { return &Surfaces[index]; }
	unsigned int GetSurfaceIndex(const LevelMeshSurface* surface) const override { return (unsigned int)(ptrdiff_t)(static_cast<const DoomLevelMeshSurface*>(surface) - Surfaces.Data()); }
	int GetSurfaceCount() override { return Surfaces.Size(); }

	void BeginFrame(FLevel& doomMap);
	bool TraceSky(const FVector3& start, FVector3 direction, float dist);
	void DumpMesh(const FString& objFilename, const FString& mtlFilename) const;

	void BuildSectorGroups(const FLevel& doomMap);

	void AddLightmapLump(FLevel& doomMap, FWadWriter& wadFile);

	TArray<DoomLevelMeshSurface> Surfaces;

	TArray<int> sectorGroup; // index is sector, value is sectorGroup
	TArray<int> sectorPortals[2]; // index is sector+plane, value is index into the portal list
	TArray<int> linePortals; // index is linedef, value is index into the portal list

	void CreateLights(FLevel& doomMap);

private:
	void CreateSurfaces(FLevel& doomMap);

	void CreateSideSurfaces(FLevel& doomMap, IntSideDef* side);
	void CreateLineHorizonSurface(FLevel& doomMap, IntSideDef* side);
	void CreateFrontWallSurface(FLevel& doomMap, IntSideDef* side);
	void CreateMidWallSurface(FLevel& doomMap, IntSideDef* side);
	void Create3DFloorWallSurfaces(FLevel& doomMap, IntSideDef* side);
	void CreateTopWallSurface(FLevel& doomMap, IntSideDef* side);
	void CreateBottomWallSurface(FLevel& doomMap, IntSideDef* side);
	void AddWallVertices(DoomLevelMeshSurface& surf, FFlatVertex* verts);
	void SetSideTextureUVs(DoomLevelMeshSurface& surface, IntSideDef* side, WallPart texpart, float v1TopZ, float v1BottomZ, float v2TopZ, float v2BottomZ);

	void CreateFloorSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, X3DFloor* controlSector, int typeIndex);
	void CreateCeilingSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, X3DFloor* controlSector, int typeIndex);

	void AddSurfaceToTile(DoomLevelMeshSurface& surf, FLevel& doomMap, uint16_t sampleDimension);
	int GetSampleDimension(const DoomLevelMeshSurface& surf, uint16_t sampleDimension);

	static bool IsTopSideSky(IntSector* frontsector, IntSector* backsector, IntSideDef* side);
	static bool IsTopSideVisible(IntSideDef* side);
	static bool IsBottomSideVisible(IntSideDef* side);
	static bool IsSkySector(IntSector* sector, SecPlaneType plane);
	static bool IsDegenerate(const FVector3& v0, const FVector3& v1, const FVector3& v2);

	void SortIndexes();

	BBox GetBoundsFromSurface(const LevelMeshSurface& surface) const;

	int AddSurfaceToTile(const DoomLevelMeshSurface& surf);
	int GetSampleDimension(const DoomLevelMeshSurface& surf);

	void CreatePortals(FLevel& doomMap);

	void PropagateLight(FLevel& doomMap, ThingLight* light, int recursiveDepth);
	int GetLightIndex(ThingLight* light, int portalgroup);

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

	TArray<SideSurfaceRange> Sides;
	TArray<FlatSurfaceRange> Flats;
	std::map<LightmapTileBinding, int> bindings;
};
