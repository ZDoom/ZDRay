
#include "doom_levelsubmesh.h"
#include "level/level.h"
#include "framework/halffloat.h"
#include <unordered_map>

extern float lm_scale;

DoomLevelSubmesh::DoomLevelSubmesh(DoomLevelMesh* mesh, FLevel& doomMap, bool staticMesh) : LevelMesh(mesh), StaticMesh(staticMesh)
{
	LightmapSampleDistance = doomMap.DefaultSamples;
	Reset();

	if (StaticMesh)
	{
		CreateStaticSurfaces(doomMap);

		SortIndexes();
		BuildTileSurfaceLists();
		UpdateCollision();
		PackLightmapAtlas(doomMap, 0);
	}
}

void DoomLevelSubmesh::Update(FLevel& doomMap, int lightmapStartIndex)
{
	if (!StaticMesh)
	{
		Reset();

		CreateDynamicSurfaces(doomMap);

		SortIndexes();
		BuildTileSurfaceLists();
		UpdateCollision();
		PackLightmapAtlas(doomMap, lightmapStartIndex);
	}
}

void DoomLevelSubmesh::Reset()
{
	Surfaces.Clear();
	Mesh.Vertices.Clear();
	Mesh.Indexes.Clear();
	Mesh.SurfaceIndexes.Clear();
	Mesh.UniformIndexes.Clear();
	Mesh.Uniforms.Clear();
	Mesh.Materials.Clear();
}

void DoomLevelSubmesh::CreateStaticSurfaces(FLevel& doomMap)
{
	std::map<LightmapTileBinding, int> bindings;

	// Create surface objects for all sides
	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(bindings, doomMap, &doomMap.Sides[i]);
	}

	// Create surfaces for all flats
	for (int i = 0; i < doomMap.NumGLSubsectors; i++)
	{
		MapSubsectorEx* sub = &doomMap.GLSubsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		IntSector* sector = sub->GetSector(doomMap);
		if (!sector)
			continue;

		CreateFloorSurface(bindings, doomMap, sub, sector, nullptr, i);
		CreateCeilingSurface(bindings, doomMap, sub, sector, nullptr, i);

		for (unsigned int j = 0; j < sector->x3dfloors.Size(); j++)
		{
			CreateFloorSurface(bindings, doomMap, sub, sector, sector->x3dfloors[j], i);
			CreateCeilingSurface(bindings, doomMap, sub, sector, sector->x3dfloors[j], i);
		}
	}

	for (auto& tile : LightmapTiles)
	{
		SetupTileTransform(LMTextureSize, LMTextureSize, tile);
	}
}

void DoomLevelSubmesh::CreateSideSurfaces(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	/*if (side->line->getPortal() && side->line->frontsector == front)
	{
		CreateLinePortalSurface(bindings, doomMap, side);
	}
	else*/ if (side->line->special == Line_Horizon && front != back)
	{
		CreateLineHorizonSurface(bindings, doomMap, side);
	}
	else if (!back)
	{
		if (side->GetTexture(WallPart::MIDDLE).isValid())
		{
			CreateFrontWallSurface(bindings, doomMap, side);
		}
	}
	else
	{
		if (side->GetTexture(WallPart::MIDDLE).isValid())
		{
			CreateMidWallSurface(bindings, doomMap, side);
		}

		Create3DFloorWallSurfaces(bindings, doomMap, side);

		float v1TopBack = (float)back->ceilingplane.ZatPoint(v1);
		float v1BottomBack = (float)back->floorplane.ZatPoint(v1);
		float v2TopBack = (float)back->ceilingplane.ZatPoint(v2);
		float v2BottomBack = (float)back->floorplane.ZatPoint(v2);

		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			CreateBottomWallSurface(bindings, doomMap, side);
		}

		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			CreateTopWallSurface(bindings, doomMap, side);
		}
	}
}

