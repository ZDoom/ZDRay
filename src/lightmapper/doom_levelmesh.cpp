
#include "doom_levelmesh.h"
#include "level/level.h"
#include "framework/halffloat.h"
#include "framework/binfile.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

extern float lm_scale;

DoomLevelMesh::DoomLevelMesh(FLevel& doomMap)
{
	// Remove the empty mesh added in the LevelMesh constructor
	Mesh.Vertices.Clear();
	Mesh.Indexes.Clear();

	SunColor = doomMap.defaultSunColor; // TODO keep only one copy?
	SunDirection = doomMap.defaultSunDirection;

	BuildSectorGroups(doomMap);
	CreatePortals(doomMap);

	LightmapSampleDistance = doomMap.DefaultSamples;

	CreateSurfaces(doomMap);

	SortIndexes();
	BuildTileSurfaceLists();

	Mesh.DynamicIndexStart = Mesh.Indexes.Size();
	UpdateCollision();

	// Assume double the size of the static mesh will be enough for anything dynamic.
	Mesh.MaxVertices = std::max(Mesh.Vertices.Size() * 2, (unsigned int)10000);
	Mesh.MaxIndexes = std::max(Mesh.Indexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxSurfaces = std::max(Mesh.SurfaceIndexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxUniforms = std::max(Mesh.Uniforms.Size() * 2, (unsigned int)10000);
	Mesh.MaxSurfaceIndexes = std::max(Mesh.SurfaceIndexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxNodes = (int)std::max(Collision->get_nodes().size() * 2, (size_t)10000);
	Mesh.MaxLights = 100'000;
	Mesh.MaxLightIndexes = 4 * 1024 * 1024;
}

void DoomLevelMesh::CreateLights(FLevel& doomMap)
{
	if (Mesh.Lights.Size() != 0)
		return;

	for (unsigned i = 0; i < doomMap.ThingLights.Size(); ++i)
	{
		printf("   Building light lists: %u / %u\r", i, doomMap.ThingLights.Size());
		PropagateLight(doomMap, &doomMap.ThingLights[i], 0);
	}

	printf("   Building light lists: %u / %u\n", doomMap.ThingLights.Size(), doomMap.ThingLights.Size());

	for (DoomLevelMeshSurface& surface : Surfaces)
	{
		surface.LightList.Pos = Mesh.LightIndexes.Size();
		surface.LightList.Count = 0;

		int listpos = 0;
		for (ThingLight* light : surface.Lights)
		{
			int lightindex = GetLightIndex(light, surface.PortalIndex);
			if (lightindex >= 0)
			{
				Mesh.LightIndexes.Push(lightindex);
				surface.LightList.Count++;
			}
		}
	}
}

void DoomLevelMesh::PropagateLight(FLevel& doomMap, ThingLight* light, int recursiveDepth)
{
	if (recursiveDepth > 32)
		return;

	SphereShape sphere;
	sphere.center = light->LightRelativeOrigin();
	sphere.radius = light->LightRadius();
	//std::set<Portal, RecursivePortalComparator> portalsToErase;
	for (int triangleIndex : TriangleMeshShape::find_all_hits(Collision.get(), &sphere))
	{
		DoomLevelMeshSurface* surface = &Surfaces[Mesh.SurfaceIndexes[triangleIndex]];

		// skip any surface which isn't physically connected to the sector group in which the light resides
		//if (light->sectorGroup == surface->sectorGroup)
		{
			/*if (surface->portalIndex >= 0)
			{
				auto portal = portals[surface->portalIndex].get();

				if (touchedPortals.insert(*portal).second)
				{
					auto fakeLight = std::make_unique<ThingLight>(*light);

					fakeLight->relativePosition.emplace(portal->TransformPosition(light->LightRelativeOrigin()));
					fakeLight->sectorGroup = portal->targetSectorGroup;

					PropagateLight(doomMap, fakeLight.get(), recursiveDepth + 1);
					portalsToErase.insert(*portal);
					portalLights.push_back(std::move(fakeLight));
				}
			}*/

			// Add light to the list if it isn't already there
			bool found = false;
			for (ThingLight* light2 : surface->Lights)
			{
				if (light2 == light)
				{
					found = true;
					break;
				}
			}
			if (!found)
				surface->Lights.Push(light);
		}
	}

	/*for (auto& portal : portalsToErase)
	{
		touchedPortals.erase(portal);
	}*/
}

int DoomLevelMesh::GetLightIndex(ThingLight* light, int portalgroup)
{
	int index;
	for (index = 0; index < ThingLight::max_levelmesh_entries && light->levelmesh[index].index != 0; index++)
	{
		if (light->levelmesh[index].portalgroup == portalgroup)
			return light->levelmesh[index].index - 1;
	}
	if (index == ThingLight::max_levelmesh_entries)
		return 0;

	LevelMeshLight meshlight;
	meshlight.Origin = light->LightOrigin(); // light->PosRelative(portalgroup);
	meshlight.RelativeOrigin = light->LightRelativeOrigin();
	meshlight.Radius = light->LightRadius();
	meshlight.Intensity = light->intensity;
	meshlight.InnerAngleCos = light->innerAngleCos;
	meshlight.OuterAngleCos = light->outerAngleCos;
	meshlight.SpotDir = light->SpotDir();
	meshlight.Color = light->rgb;

	meshlight.SectorGroup = 0;
	// if (light->sector)
	//	meshlight.SectorGroup = sectorGroup[light->sector->Index(doomMap)];

	int lightindex = Mesh.Lights.Size();
	light->levelmesh[index].index = lightindex + 1;
	light->levelmesh[index].portalgroup = portalgroup;
	Mesh.Lights.Push(meshlight);
	return lightindex;
}

void DoomLevelMesh::BeginFrame(FLevel& doomMap)
{
	CreateLights(doomMap);
}

bool DoomLevelMesh::TraceSky(const FVector3& start, FVector3 direction, float dist)
{
	FVector3 end = start + direction * dist;
	auto surface = Trace(start, direction, dist);
	return surface && surface->IsSky;
}

void DoomLevelMesh::CreateSurfaces(FLevel& doomMap)
{
	bindings.clear();
	Sides.Clear();
	Flats.Clear();
	Sides.Resize(doomMap.Sides.Size());
	Flats.Resize(doomMap.Sectors.Size());

	// Create surface objects for all sides
	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
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

		CreateFloorSurface(doomMap, sub, sector, nullptr, i);
		CreateCeilingSurface(doomMap, sub, sector, nullptr, i);

		for (unsigned int j = 0; j < sector->x3dfloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector, sector->x3dfloors[j], i);
			CreateCeilingSurface(doomMap, sub, sector, sector->x3dfloors[j], i);
		}
	}
}

void DoomLevelMesh::CreateSideSurfaces(FLevel& doomMap, IntSideDef* side)
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
		CreateLinePortalSurface(doomMap, side);
	}
	else*/ if (side->line->special == Line_Horizon && front != back)
	{
		CreateLineHorizonSurface(doomMap, side);
	}
	else if (!back)
	{
		if (side->GetTexture(WallPart::MIDDLE).isValid())
		{
			CreateFrontWallSurface(doomMap, side);
		}
	}
	else
	{
		if (side->GetTexture(WallPart::MIDDLE).isValid())
		{
			CreateMidWallSurface(doomMap, side);
		}

		Create3DFloorWallSurfaces(doomMap, side);

		float v1TopBack = (float)back->ceilingplane.ZatPoint(v1);
		float v1BottomBack = (float)back->floorplane.ZatPoint(v1);
		float v2TopBack = (float)back->ceilingplane.ZatPoint(v2);
		float v2BottomBack = (float)back->floorplane.ZatPoint(v2);

		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			CreateBottomWallSurface(doomMap, side);
		}

		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			CreateTopWallSurface( doomMap, side);
		}
	}
}