void DoomLevelSubmesh::AddWallVertices(DoomLevelMeshSurface& surf, FFlatVertex* verts)
{
	surf.MeshLocation.StartVertIndex = Mesh.Vertices.Size();
	surf.MeshLocation.StartElementIndex = Mesh.Indexes.Size();
	surf.MeshLocation.NumVerts = 4;
	surf.MeshLocation.NumElements = 6;
	surf.Plane = ToPlane(verts[2], verts[0], verts[3], verts[1]);

	Mesh.Vertices.Push(verts[0]);
	Mesh.Vertices.Push(verts[1]);
	Mesh.Vertices.Push(verts[2]);
	Mesh.Vertices.Push(verts[3]);

	unsigned int startVertIndex = surf.MeshLocation.StartVertIndex;
	Mesh.Indexes.Push(startVertIndex + 0);
	Mesh.Indexes.Push(startVertIndex + 1);
	Mesh.Indexes.Push(startVertIndex + 2);
	Mesh.Indexes.Push(startVertIndex + 3);
	Mesh.Indexes.Push(startVertIndex + 2);
	Mesh.Indexes.Push(startVertIndex + 1);

	surf.Bounds = GetBoundsFromSurface(surf);
}

void DoomLevelSubmesh::CreateLineHorizonSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.Side = side;
	surf.IsSky = front->skyFloor || front->skyCeiling; // front->GetTexture(PLANE_FLOOR) == skyflatnum || front->GetTexture(PLANE_CEILING) == skyflatnum;
	surf.SectorGroup = LevelMesh->sectorGroup[front->Index(doomMap)];

	FFlatVertex verts[4];
	verts[0].x = verts[2].x = v1.X;
	verts[0].y = verts[2].y = v1.Y;
	verts[1].x = verts[3].x = v2.X;
	verts[1].y = verts[3].y = v2.Y;
	verts[0].z = v1Bottom;
	verts[1].z = v2Bottom;
	verts[2].z = v1Top;
	verts[3].z = v2Top;
	AddWallVertices(surf, verts);

	SetSideTextureUVs(surf, side, WallPart::TOP, v1Top, v1Bottom, v2Top, v2Bottom);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateFrontWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	FFlatVertex verts[4];
	verts[0].x = verts[2].x = v1.X;
	verts[0].y = verts[2].y = v1.Y;
	verts[1].x = verts[3].x = v2.X;
	verts[1].y = verts[3].y = v2.Y;
	verts[0].z = v1Bottom;
	verts[1].z = v2Bottom;
	verts[2].z = v1Top;
	verts[3].z = v2Top;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Side = side;
	surf.IsSky = false;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.ControlSector = nullptr;
	surf.SectorGroup = LevelMesh->sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::MIDDLE);
	AddWallVertices(surf, verts);

	SetSideTextureUVs(surf, side, WallPart::TOP, v1Top, v1Bottom, v2Top, v2Bottom);
	AddSurfaceToTile(surf, bindings, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateMidWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	FFlatVertex verts[4];
	verts[0].x = verts[2].x = v1.X;
	verts[0].y = verts[2].y = v1.Y;
	verts[1].x = verts[3].x = v2.X;
	verts[1].y = verts[3].y = v2.Y;

	const auto& texture = side->GetTexture(WallPart::MIDDLE);

	//if ((side->Flags & WALLF_WRAP_MIDTEX) || (side->line->flags & WALLF_WRAP_MIDTEX))
	{
		verts[0].z = v1Bottom;
		verts[1].z = v2Bottom;
		verts[2].z = v1Top;
		verts[3].z = v2Top;
	}
	/*else
	{
		int offset = 0;

		auto gameTexture = TexMan.GetGameTexture(texture);

		float mid1Top = (float)(gameTexture->GetDisplayHeight() / side->GetTextureYScale(WallPart::MIDDLE));
		float mid2Top = (float)(gameTexture->GetDisplayHeight() / side->GetTextureYScale(WallPart::MIDDLE));
		float mid1Bottom = 0;
		float mid2Bottom = 0;

		float yTextureOffset = (float)(side->GetTextureYOffset(WallPart::MIDDLE) / gameTexture->GetScaleY());

		if (side->line->flags & ML_DONTPEGBOTTOM)
		{
			yTextureOffset += (float)side->sectordef->planes[PLANE_FLOOR].TexZ;
		}
		else
		{
			yTextureOffset += (float)(side->sectordef->planes[PLANE_CEILING].TexZ - gameTexture->GetDisplayHeight() / side->GetTextureYScale(WallPart::MIDDLE));
		}

		verts[0].z = std::min(std::max(yTextureOffset + mid1Bottom, v1Bottom), v1Top);
		verts[1].z = std::min(std::max(yTextureOffset + mid2Bottom, v2Bottom), v2Top);
		verts[2].z = std::max(std::min(yTextureOffset + mid1Top, v1Top), v1Bottom);
		verts[3].z = std::max(std::min(yTextureOffset + mid2Top, v2Top), v2Bottom);
	}*/

	// mid texture
	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Side = side;
	surf.IsSky = false;


	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.ControlSector = nullptr;
	surf.SectorGroup = LevelMesh->sectorGroup[front->Index(doomMap)];
	surf.Texture = texture;
	// surf.alpha = float(side->line->alpha);

	// FVector3 offset = surf.Plane.XYZ() * 0.05f; // for better accuracy when raytracing mid-textures from each side
	AddWallVertices(surf, verts/*, offset*/);
	if (side->line->sidenum[0] != side->Index(doomMap))
	{
		surf.Plane = -surf.Plane;
		surf.Plane.W = -surf.Plane.W;
	}

	SetSideTextureUVs(surf, side, WallPart::TOP, verts[2].z, verts[0].z, verts[3].z, verts[1].z);
	AddSurfaceToTile(surf, bindings, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::Create3DFloorWallSurfaces(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)back->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)back->floorplane.ZatPoint(v1);
	float v2Top = (float)back->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)back->floorplane.ZatPoint(v2);

	for (unsigned int j = 0; j < back->x3dfloors.Size(); j++)
	{
		IntSector* xfloor = back->x3dfloors[j];

		// Don't create a line when both sectors have the same 3d floor
		bool bothSides = false;
		for (unsigned int k = 0; k < front->x3dfloors.Size(); k++)
		{
			if (front->x3dfloors[k] == xfloor)
			{
				bothSides = true;
				break;
			}
		}
		if (bothSides)
			continue;

		DoomLevelMeshSurface surf;
		surf.Submesh = this;
		surf.Type = ST_MIDDLESIDE;
		surf.TypeIndex = side->Index(doomMap);
		surf.Side = side;
		surf.ControlSector = xfloor;
		surf.IsSky = false;

		float blZ = (float)xfloor->floorplane.ZatPoint(v1);
		float brZ = (float)xfloor->floorplane.ZatPoint(v2);
		float tlZ = (float)xfloor->ceilingplane.ZatPoint(v1);
		float trZ = (float)xfloor->ceilingplane.ZatPoint(v2);

		FFlatVertex verts[4];
		verts[0].x = verts[2].x = v2.X;
		verts[0].y = verts[2].y = v2.Y;
		verts[1].x = verts[3].x = v1.X;
		verts[1].y = verts[3].y = v1.Y;
		verts[0].z = brZ;
		verts[1].z = blZ;
		verts[2].z = trZ;
		verts[3].z = tlZ;

		surf.SectorGroup = LevelMesh->sectorGroup[back->Index(doomMap)];
		surf.Texture = side->GetTexture(WallPart::MIDDLE);

		AddWallVertices(surf, verts);
		SetSideTextureUVs(surf, side, WallPart::TOP, tlZ, blZ, trZ, brZ);
		AddSurfaceToTile(surf, bindings, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

		Surfaces.Push(surf);
	}
}

void DoomLevelSubmesh::CreateTopWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v1TopBack = (float)back->ceilingplane.ZatPoint(v1);
	float v2TopBack = (float)back->ceilingplane.ZatPoint(v2);

	bool bSky = IsTopSideSky(front, back, side);
	if (!bSky && !IsTopSideVisible(side))
		return;

	FFlatVertex verts[4];
	verts[0].x = verts[2].x = v1.X;
	verts[0].y = verts[2].y = v1.Y;
	verts[1].x = verts[3].x = v2.X;
	verts[1].y = verts[3].y = v2.Y;
	verts[0].z = v1TopBack;
	verts[1].z = v2TopBack;
	verts[2].z = v1Top;
	verts[3].z = v2Top;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Side = side;
	surf.Type = ST_UPPERSIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.IsSky = bSky;
	surf.ControlSector = nullptr;
	surf.SectorGroup = LevelMesh->sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::TOP);

	AddWallVertices(surf, verts);
	SetSideTextureUVs(surf, side, WallPart::TOP, v1Top, v1TopBack, v2Top, v2TopBack);
	AddSurfaceToTile(surf, bindings, doomMap, side->GetSampleDistance(WallPart::TOP));

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateBottomWallSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, IntSideDef* side)
{
	if (!IsBottomSideVisible(side))
		return;

	IntSector* front = side->sectordef;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);
	float v1BottomBack = (float)back->floorplane.ZatPoint(v1);
	float v2BottomBack = (float)back->floorplane.ZatPoint(v2);

	FFlatVertex verts[4];
	verts[0].x = verts[2].x = v1.X;
	verts[0].y = verts[2].y = v1.Y;
	verts[1].x = verts[3].x = v2.X;
	verts[1].y = verts[3].y = v2.Y;
	verts[0].z = v1Bottom;
	verts[1].z = v2Bottom;
	verts[2].z = v1BottomBack;
	verts[3].z = v2BottomBack;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Side = side;
	surf.Type = ST_LOWERSIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.IsSky = false;
	surf.ControlSector = nullptr;
	surf.SectorGroup = LevelMesh->sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::BOTTOM);

	AddWallVertices(surf, verts);
	SetSideTextureUVs(surf, side, WallPart::BOTTOM, v1BottomBack, v1Bottom, v2BottomBack, v2Bottom);
	AddSurfaceToTile(surf, bindings, doomMap, side->GetSampleDistance(WallPart::BOTTOM));

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::SetSideTextureUVs(DoomLevelMeshSurface& surface, IntSideDef* side, WallPart texpart, float v1TopZ, float v1BottomZ, float v2TopZ, float v2BottomZ)
{
	FFlatVertex* verts = &Mesh.Vertices[surface.MeshLocation.StartVertIndex];

#if 0
	if (surface.Texture.isValid())
	{
		const auto gtxt = TexMan.GetGameTexture(surface.Texture);

		FTexCoordInfo tci;
		GetTexCoordInfo(gtxt, &tci, side, texpart);

		float startU = tci.FloatToTexU(tci.TextureOffset((float)side->GetTextureXOffset(texpart)) + tci.TextureOffset((float)side->GetTextureXOffset(texpart)));
		float endU = startU + tci.FloatToTexU(side->TexelLength);

		verts[0].u = startU;
		verts[1].u = endU;
		verts[2].u = startU;
		verts[3].u = endU;

		// To do: the ceiling version is apparently used in some situation related to 3d floors (rover->top.isceiling)
		//float offset = tci.RowOffset((float)side->GetTextureYOffset(texpart)) + tci.RowOffset((float)side->GetTextureYOffset(texpart)) + (float)side->sector->GetPlaneTexZ(PLANE_CEILING);
		float offset = tci.RowOffset((float)side->GetTextureYOffset(texpart)) + tci.RowOffset((float)side->GetTextureYOffset(texpart)) + (float)side->sector->GetPlaneTexZ(PLANE_FLOOR);

		verts[0].v = tci.FloatToTexV(offset - v1BottomZ);
		verts[1].v = tci.FloatToTexV(offset - v2BottomZ);
		verts[2].v = tci.FloatToTexV(offset - v1TopZ);
		verts[3].v = tci.FloatToTexV(offset - v2TopZ);
	}
	else
#endif
	{
		for (int i = 0; i < 4; i++)
		{
			verts[i].u = 0.0f;
			verts[i].v = 0.0f;
		}
	}
}