void DoomLevelMesh::AddWallVertices(DoomLevelMeshSurface& surf, FFlatVertex* verts)
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

void DoomLevelMesh::CreateLineHorizonSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sectordef;

	FVector2 v1 = side->V1(doomMap);
	FVector2 v2 = side->V2(doomMap);

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	DoomLevelMeshSurface surf;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.Side = side;
	surf.IsSky = front->skyFloor || front->skyCeiling; // front->GetTexture(PLANE_FLOOR) == skyflatnum || front->GetTexture(PLANE_CEILING) == skyflatnum;
	surf.SectorGroup = sectorGroup[front->Index(doomMap)];

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

void DoomLevelMesh::CreateFrontWallSurface(FLevel& doomMap, IntSideDef* side)
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
	surf.Side = side;
	surf.IsSky = false;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.ControlSector = nullptr;
	surf.SectorGroup = sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::MIDDLE);
	AddWallVertices(surf, verts);

	SetSideTextureUVs(surf, side, WallPart::TOP, v1Top, v1Bottom, v2Top, v2Bottom);
	AddSurfaceToTile(surf, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateMidWallSurface(FLevel& doomMap, IntSideDef* side)
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
	surf.Side = side;
	surf.IsSky = false;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.ControlSector = nullptr;
	surf.SectorGroup = sectorGroup[front->Index(doomMap)];
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
	AddSurfaceToTile(surf, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

	Surfaces.Push(surf);
}

void DoomLevelMesh::Create3DFloorWallSurfaces(FLevel& doomMap, IntSideDef* side)
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

		surf.SectorGroup = sectorGroup[back->Index(doomMap)];
		surf.Texture = side->GetTexture(WallPart::MIDDLE);

		AddWallVertices(surf, verts);
		SetSideTextureUVs(surf, side, WallPart::TOP, tlZ, blZ, trZ, brZ);
		AddSurfaceToTile(surf, doomMap, side->GetSampleDistance(WallPart::MIDDLE));

		Surfaces.Push(surf);
	}
}