void DoomLevelSubmesh::CreateFloorSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Subsector = sub;

	Plane plane;
	if (!controlSector)
	{
		plane = sector->floorplane;
		surf.IsSky = IsSkySector(sector, PLANE_FLOOR);
	}
	else
	{
		plane = controlSector->ceilingplane;
		plane.FlipVert();
		surf.IsSky = false;
	}

	surf.MeshLocation.NumVerts = sub->numlines;
	surf.MeshLocation.StartVertIndex = Mesh.Vertices.Size();
	surf.Texture = (controlSector ? controlSector : sector)->GetTexture(PLANE_FLOOR);

	FGameTexture* txt = TexMan.GetGameTexture(surf.Texture);
	float w = txt->GetDisplayWidth();
	float h = txt->GetDisplayHeight();
	//VSMatrix mat = GetPlaneTextureRotationMatrix(txt, sector, PLANE_FLOOR);
	VSMatrix mat; mat.loadIdentity();

	Mesh.Vertices.Resize(surf.MeshLocation.StartVertIndex + surf.MeshLocation.NumVerts);

	FFlatVertex* verts = &Mesh.Vertices[surf.MeshLocation.StartVertIndex];

	for (int j = 0; j < surf.MeshLocation.NumVerts; j++)
	{
		MapSegGLEx* seg = &doomMap.GLSegs[sub->firstline + (surf.MeshLocation.NumVerts - 1) - j];
		auto v = doomMap.GetSegVertex(seg->v1);
		FVector2 v1(v.x, v.y);
		FVector2 uv = (mat * FVector4(v1.X / 64.f, -v1.Y / 64.f, 0.f, 1.f)).XY(); // The magic 64.f and negative Y is based on SetFlatVertex

		verts[j].x = v1.X;
		verts[j].y = v1.Y;
		verts[j].z = (float)plane.ZatPoint(v1.X, v1.Y);
		verts[j].u = uv.X;
		verts[j].v = uv.Y;
	}

	unsigned int startVertIndex = surf.MeshLocation.StartVertIndex;
	unsigned int numElements = 0;
	surf.MeshLocation.StartElementIndex = Mesh.Indexes.Size();
	for (int j = 2; j < surf.MeshLocation.NumVerts; j++)
	{
		Mesh.Indexes.Push(startVertIndex);
		Mesh.Indexes.Push(startVertIndex + j - 1);
		Mesh.Indexes.Push(startVertIndex + j);
		numElements += 3;
	}
	surf.MeshLocation.NumElements = numElements;
	surf.Bounds = GetBoundsFromSurface(surf);

	surf.Type = ST_FLOOR;
	surf.TypeIndex = typeIndex;
	surf.ControlSector = controlSector;
	surf.Plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.d);
	surf.SectorGroup = LevelMesh->sectorGroup[sector->Index(doomMap)];
	AddSurfaceToTile(surf, bindings, doomMap, (controlSector ? controlSector : sector)->sampleDistanceFloor);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateCeilingSurface(std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Subsector = sub;

	Plane plane;
	if (!controlSector)
	{
		plane = sector->ceilingplane;
		surf.IsSky = IsSkySector(sector, PLANE_CEILING);
	}
	else
	{
		plane = controlSector->floorplane;
		plane.FlipVert();
		surf.IsSky = false;
	}

	surf.MeshLocation.NumVerts = sub->numlines;
	surf.MeshLocation.StartVertIndex = Mesh.Vertices.Size();
	surf.Texture = (controlSector ? controlSector : sector)->GetTexture(PLANE_CEILING);

	FGameTexture* txt = TexMan.GetGameTexture(surf.Texture);
	float w = txt->GetDisplayWidth();
	float h = txt->GetDisplayHeight();
	//VSMatrix mat = GetPlaneTextureRotationMatrix(txt, sector, PLANE_CEILING);
	VSMatrix mat; mat.loadIdentity();

	Mesh.Vertices.Resize(surf.MeshLocation.StartVertIndex + surf.MeshLocation.NumVerts);

	FFlatVertex* verts = &Mesh.Vertices[surf.MeshLocation.StartVertIndex];

	for (int j = 0; j < surf.MeshLocation.NumVerts; j++)
	{
		MapSegGLEx* seg = &doomMap.GLSegs[sub->firstline + j];
		auto v = doomMap.GetSegVertex(seg->v1);
		FVector2 v1 = FVector2(v.x, v.y);
		FVector2 uv = (mat * FVector4(v1.X / 64.f, -v1.Y / 64.f, 0.f, 1.f)).XY(); // The magic 64.f and negative Y is based on SetFlatVertex

		verts[j].x = v1.X;
		verts[j].y = v1.Y;
		verts[j].z = (float)plane.ZatPoint(v1.X, v1.Y);
		verts[j].u = uv.X;
		verts[j].v = uv.Y;
	}

	unsigned int startVertIndex = surf.MeshLocation.StartVertIndex;
	unsigned int numElements = 0;
	surf.MeshLocation.StartElementIndex = Mesh.Indexes.Size();
	for (int j = 2; j < surf.MeshLocation.NumVerts; j++)
	{
		Mesh.Indexes.Push(startVertIndex);
		Mesh.Indexes.Push(startVertIndex + j - 1);
		Mesh.Indexes.Push(startVertIndex + j);
		numElements += 3;
	}
	surf.MeshLocation.NumElements = numElements;
	surf.Bounds = GetBoundsFromSurface(surf);

	surf.Type = ST_CEILING;
	surf.TypeIndex = typeIndex;
	surf.ControlSector = controlSector;
	surf.Plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.d);
	surf.SectorGroup = LevelMesh->sectorGroup[sector->Index(doomMap)];
	AddSurfaceToTile(surf, bindings, doomMap, (controlSector ? controlSector : sector)->sampleDistanceCeiling);

	Surfaces.Push(surf);
}

bool DoomLevelSubmesh::IsTopSideSky(IntSector* frontsector, IntSector* backsector, IntSideDef* side)
{
	return IsSkySector(frontsector, PLANE_CEILING) && IsSkySector(backsector, PLANE_CEILING);
}

bool DoomLevelSubmesh::IsTopSideVisible(IntSideDef* side)
{
	//auto tex = TexMan.GetGameTexture(side->GetTexture(WallPart::TOP), true);
	//return tex && tex->isValid();
	return true;
}

bool DoomLevelSubmesh::IsBottomSideVisible(IntSideDef* side)
{
	//auto tex = TexMan.GetGameTexture(side->GetTexture(WallPart::BOTTOM), true);
	//return tex && tex->isValid();
	return true;
}

bool DoomLevelSubmesh::IsSkySector(IntSector* sector, SecPlaneType plane)
{
	// plane is either PLANE_CEILING or PLANE_FLOOR
	return plane == PLANE_CEILING ? sector->skyCeiling : sector->skyFloor; //return sector->GetTexture(plane) == skyflatnum;
}

bool DoomLevelSubmesh::IsDegenerate(const FVector3& v0, const FVector3& v1, const FVector3& v2)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = v1.X - v0.X;
	float ay = v1.Y - v0.Y;
	float az = v1.Z - v0.Z;
	float bx = v2.X - v0.X;
	float by = v2.Y - v0.Y;
	float bz = v2.Z - v0.Z;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