void DoomLevelMesh::CreateTopWallSurface(FLevel& doomMap, IntSideDef* side)
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
	surf.Side = side;
	surf.Type = ST_UPPERSIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.IsSky = bSky;
	surf.ControlSector = nullptr;
	surf.SectorGroup = sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::TOP);

	AddWallVertices(surf, verts);
	SetSideTextureUVs(surf, side, WallPart::TOP, v1Top, v1TopBack, v2Top, v2TopBack);
	AddSurfaceToTile(surf, doomMap, side->GetSampleDistance(WallPart::TOP));

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateBottomWallSurface(FLevel& doomMap, IntSideDef* side)
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
	surf.Side = side;
	surf.Type = ST_LOWERSIDE;
	surf.TypeIndex = side->Index(doomMap);
	surf.IsSky = false;
	surf.ControlSector = nullptr;
	surf.SectorGroup = sectorGroup[front->Index(doomMap)];
	surf.Texture = side->GetTexture(WallPart::BOTTOM);

	AddWallVertices(surf, verts);
	SetSideTextureUVs(surf, side, WallPart::BOTTOM, v1BottomBack, v1Bottom, v2BottomBack, v2Bottom);
	AddSurfaceToTile(surf, doomMap, side->GetSampleDistance(WallPart::BOTTOM));

	Surfaces.Push(surf);
}

void DoomLevelMesh::SetSideTextureUVs(DoomLevelMeshSurface& surface, IntSideDef* side, WallPart texpart, float v1TopZ, float v1BottomZ, float v2TopZ, float v2BottomZ)
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

void DoomLevelMesh::CreateFloorSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
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
	surf.SectorGroup = sectorGroup[sector->Index(doomMap)];
	AddSurfaceToTile(surf, doomMap, (controlSector ? controlSector : sector)->sampleDistanceFloor);

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateCeilingSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
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
		Mesh.Indexes.Push(startVertIndex + j);
		Mesh.Indexes.Push(startVertIndex + j - 1);
		Mesh.Indexes.Push(startVertIndex);
		numElements += 3;
	}
	surf.MeshLocation.NumElements = numElements;
	surf.Bounds = GetBoundsFromSurface(surf);

	surf.Type = ST_CEILING;
	surf.TypeIndex = typeIndex;
	surf.ControlSector = controlSector;
	surf.Plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.d);
	surf.SectorGroup = sectorGroup[sector->Index(doomMap)];
	AddSurfaceToTile(surf, doomMap, (controlSector ? controlSector : sector)->sampleDistanceCeiling);

	Surfaces.Push(surf);
}