void DoomLevelSubmesh::AddSurfaceToTile(DoomLevelMeshSurface& surf, std::map<LightmapTileBinding, int>& bindings, FLevel& doomMap, uint16_t sampleDimension)
{
	if (surf.IsSky)
	{
		surf.LightmapTileIndex = -1;
	}

	LightmapTileBinding binding;
	binding.Type = surf.Type;
	binding.TypeIndex = surf.TypeIndex;
	binding.ControlSector = surf.ControlSector ? surf.ControlSector->Index(doomMap) : (int)0xffffffffUL;

	auto it = bindings.find(binding);
	if (it != bindings.end())
	{
		int index = it->second;

		LightmapTile& tile = LightmapTiles[index];
		tile.Bounds.min.X = std::min(tile.Bounds.min.X, surf.Bounds.min.X);
		tile.Bounds.min.Y = std::min(tile.Bounds.min.Y, surf.Bounds.min.Y);
		tile.Bounds.min.Z = std::min(tile.Bounds.min.Z, surf.Bounds.min.Z);
		tile.Bounds.max.X = std::max(tile.Bounds.max.X, surf.Bounds.max.X);
		tile.Bounds.max.Y = std::max(tile.Bounds.max.Y, surf.Bounds.max.Y);
		tile.Bounds.max.Z = std::max(tile.Bounds.max.Z, surf.Bounds.max.Z);

		surf.LightmapTileIndex = index;
	}
	else
	{
		int index = LightmapTiles.Size();

		LightmapTile tile;
		tile.Binding = binding;
		tile.Bounds = surf.Bounds;
		tile.Plane = surf.Plane;
		tile.SampleDimension = GetSampleDimension(surf, sampleDimension);

		LightmapTiles.Push(tile);
		bindings[binding] = index;

		surf.LightmapTileIndex = index;
	}
}

int DoomLevelSubmesh::GetSampleDimension(const DoomLevelMeshSurface& surf, uint16_t sampleDimension)
{
	if (sampleDimension <= 0)
	{
		sampleDimension = LightmapSampleDistance;
	}

	sampleDimension = uint16_t(std::max(int(roundf(float(sampleDimension) / std::max(1.0f / 4, float(lm_scale)))), 1));

	// Round to nearest power of two
	uint32_t n = uint16_t(sampleDimension);
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n = (n + 1) >> 1;
	sampleDimension = uint16_t(n) ? uint16_t(n) : uint16_t(0xFFFF);

	return sampleDimension;
}

void DoomLevelSubmesh::CreateDynamicSurfaces(FLevel& doomMap)
{
}

void DoomLevelSubmesh::SortIndexes()
{
	// Order surfaces by pipeline
	std::unordered_map<int64_t, TArray<int>> pipelineSurfaces;
	for (size_t i = 0; i < Surfaces.Size(); i++)
	{
		DoomLevelMeshSurface* s = &Surfaces[i];
		pipelineSurfaces[(int64_t(s->PipelineID) << 32) | int64_t(s->IsSky)].Push((int)i);
	}

	// Create reorder surface indexes by pipeline and create a draw range for each
	TArray<uint32_t> sortedIndexes;
	for (const auto& it : pipelineSurfaces)
	{
		LevelSubmeshDrawRange range;
		range.PipelineID = it.first >> 32;
		range.Start = sortedIndexes.Size();

		// Move indexes to new array
		for (unsigned int i : it.second)
		{
			DoomLevelMeshSurface& s = Surfaces[i];

			unsigned int start = s.MeshLocation.StartElementIndex;
			unsigned int count = s.MeshLocation.NumElements;

			s.MeshLocation.StartElementIndex = sortedIndexes.Size();

			for (unsigned int j = 0; j < count; j++)
			{
				sortedIndexes.Push(Mesh.Indexes[start + j]);
			}

			for (unsigned int j = 0; j < count; j += 3)
			{
				Mesh.SurfaceIndexes.Push((int)i);
			}
		}

		range.Count = sortedIndexes.Size() - range.Start;

		if ((it.first & 1) == 0)
			DrawList.Push(range);
		else
			PortalList.Push(range);
	}

	Mesh.Indexes.Swap(sortedIndexes);
}

void DoomLevelSubmesh::PackLightmapAtlas(FLevel& doomMap, int lightmapStartIndex)
{
	std::vector<LightmapTile*> sortedTiles;
	sortedTiles.reserve(LightmapTiles.Size());

	for (auto& tile : LightmapTiles)
	{
		sortedTiles.push_back(&tile);
	}

	std::sort(sortedTiles.begin(), sortedTiles.end(), [](LightmapTile* a, LightmapTile* b) { return a->AtlasLocation.Height != b->AtlasLocation.Height ? a->AtlasLocation.Height > b->AtlasLocation.Height : a->AtlasLocation.Width > b->AtlasLocation.Width; });

	RectPacker packer(LMTextureSize, LMTextureSize, RectPacker::Spacing(0));

	for (LightmapTile* tile : sortedTiles)
	{
		int sampleWidth = tile->AtlasLocation.Width;
		int sampleHeight = tile->AtlasLocation.Height;

		auto result = packer.insert(sampleWidth, sampleHeight);
		int x = result.pos.x, y = result.pos.y;

		tile->AtlasLocation.X = x;
		tile->AtlasLocation.Y = y;
		tile->AtlasLocation.ArrayIndex = lightmapStartIndex + (int)result.pageIndex;
	}

	LMTextureCount = (int)packer.getNumPages();

	// Calculate final texture coordinates
	for (auto& surface : Surfaces)
	{
		if (surface.LightmapTileIndex >= 0)
		{
			const LightmapTile& tile = LightmapTiles[surface.LightmapTileIndex];
			for (int i = 0; i < surface.MeshLocation.NumVerts; i++)
			{
				auto& vertex = Mesh.Vertices[surface.MeshLocation.StartVertIndex + i];
				FVector2 uv = tile.ToUV(vertex.fPos(), (float)LMTextureSize);
				vertex.lu = uv.X;
				vertex.lv = uv.Y;
				vertex.lindex = (float)tile.AtlasLocation.ArrayIndex;
			}
		}
	}

#if 0 // Debug atlas tile locations:
	float colors[30] =
	{
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		0.5f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.0f,
		0.0f, 0.5f, 0.5f,
		0.5f, 0.0f, 0.5f
	};
	LMTextureData.Resize(LMTextureSize * LMTextureSize * LMTextureCount * 4);
	uint16_t* pixels = LMTextureData.Data();
	for (LightmapTile& tile : LightmapTiles)
	{
		tile.NeedsUpdate = false;

		int index = tile.Binding.TypeIndex;
		float* color = colors + (index % 10) * 3;

		int x = tile.AtlasLocation.X;
		int y = tile.AtlasLocation.Y;
		int w = tile.AtlasLocation.Width;
		int h = tile.AtlasLocation.Height;
		for (int yy = y; yy < y + h; yy++)
		{
			uint16_t* line = pixels + tile.AtlasLocation.ArrayIndex * LMTextureSize * LMTextureSize + yy * LMTextureSize * 4;
			for (int xx = x; xx < x + w; xx++)
			{
				float gray = (yy - y) / (float)h;
				line[xx * 4] = floatToHalf(color[0] * gray);
				line[xx * 4 + 1] = floatToHalf(color[1] * gray);
				line[xx * 4 + 2] = floatToHalf(color[2] * gray);
				line[xx * 4 + 3] = 0x3c00; // half-float 1.0
			}
		}
	}
	for (DoomLevelMeshSurface& surf : Surfaces)
	{
		surf.AlwaysUpdate = false;
	}
#endif
}