bool DoomLevelMesh::IsTopSideSky(IntSector* frontsector, IntSector* backsector, IntSideDef* side)
{
	return IsSkySector(frontsector, PLANE_CEILING) && IsSkySector(backsector, PLANE_CEILING);
}

bool DoomLevelMesh::IsTopSideVisible(IntSideDef* side)
{
	//auto tex = TexMan.GetGameTexture(side->GetTexture(WallPart::TOP), true);
	//return tex && tex->isValid();
	return true;
}

bool DoomLevelMesh::IsBottomSideVisible(IntSideDef* side)
{
	//auto tex = TexMan.GetGameTexture(side->GetTexture(WallPart::BOTTOM), true);
	//return tex && tex->isValid();
	return true;
}

bool DoomLevelMesh::IsSkySector(IntSector* sector, SecPlaneType plane)
{
	// plane is either PLANE_CEILING or PLANE_FLOOR
	return plane == PLANE_CEILING ? sector->skyCeiling : sector->skyFloor; //return sector->GetTexture(plane) == skyflatnum;
}

bool DoomLevelMesh::IsDegenerate(const FVector3& v0, const FVector3& v1, const FVector3& v2)
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

void DoomLevelMesh::AddSurfaceToTile(DoomLevelMeshSurface& surf, FLevel& doomMap, uint16_t sampleDimension)
{
	if (surf.IsSky)
	{
		surf.LightmapTileIndex = -1;
		return;
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

int DoomLevelMesh::GetSampleDimension(const DoomLevelMeshSurface& surf, uint16_t sampleDimension)
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

void DoomLevelMesh::SortIndexes()
{
	// Order surfaces by pipeline
	std::unordered_map<int64_t, TArray<int>> pipelineSurfaces;
	for (int i = 0; i < (int)Surfaces.Size(); i++)
	{
		DoomLevelMeshSurface* s = &Surfaces[i];
		pipelineSurfaces[(int64_t(s->PipelineID) << 32) | int64_t(s->IsSky)].Push(i);
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

BBox DoomLevelMesh::GetBoundsFromSurface(const LevelMeshSurface& surface) const
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

void DoomLevelMesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
	auto f = fopen(objFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");
	fprintf(f, "# Vertices: %u, Indexes: %u, Surfaces: %u\n", Mesh.Vertices.Size(), Mesh.Indexes.Size(), Surfaces.Size());
	fprintf(f, "mtllib %s\n", mtlFilename.GetChars());

	double scale = 1 / 10.0;

	for (const auto& v : Mesh.Vertices)
	{
		fprintf(f, "v %f %f %f\n", v.x * scale, v.y * scale, v.z * scale);
	}

	for (const auto& v : Mesh.Vertices)
	{
		fprintf(f, "vt %f %f\n", v.lu, v.lv);
	}

	auto name = [](DoomLevelMeshSurfaceType type) -> const char* {
		switch (type)
		{
		case ST_CEILING:
			return "ceiling";
		case ST_FLOOR:
			return "floor";
		case ST_LOWERSIDE:
			return "lowerside";
		case ST_UPPERSIDE:
			return "upperside";
		case ST_MIDDLESIDE:
			return "middleside";
		case ST_NONE:
			return "none";
		default:
			break;
		}
		return "error";
		};


	uint32_t lastSurfaceIndex = -1;


	bool useErrorMaterial = false;
	int highestUsedAtlasPage = -1;

	for (unsigned i = 0, count = Mesh.Indexes.Size(); i + 2 < count; i += 3)
	{
		auto index = Mesh.SurfaceIndexes[i / 3];

		if (index != lastSurfaceIndex)
		{
			lastSurfaceIndex = index;

			if (unsigned(index) >= Surfaces.Size())
			{
				fprintf(f, "o Surface[%d] (bad index)\n", index);
				fprintf(f, "usemtl error\n");

				useErrorMaterial = true;
			}
			else
			{
				const auto& surface = Surfaces[index];
				fprintf(f, "o Surface[%d] %s %d%s\n", index, name(surface.Type), surface.TypeIndex, surface.IsSky ? " sky" : "");

				if (surface.LightmapTileIndex >= 0)
				{
					auto& tile = LightmapTiles[surface.LightmapTileIndex];
					fprintf(f, "usemtl lightmap%d\n", tile.AtlasLocation.ArrayIndex);

					if (tile.AtlasLocation.ArrayIndex > highestUsedAtlasPage)
					{
						highestUsedAtlasPage = tile.AtlasLocation.ArrayIndex;
					}
				}
			}
		}

		// fprintf(f, "f %d %d %d\n", MeshElements[i] + 1, MeshElements[i + 1] + 1, MeshElements[i + 2] + 1);
		fprintf(f, "f %d/%d %d/%d %d/%d\n",
			Mesh.Indexes[i + 0] + 1, Mesh.Indexes[i + 0] + 1,
			Mesh.Indexes[i + 1] + 1, Mesh.Indexes[i + 1] + 1,
			Mesh.Indexes[i + 2] + 1, Mesh.Indexes[i + 2] + 1);

	}

	fclose(f);

	// material

	f = fopen(mtlFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");

	if (useErrorMaterial)
	{
		fprintf(f, "# Surface indices that are referenced, but do not exists in the 'Surface' array\n");
		fprintf(f, "newmtl error\nKa 1 0 0\nKd 1 0 0\nKs 1 0 0\n");
	}

	for (int page = 0; page <= highestUsedAtlasPage; ++page)
	{
		fprintf(f, "newmtl lightmap%d\n", page);
		fprintf(f, "Ka 1 1 1\nKd 1 1 1\nKs 0 0 0\n");
		fprintf(f, "map_Ka lightmap%d.png\n", page);
		fprintf(f, "map_Kd lightmap%d.png\n", page);
	}

	fclose(f);
}

void DoomLevelMesh::BuildSectorGroups(const FLevel& doomMap)
{
	int groupIndex = 0;

	TArray<IntSector*> queue;

	sectorGroup.Resize(doomMap.Sectors.Size());
	memset(sectorGroup.Data(), 0, sectorGroup.Size() * sizeof(int));

	for (int i = 0, count = doomMap.Sectors.Size(); i < count; ++i)
	{
		auto* sector = &doomMap.Sectors[i];

		auto& currentSectorGroup = sectorGroup[sector->Index(doomMap)];
		if (currentSectorGroup == 0)
		{
			currentSectorGroup = ++groupIndex;

			queue.Push(sector);

			while (queue.Size() > 0)
			{
				auto* sector = queue.Last();
				queue.Pop();

				for (auto& line : sector->lines)
				{
					auto otherSector = line->frontsector == sector ? line->backsector : line->frontsector;
					if (otherSector && otherSector != sector)
					{
						auto& id = sectorGroup[otherSector->Index(doomMap)];

						if (id == 0)
						{
							id = groupIndex;
							queue.Push(otherSector);
						}
					}
				}
			}
		}
	}
}

void DoomLevelMesh::CreatePortals(FLevel& doomMap)
{
	std::map<LevelMeshPortal, int, IdenticalPortalComparator> transformationIndices;
	transformationIndices.emplace(LevelMeshPortal{}, 0); // first portal is an identity matrix

	sectorPortals[0].Resize(doomMap.Sectors.Size());
	sectorPortals[1].Resize(doomMap.Sectors.Size());

	for (unsigned int i = 0, count = doomMap.Sectors.Size(); i < count; i++)
	{
		IntSector* sector = &doomMap.Sectors[i];
		for (int plane = 0; plane < 2; plane++)
		{
#ifdef NEEDS_PORTING
			auto d = sector->GetPortalDisplacement(plane);
			if (!d.isZero())
			{
				// Note: Y and Z is swapped in the shader due to how the hwrenderer was implemented
				VSMatrix transformation;
				transformation.loadIdentity();
				transformation.translate((float)d.X, 0.0f, (float)d.Y);

				int targetSectorGroup = 0;
				auto portalDestination = sector->GetPortal(plane)->mDestination;
				if (portalDestination)
				{
					targetSectorGroup = sectorGroup[portalDestination->Index()];
				}

				LevelMeshPortal portal;
				portal.transformation = transformation;
				portal.sourceSectorGroup = sectorGroup[i];
				portal.targetSectorGroup = targetSectorGroup;

				auto& index = transformationIndices[portal];
				if (index == 0) // new transformation was created
				{
					index = Portals.Size();
					Portals.Push(portal);
				}

				sectorPortals[plane][i] = index;
			}
			else
			{
				sectorPortals[plane][i] = 0;
			}
#else
			sectorPortals[plane][i] = 0;
#endif
		}
	}

	linePortals.Resize(doomMap.Lines.Size());
	for (unsigned int i = 0, count = doomMap.Lines.Size(); i < count; i++)
	{
		linePortals[i] = 0;

#ifdef NEEDS_PORTING
		IntLineDef* sourceLine = &doomMap.lines[i];
		if (sourceLine->isLinePortal())
		{
			VSMatrix transformation;
			transformation.loadIdentity();

			auto targetLine = sourceLine->getPortalDestination();
			if (!targetLine || !sourceLine->frontsector || !targetLine->frontsector)
				continue;

			double z = 0;

			// auto xy = surface.Side->linedef->getPortalDisplacement(); // Works only for static portals... ugh
			auto sourceXYZ = DVector2((sourceLine->v1->fX() + sourceLine->v2->fX()) / 2, (sourceLine->v2->fY() + sourceLine->v1->fY()) / 2);
			auto targetXYZ = DVector2((targetLine->v1->fX() + targetLine->v2->fX()) / 2, (targetLine->v2->fY() + targetLine->v1->fY()) / 2);

			// floor or ceiling alignment
			auto alignment = sourceLine->GetLevel()->linePortals[sourceLine->portalindex].mAlign;
			if (alignment != PORG_ABSOLUTE)
			{
				int plane = alignment == PORG_FLOOR ? 1 : 0;

				auto& sourcePlane = plane ? sourceLine->frontsector->floorplane : sourceLine->frontsector->ceilingplane;
				auto& targetPlane = plane ? targetLine->frontsector->floorplane : targetLine->frontsector->ceilingplane;

				auto tz = targetPlane.ZatPoint(targetXYZ);
				auto sz = sourcePlane.ZatPoint(sourceXYZ);

				z = tz - sz;
			}

			// Note: Y and Z is swapped in the shader due to how the hwrenderer was implemented
			transformation.rotate((float)sourceLine->getPortalAngleDiff().Degrees(), 0.0f, 1.0f, 0.0f);
			transformation.translate((float)(targetXYZ.X - sourceXYZ.X), (float)z, (float)(targetXYZ.Y - sourceXYZ.Y));

			int targetSectorGroup = 0;
			if (auto sector = targetLine->frontsector ? targetLine->frontsector : targetLine->backsector)
			{
				targetSectorGroup = sectorGroup[sector->Index()];
			}

			LevelMeshPortal portal;
			portal.transformation = transformation;
			portal.sourceSectorGroup = sectorGroup[sourceLine->frontsector->Index()];
			portal.targetSectorGroup = targetSectorGroup;

			auto& index = transformationIndices[portal];
			if (index == 0) // new transformation was created
			{
				index = Portals.Size();
				Portals.Push(portal);
			}

			linePortals[i] = index;
		}
#endif
	}
}

void DoomLevelMesh::AddLightmapLump(FLevel& doomMap, FWadWriter& wadFile)
{
	/*
	// LIGHTMAP V2 pseudo-C specification:

	struct LightmapLump
	{
		int version = 2;
		uint32_t tileCount;
		uint32_t pixelCount;
		uint32_t uvCount;
		SurfaceEntry surfaces[surfaceCount];
		uint16_t pixels[pixelCount * 3];
		float uvs[uvCount * 2];
	};

	struct TileEntry
	{
		uint32_t type, typeIndex;
		uint32_t controlSector; // 0xFFFFFFFF is none
		uint16_t width, height; // in pixels
		uint32_t pixelsOffset; // offset in pixels array
		vec3 translateWorldToLocal;
		vec3 projLocalToU;
		vec3 projLocalToV;
	};
	*/
	// Calculate size of lump
	uint32_t tileCount = 0;
	uint32_t pixelCount = 0;

	for (unsigned int i = 0; i < LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &LightmapTiles[i];
		if (tile->AtlasLocation.ArrayIndex != -1)
		{
			tileCount++;
			pixelCount += tile->AtlasLocation.Area();
		}
	}

	printf("   Writing %u tiles out of %llu\n", tileCount, (size_t)LightmapTiles.Size());

	const int version = 3;

	const uint32_t headerSize = sizeof(int) + 2 * sizeof(uint32_t);
	const uint32_t bytesPerTileEntry = sizeof(uint32_t) * 4 + sizeof(uint16_t) * 2 + sizeof(float) * 9;
	const uint32_t bytesPerPixel = sizeof(uint16_t) * 3; // F16 RGB

	uint32_t lumpSize = headerSize + tileCount * bytesPerTileEntry + pixelCount * bytesPerPixel;

	bool debug = false;

	if (debug)
	{
		printf("Lump size %u bytes\n", lumpSize);
		printf("Tiles: %u\nPixels: %u\n", tileCount, pixelCount);
	}

	// Setup buffer
	std::vector<uint8_t> buffer(lumpSize);
	BinFile lumpFile;
	lumpFile.SetBuffer(buffer.data());

	// Write header
	lumpFile.Write32(version);
	lumpFile.Write32(tileCount);
	lumpFile.Write32(pixelCount);

	if (debug)
	{
		printf("--- Saving tiles ---\n");
	}

	// Write tiles
	uint32_t pixelsOffset = 0;

	for (unsigned int i = 0; i < LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &LightmapTiles[i];

		if (tile->AtlasLocation.ArrayIndex == -1)
			continue;

		lumpFile.Write32(tile->Binding.Type);
		lumpFile.Write32(tile->Binding.TypeIndex);
		lumpFile.Write32(tile->Binding.ControlSector);

		lumpFile.Write16(uint16_t(tile->AtlasLocation.Width));
		lumpFile.Write16(uint16_t(tile->AtlasLocation.Height));

		lumpFile.Write32(pixelsOffset * 3);

		lumpFile.WriteFloat(tile->Transform.TranslateWorldToLocal.X);
		lumpFile.WriteFloat(tile->Transform.TranslateWorldToLocal.Y);
		lumpFile.WriteFloat(tile->Transform.TranslateWorldToLocal.Z);

		lumpFile.WriteFloat(tile->Transform.ProjLocalToU.X);
		lumpFile.WriteFloat(tile->Transform.ProjLocalToU.Y);
		lumpFile.WriteFloat(tile->Transform.ProjLocalToU.Z);

		lumpFile.WriteFloat(tile->Transform.ProjLocalToV.X);
		lumpFile.WriteFloat(tile->Transform.ProjLocalToV.Y);
		lumpFile.WriteFloat(tile->Transform.ProjLocalToV.Z);

		pixelsOffset += tile->AtlasLocation.Area();
	}

	if (debug)
	{
		printf("--- Saving pixels ---\n");
	}

	// Write surface pixels
	for (unsigned int i = 0; i < LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &LightmapTiles[i];

		if (tile->AtlasLocation.ArrayIndex == -1)
			continue;

		const uint16_t* pixels = LMTextureData.Data() + tile->AtlasLocation.ArrayIndex * LMTextureSize * LMTextureSize * 4;
		int width = tile->AtlasLocation.Width;
		int height = tile->AtlasLocation.Height;
		for (int y = 0; y < height; y++)
		{
			const uint16_t* srcline = pixels + (tile->AtlasLocation.X + (tile->AtlasLocation.Y + y) * LMTextureSize) * 4;
			for (int x = 0; x < width; x++)
			{
				lumpFile.Write16(*(srcline++));
				lumpFile.Write16(*(srcline++));
				lumpFile.Write16(*(srcline++));
				srcline++;
			}
		}
	}

	// Compress and store in lump
	ZLibOut zout(wadFile);
	wadFile.StartWritingLump("LIGHTMAP");
	zout.Write(buffer.data(), (int)(ptrdiff_t)(lumpFile.BufferAt() - lumpFile.Buffer()));
}