BBox DoomLevelSubmesh::GetBoundsFromSurface(const LevelMeshSurface& surface) const
{
	BBox bounds;
	bounds.Clear();
	for (int i = int(surface.MeshLocation.StartVertIndex); i < int(surface.MeshLocation.StartVertIndex) + surface.MeshLocation.NumVerts; i++)
	{
		FVector3 v = Mesh.Vertices[(int)i].fPos();
		bounds.min.X = std::min(bounds.min.X, v.X);
		bounds.min.Y = std::min(bounds.min.Y, v.Y);
		bounds.min.Z = std::min(bounds.min.Z, v.Z);
		bounds.max.X = std::max(bounds.max.X, v.X);
		bounds.max.Y = std::max(bounds.max.Y, v.Y);
		bounds.max.Z = std::max(bounds.max.Z, v.Z);
	}
	return bounds;
}

DoomLevelSubmesh::PlaneAxis DoomLevelSubmesh::BestAxis(const FVector4& p)
{
	float na = fabs(float(p.X));
	float nb = fabs(float(p.Y));
	float nc = fabs(float(p.Z));

	// figure out what axis the plane lies on
	if (na >= nb && na >= nc)
	{
		return AXIS_YZ;
	}
	else if (nb >= na && nb >= nc)
	{
		return AXIS_XZ;
	}

	return AXIS_XY;
}

void DoomLevelSubmesh::SetupTileTransform(int lightMapTextureWidth, int lightMapTextureHeight, LightmapTile& tile)
{
	BBox bounds = tile.Bounds;

	// round off dimensions
	FVector3 roundedSize;
	for (int i = 0; i < 3; i++)
	{
		bounds.min[i] = tile.SampleDimension * (floor(bounds.min[i] / tile.SampleDimension) - 1);
		bounds.max[i] = tile.SampleDimension * (ceil(bounds.max[i] / tile.SampleDimension) + 1);
		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / tile.SampleDimension;
	}

	FVector3 tCoords[2] = { FVector3(0.0f, 0.0f, 0.0f), FVector3(0.0f, 0.0f, 0.0f) };

	PlaneAxis axis = BestAxis(tile.Plane);

	int width;
	int height;
	switch (axis)
	{
	default:
	case AXIS_YZ:
		width = (int)roundedSize.Y;
		height = (int)roundedSize.Z;
		tCoords[0].Y = 1.0f / tile.SampleDimension;
		tCoords[1].Z = 1.0f / tile.SampleDimension;
		break;

	case AXIS_XZ:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Z;
		tCoords[0].X = 1.0f / tile.SampleDimension;
		tCoords[1].Z = 1.0f / tile.SampleDimension;
		break;

	case AXIS_XY:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Y;
		tCoords[0].X = 1.0f / tile.SampleDimension;
		tCoords[1].Y = 1.0f / tile.SampleDimension;
		break;
	}

	// clamp width
	if (width > lightMapTextureWidth - 2)
	{
		tCoords[0] *= ((float)(lightMapTextureWidth - 2) / (float)width);
		width = (lightMapTextureWidth - 2);
	}

	// clamp height
	if (height > lightMapTextureHeight - 2)
	{
		tCoords[1] *= ((float)(lightMapTextureHeight - 2) / (float)height);
		height = (lightMapTextureHeight - 2);
	}

	tile.Transform.TranslateWorldToLocal = bounds.min;
	tile.Transform.ProjLocalToU = tCoords[0];
	tile.Transform.ProjLocalToV = tCoords[1];

	tile.AtlasLocation.Width = width;
	tile.AtlasLocation.Height = height;
}
