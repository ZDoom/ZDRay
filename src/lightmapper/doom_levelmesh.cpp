
#include "doom_levelmesh.h"
#include <algorithm>
#include <map>

extern float lm_scale;

DoomLevelMesh::DoomLevelMesh(FLevel& doomMap, int samples, int lmdims)
{
	SunColor = doomMap.defaultSunColor; // TODO keep only one copy?
	SunDirection = doomMap.defaultSunDirection;

	StaticMesh = std::make_unique<DoomLevelSubmesh>();

	static_cast<DoomLevelSubmesh*>(StaticMesh.get())->CreateStatic(doomMap);
}

int DoomLevelMesh::AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize)
{
	return 0;
}

void DoomLevelMesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
	static_cast<DoomLevelSubmesh*>(StaticMesh.get())->DumpMesh(objFilename, mtlFilename);
}

void DoomLevelMesh::AddLightmapLump(FWadWriter& out)
{
}

/////////////////////////////////////////////////////////////////////////////

void DoomLevelSubmesh::CreateStatic(FLevel& doomMap)
{
	MeshVertices.Clear();
	MeshVertexUVs.Clear();
	MeshElements.Clear();

	LightmapSampleDistance = doomMap.DefaultSamples;

	BuildSectorGroups(doomMap);

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
	}

	CreateSubsectorSurfaces(doomMap);

	CreateIndexes();
	SetupLightmapUvs(doomMap);
	BuildTileSurfaceLists();
	UpdateCollision();
}

void DoomLevelSubmesh::CreateIndexes()
{
	for (unsigned int i = 0; i < Surfaces.Size(); i++)
	{
		DoomLevelMeshSurface& s = Surfaces[i];
		int numVerts = s.numVerts;
		unsigned int pos = s.startVertIndex;
		FVector3* verts = &MeshVertices[pos];

		s.startElementIndex = MeshElements.Size();
		s.numElements = 0;

		if (s.Type == ST_FLOOR || s.Type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(verts[0], verts[j - 1], verts[j]))
				{
					MeshElements.Push(pos);
					MeshElements.Push(pos + j - 1);
					MeshElements.Push(pos + j);
					MeshSurfaceIndexes.Push((int)i);
					s.numElements += 3;
				}
			}
		}
		else if (s.Type == ST_MIDDLESIDE || s.Type == ST_UPPERSIDE || s.Type == ST_LOWERSIDE)
		{
			if (!IsDegenerate(verts[0], verts[1], verts[2]))
			{
				MeshElements.Push(pos + 0);
				MeshElements.Push(pos + 1);
				MeshElements.Push(pos + 2);
				MeshSurfaceIndexes.Push((int)i);
				s.numElements += 3;
			}
			if (!IsDegenerate(verts[1], verts[2], verts[3]))
			{
				MeshElements.Push(pos + 3);
				MeshElements.Push(pos + 2);
				MeshElements.Push(pos + 1);
				MeshSurfaceIndexes.Push((int)i);
				s.numElements += 3;
			}
		}
	}
}

#if 0

void DoomLevelSubmesh::BuildSectorGroups(const FLevel& doomMap)
{
	int groupIndex = 0;

	TArray<IntSector*> queue;

	sectorGroup.Resize(doomMap.Sectors.Size());
	memset(sectorGroup.Data(), 0, sectorGroup.Size() * sizeof(int));

	for (int i = 0, count = doomMap.Sectors.Size(); i < count; ++i)
	{
		auto* sector = &doomMap.Sectors[i];

		auto& currentSectorGroup = sectorGroup[sector->Index()];
		if (currentSectorGroup == 0)
		{
			currentSectorGroup = ++groupIndex;

			queue.Push(sector);

			while (queue.Size() > 0)
			{
				auto* sector = queue.Last();
				queue.Pop();

				for (auto& line : sector->Lines)
				{
					auto otherSector = line->frontsector == sector ? line->backsector : line->frontsector;
					if (otherSector && otherSector != sector)
					{
						auto& id = sectorGroup[otherSector->Index()];

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

void DoomLevelSubmesh::CreatePortals()
{
	std::map<LevelMeshPortal, int, IdenticalPortalComparator> transformationIndices; // TODO use the list of portals from the level to avoids duplicates?
	transformationIndices.emplace(LevelMeshPortal{}, 0); // first portal is an identity matrix

	for (auto& surface : Surfaces)
	{
		bool hasPortal = [&]() {
			if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
			{
				return !surface.Subsector->sector->GetPortalDisplacement(surface.Type == ST_FLOOR ? IntSector::floor : IntSector::ceiling).isZero();
			}
			else if (surface.Type == ST_MIDDLESIDE)
			{
				return surface.Side->line->isLinePortal();
			}
			return false; // It'll take eternity to get lower/upper side portals into the ZDoom family.
			}();

			if (hasPortal)
			{
				auto transformation = [&]() {
					VSMatrix matrix;
					matrix.loadIdentity();

					if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
					{
						auto d = surface.Subsector->sector->GetPortalDisplacement(surface.Type == ST_FLOOR ? IntSector::floor : IntSector::ceiling);
						matrix.translate((float)d.X, (float)d.Y, 0.0f);
					}
					else if (surface.Type == ST_MIDDLESIDE)
					{
						auto sourceLine = surface.Side->line;

						if (sourceLine->isLinePortal())
						{
							auto targetLine = sourceLine->getPortalDestination();
							if (targetLine && sourceLine->frontsector && targetLine->frontsector)
							{
								double z = 0;

								// auto xy = surface.Side->line->getPortalDisplacement(); // Works only for static portals... ugh
								auto sourceXYZ = DVector2((sourceLine->v1->fX() + sourceLine->v2->fX()) / 2, (sourceLine->v2->fY() + sourceLine->v1->fY()) / 2);
								auto targetXYZ = DVector2((targetLine->v1->fX() + targetLine->v2->fX()) / 2, (targetLine->v2->fY() + targetLine->v1->fY()) / 2);

								// floor or ceiling alignment
								auto alignment = surface.Side->line->GetLevel()->linePortals[surface.Side->line->portalindex].mAlign;
								if (alignment != PORG_ABSOLUTE)
								{
									int plane = alignment == PORG_FLOOR ? 1 : 0;

									auto& sourcePlane = plane ? sourceLine->frontsector->floorplane : sourceLine->frontsector->ceilingplane;
									auto& targetPlane = plane ? targetLine->frontsector->floorplane : targetLine->frontsector->ceilingplane;

									auto tz = targetPlane.ZatPoint(targetXYZ);
									auto sz = sourcePlane.ZatPoint(sourceXYZ);

									z = tz - sz;
								}

								matrix.rotate((float)sourceLine->getPortalAngleDiff().Degrees(), 0.0f, 0.0f, 1.0f);
								matrix.translate((float)(targetXYZ.X - sourceXYZ.X), (float)(targetXYZ.Y - sourceXYZ.Y), (float)z);
							}
						}
					}
					return matrix;
					}();

					LevelMeshPortal portal;
					portal.transformation = transformation;
					portal.sourceSectorGroup = surface.sectorGroup;
					portal.targetSectorGroup = [&]() {
						if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
						{
							auto plane = surface.Type == ST_FLOOR ? IntSector::floor : IntSector::ceiling;
							auto portalDestination = surface.Subsector->sector->GetPortal(plane)->mDestination;
							if (portalDestination)
							{
								return sectorGroup[portalDestination->Index()];
							}
						}
						else if (surface.Type == ST_MIDDLESIDE)
						{
							auto targetLine = surface.Side->line->getPortalDestination();
							auto sector = targetLine->frontsector ? targetLine->frontsector : targetLine->backsector;
							if (sector)
							{
								return sectorGroup[sector->Index()];
							}
						}
						return 0;
						}();

						auto& index = transformationIndices[portal];

						if (index == 0) // new transformation was created
						{
							index = Portals.Size();
							Portals.Push(portal);
						}

						surface.portalIndex = index;
			}
			else
			{
				surface.portalIndex = 0;
			}
	}
}

void DoomLevelSubmesh::BindLightmapSurfacesToGeometry(FLevel& doomMap)
{
	// You have no idea how long this took me to figure out...

	// Reorder vertices into renderer format
	for (DoomLevelMeshSurface& surface : Surfaces)
	{
		if (surface.Type == ST_FLOOR)
		{
			// reverse vertices on floor
			for (int j = surface.startUvIndex + surface.numVerts - 1, k = surface.startUvIndex; j > k; j--, k++)
			{
				std::swap(LightmapUvs[k], LightmapUvs[j]);
			}
		}
		else if (surface.Type != ST_CEILING) // walls
		{
			// from 0 1 2 3
			// to   0 2 1 3
			std::swap(LightmapUvs[surface.startUvIndex + 1], LightmapUvs[surface.startUvIndex + 2]);
			std::swap(LightmapUvs[surface.startUvIndex + 2], LightmapUvs[surface.startUvIndex + 3]);
		}

		surface.TexCoords = (float*)&LightmapUvs[surface.startUvIndex];
	}

	// Link surfaces
	for (auto& surface : Surfaces)
	{
		if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
		{
			surface.Subsector = &doomMap.GLSubsectors[surface.TypeIndex];
			if (surface.Subsector->firstline && surface.Subsector->firstline->sidedef)
				surface.Subsector->firstline->sidedef->sector->HasLightmaps = true;
			SetSubsectorLightmap(&surface);
		}
		else
		{
			surface.Side = &doomMap.Sides[surface.TypeIndex];
			SetSideLightmap(&surface);
		}
	}
}

void DoomLevelSubmesh::SetSubsectorLightmap(DoomLevelMeshSurface* surface)
{
	if (!surface->ControlSector)
	{
		int index = surface->Type == ST_CEILING ? 1 : 0;
		surface->Subsector->lightmap[index][0] = surface;
	}
	else
	{
		int index = surface->Type == ST_CEILING ? 0 : 1;
		const auto& ffloors = surface->Subsector->sector->e->XFloor.ffloors;
		for (unsigned int i = 0; i < ffloors.Size(); i++)
		{
			if (ffloors[i]->model == surface->ControlSector)
			{
				surface->Subsector->lightmap[index][i + 1] = surface;
			}
		}
	}
}

void DoomLevelSubmesh::SetSideLightmap(DoomLevelMeshSurface* surface)
{
	if (!surface->ControlSector)
	{
		if (surface->Type == ST_UPPERSIDE)
		{
			surface->Side->lightmap[0] = surface;
		}
		else if (surface->Type == ST_MIDDLESIDE)
		{
			surface->Side->lightmap[1] = surface;
			surface->Side->lightmap[2] = surface;
		}
		else if (surface->Type == ST_LOWERSIDE)
		{
			surface->Side->lightmap[3] = surface;
		}
	}
	else
	{
		const auto& ffloors = surface->Side->sector->e->XFloor.ffloors;
		for (unsigned int i = 0; i < ffloors.Size(); i++)
		{
			if (ffloors[i]->model == surface->ControlSector)
			{
				surface->Side->lightmap[4 + i] = surface;
			}
		}
	}
}

void DoomLevelSubmesh::CreateLinePortalSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;
	verts[0].Z = v1Bottom;
	verts[1].Z = v2Bottom;
	verts[2].Z = v1Top;
	verts[3].Z = v2Top;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index();
	surf.bSky = front->GetTexture(IntSector::floor) == skyflatnum || front->GetTexture(IntSector::ceiling) == skyflatnum;
	surf.sampleDimension = side->textures[ETexpart::Mid].LightmapSampleDistance;
	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;

	MeshVertices.Push(verts[0]);
	MeshVertices.Push(verts[1]);
	MeshVertices.Push(verts[2]);
	MeshVertices.Push(verts[3]);

	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
	surf.sectorGroup = sectorGroup[front->Index()];

	SetSideTextureUVs(surf, side, ETexpart::Top, v1Top, v1Bottom, v2Top, v2Bottom);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateSideSurfaces(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sector;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	if (side->line->getPortal() && side->line->frontsector == front)
	{
		CreateLinePortalSurface(doomMap, side);
	}
	else if (side->line->special == Line_Horizon && front != back)
	{
		CreateLineHorizonSurface(doomMap, side);
	}
	else if (!back)
	{
		CreateFrontWallSurface(doomMap, side);
	}
	else
	{
		if (side->textures[ETexpart::Mid].texture.isValid())
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
			CreateTopWallSurface(doomMap, side);
		}
	}
}

void DoomLevelSubmesh::CreateLineHorizonSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index();
	surf.bSky = front->GetTexture(IntSector::floor) == skyflatnum || front->GetTexture(IntSector::ceiling) == skyflatnum;
	surf.sampleDimension = side->textures[ETexpart::Mid].LightmapSampleDistance;

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;
	verts[0].Z = v1Bottom;
	verts[1].Z = v2Bottom;
	verts[2].Z = v1Top;
	verts[3].Z = v2Top;

	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;
	MeshVertices.Push(verts[0]);
	MeshVertices.Push(verts[1]);
	MeshVertices.Push(verts[2]);
	MeshVertices.Push(verts[3]);

	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
	surf.sectorGroup = sectorGroup[front->Index()];

	SetSideTextureUVs(surf, side, ETexpart::Top, v1Top, v1Bottom, v2Top, v2Bottom);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateFrontWallSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = &doomMap.Sectors[side->sector];
	/*
		bool isPolyLine = !!(side->Flags & WALLF_POLYOBJ);
		if (isPolyLine)
		{
			MapSubsectorEx* subsector = level.PointInRenderSubsector((side->V1()->fPos() + side->V2()->fPos()) * 0.5);
			front = subsector->sector;
		}
	*/
	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;
	verts[0].Z = v1Bottom;
	verts[1].Z = v2Bottom;
	verts[2].Z = v1Top;
	verts[3].Z = v2Top;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.bSky = false;
	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;
	surf.bSky = false;
	MeshVertices.Push(verts[0]);
	MeshVertices.Push(verts[1]);
	MeshVertices.Push(verts[2]);
	MeshVertices.Push(verts[3]);

	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index();
	surf.sampleDimension = side->textures[ETexpart::Mid].LightmapSampleDistance;
	surf.ControlSector = nullptr;
	surf.sectorGroup = sectorGroup[front->Index()];
	surf.texture = side->textures[ETexpart::Mid].texture;

	SetSideTextureUVs(surf, side, ETexpart::Top, v1Top, v1Bottom, v2Top, v2Bottom);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateMidWallSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;

	const auto& texture = side->textures[ETexpart::Mid].texture;

	if ((side->Flags & WALLF_WRAP_MIDTEX) || (side->line->flags & WALLF_WRAP_MIDTEX))
	{
		verts[0].Z = v1Bottom;
		verts[1].Z = v2Bottom;
		verts[2].Z = v1Top;
		verts[3].Z = v2Top;
	}
	else
	{
		int offset = 0;

		auto gameTexture = TexMan.GetGameTexture(texture);

		float mid1Top = (float)(gameTexture->GetDisplayHeight() / side->textures[ETexpart::Mid].yScale);
		float mid2Top = (float)(gameTexture->GetDisplayHeight() / side->textures[ETexpart::Mid].yScale);
		float mid1Bottom = 0;
		float mid2Bottom = 0;

		float yTextureOffset = (float)(side->textures[ETexpart::Mid].yOffset / gameTexture->GetScaleY());

		if (side->line->flags & ML_DONTPEGBOTTOM)
		{
			yTextureOffset += (float)side->sector->planes[IntSector::floor].TexZ;
		}
		else
		{
			yTextureOffset += (float)(side->sector->planes[IntSector::ceiling].TexZ - gameTexture->GetDisplayHeight() / side->textures[ETexpart::Mid].yScale);
		}

		verts[0].Z = std::min(std::max(yTextureOffset + mid1Bottom, v1Bottom), v1Top);
		verts[1].Z = std::min(std::max(yTextureOffset + mid2Bottom, v2Bottom), v2Top);
		verts[2].Z = std::max(std::min(yTextureOffset + mid1Top, v1Top), v1Bottom);
		verts[3].Z = std::max(std::min(yTextureOffset + mid2Top, v2Top), v2Bottom);
	}

	// mid texture
	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.bSky = false;
	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;
	surf.bSky = false;
	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);

	FVector3 offset = surf.plane.XYZ() * 0.05f; // for better accuracy when raytracing mid-textures from each side

	if (side->line->sidedef[0] != side)
	{
		surf.plane = -surf.plane;
		surf.plane.W = -surf.plane.W;
	}

	MeshVertices.Push(verts[0] + offset);
	MeshVertices.Push(verts[1] + offset);
	MeshVertices.Push(verts[2] + offset);
	MeshVertices.Push(verts[3] + offset);

	surf.Type = ST_MIDDLESIDE;
	surf.TypeIndex = side->Index();
	surf.sampleDimension = side->textures[ETexpart::Mid].LightmapSampleDistance;
	surf.ControlSector = nullptr;
	surf.sectorGroup = sectorGroup[front->Index()];
	surf.texture = texture;
	surf.alpha = float(side->line->alpha);

	SetSideTextureUVs(surf, side, ETexpart::Top, verts[2].Z, verts[0].Z, verts[3].Z, verts[1].Z);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::Create3DFloorWallSurfaces(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = side->sector;
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	for (unsigned int j = 0; j < front->e->XFloor.ffloors.Size(); j++)
	{
		F3DFloor* xfloor = front->e->XFloor.ffloors[j];

		// Don't create a line when both sectors have the same 3d floor
		bool bothSides = false;
		for (unsigned int k = 0; k < back->e->XFloor.ffloors.Size(); k++)
		{
			if (back->e->XFloor.ffloors[k] == xfloor)
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
		surf.TypeIndex = side->Index();
		surf.ControlSector = xfloor->model;
		surf.bSky = false;
		surf.sampleDimension = side->textures[ETexpart::Mid].LightmapSampleDistance;

		float blZ = (float)xfloor->model->floorplane.ZatPoint(v1);
		float brZ = (float)xfloor->model->floorplane.ZatPoint(v2);
		float tlZ = (float)xfloor->model->ceilingplane.ZatPoint(v1);
		float trZ = (float)xfloor->model->ceilingplane.ZatPoint(v2);

		FVector3 verts[4];
		verts[0].X = verts[2].X = v2.X;
		verts[0].Y = verts[2].Y = v2.Y;
		verts[1].X = verts[3].X = v1.X;
		verts[1].Y = verts[3].Y = v1.Y;
		verts[0].Z = brZ;
		verts[1].Z = blZ;
		verts[2].Z = trZ;
		verts[3].Z = tlZ;

		surf.startVertIndex = MeshVertices.Size();
		surf.numVerts = 4;
		MeshVertices.Push(verts[0]);
		MeshVertices.Push(verts[1]);
		MeshVertices.Push(verts[2]);
		MeshVertices.Push(verts[3]);

		surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
		surf.sectorGroup = sectorGroup[front->Index()];
		surf.texture = side->textures[ETexpart::Mid].texture;

		SetSideTextureUVs(surf, side, ETexpart::Top, tlZ, blZ, trZ, brZ);

		Surfaces.Push(surf);
	}
}

void DoomLevelSubmesh::CreateTopWallSurface(FLevel& doomMap, IntSideDef* side)
{
	IntSector* front = &doomMap.Sectors[side->sector];
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v1TopBack = (float)back->ceilingplane.ZatPoint(v1);
	float v2TopBack = (float)back->ceilingplane.ZatPoint(v2);

	bool bSky = IsTopSideSky(front, back, side);
	if (!bSky && !IsTopSideVisible(side))
		return;

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;
	verts[0].Z = v1TopBack;
	verts[1].Z = v2TopBack;
	verts[2].Z = v1Top;
	verts[3].Z = v2Top;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;
	MeshVertices.Push(verts[0]);
	MeshVertices.Push(verts[1]);
	MeshVertices.Push(verts[2]);
	MeshVertices.Push(verts[3]);

	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
	surf.Type = ST_UPPERSIDE;
	surf.TypeIndex = side->Index();
	surf.bSky = bSky;
	surf.sampleDimension = side->textures[ETexpart::Top].LightmapSampleDistance;
	surf.ControlSector = nullptr;
	surf.sectorGroup = sectorGroup[front->Index()];
	surf.texture = side->textures[ETexpart::Top].texture;

	SetSideTextureUVs(surf, side, ETexpart::Top, v1Top, v1TopBack, v2Top, v2TopBack);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateBottomWallSurface(FLevel& doomMap, IntSideDef* side)
{
	if (!IsBottomSideVisible(side))
		return;

	IntSector* front = &doomMap.Sectors[side->sector];
	IntSector* back = (side->line->frontsector == front) ? side->line->backsector : side->line->frontsector;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);
	float v1BottomBack = (float)back->floorplane.ZatPoint(v1);
	float v2BottomBack = (float)back->floorplane.ZatPoint(v2);

	FVector3 verts[4];
	verts[0].X = verts[2].X = v1.X;
	verts[0].Y = verts[2].Y = v1.Y;
	verts[1].X = verts[3].X = v2.X;
	verts[1].Y = verts[3].Y = v2.Y;
	verts[0].Z = v1Bottom;
	verts[1].Z = v2Bottom;
	verts[2].Z = v1BottomBack;
	verts[3].Z = v2BottomBack;

	DoomLevelMeshSurface surf;
	surf.Submesh = this;
	surf.startVertIndex = MeshVertices.Size();
	surf.numVerts = 4;
	MeshVertices.Push(verts[0]);
	MeshVertices.Push(verts[1]);
	MeshVertices.Push(verts[2]);
	MeshVertices.Push(verts[3]);

	surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
	surf.Type = ST_LOWERSIDE;
	surf.TypeIndex = side->Index();
	surf.bSky = false;
	surf.sampleDimension = side->textures[ETexpart::Bottom].LightmapSampleDistance;
	surf.ControlSector = nullptr;
	surf.sectorGroup = sectorGroup[front->Index()];
	surf.texture = side->textures[ETexpart::Bottom].texture;

	SetSideTextureUVs(surf, side, ETexpart::Bottom, v1BottomBack, v1Bottom, v2BottomBack, v2Bottom);

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::SetSideTextureUVs(DoomLevelMeshSurface& surface, IntSideDef* side, ETexpart texpart, float v1TopZ, float v1BottomZ, float v2TopZ, float v2BottomZ)
{
	MeshVertexUVs.Reserve(4);
	FVector2* uvs = &MeshVertexUVs[surface.startVertIndex];

#if 0
	if (surface.texture.isValid())
	{
		const auto gtxt = TexMan.GetGameTexture(surface.texture);

		FTexCoordInfo tci;
		GetTexCoordInfo(gtxt, &tci, side, texpart);

		float startU = tci.FloatToTexU(tci.TextureOffset((float)side->GetTextureXOffset(texpart)) + tci.TextureOffset((float)side->GetTextureXOffset(texpart)));
		float endU = startU + tci.FloatToTexU(side->TexelLength);

		uvs[0].X = startU;
		uvs[1].X = endU;
		uvs[2].X = startU;
		uvs[3].X = endU;

		// To do: the ceiling version is apparently used in some situation related to 3d floors (rover->top.isceiling)
		//float offset = tci.RowOffset((float)side->GetTextureYOffset(texpart)) + tci.RowOffset((float)side->GetTextureYOffset(texpart)) + (float)side->sector->GetPlaneTexZ(IntSector::ceiling);
		float offset = tci.RowOffset((float)side->GetTextureYOffset(texpart)) + tci.RowOffset((float)side->GetTextureYOffset(texpart)) + (float)side->sector->GetPlaneTexZ(IntSector::floor);

		uvs[0].Y = tci.FloatToTexV(offset - v1BottomZ);
		uvs[1].Y = tci.FloatToTexV(offset - v2BottomZ);
		uvs[2].Y = tci.FloatToTexV(offset - v1TopZ);
		uvs[3].Y = tci.FloatToTexV(offset - v2TopZ);
	}
	else
#endif
	{
		uvs[0] = FVector2(0.0f, 0.0f);
		uvs[1] = FVector2(0.0f, 0.0f);
		uvs[2] = FVector2(0.0f, 0.0f);
		uvs[3] = FVector2(0.0f, 0.0f);
	}
}

void DoomLevelSubmesh::CreateFloorSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.Submesh = this;

	Plane plane;
	if (!controlSector)
	{
		plane = sector->floorplane;
		surf.bSky = IsSkySector(sector, IntSector::floor);
	}
	else
	{
		plane = controlSector->ceilingplane;
		plane.FlipVert();
		surf.bSky = false;
	}

	surf.numVerts = sub->numlines;
	surf.startVertIndex = MeshVertices.Size();
	surf.texture = (controlSector ? controlSector : sector)->planes[IntSector::floor].Texture;

	FGameTexture* txt = TexMan.GetGameTexture(surf.texture);
	float w = txt->GetDisplayWidth();
	float h = txt->GetDisplayHeight();
	VSMatrix mat = GetPlaneTextureRotationMatrix(txt, sector, IntSector::floor);

	MeshVertices.Resize(surf.startVertIndex + surf.numVerts);
	MeshVertexUVs.Resize(surf.startVertIndex + surf.numVerts);

	FVector3* verts = &MeshVertices[surf.startVertIndex];
	FVector2* uvs = &MeshVertexUVs[surf.startVertIndex];

	for (int j = 0; j < surf.numVerts; j++)
	{
		seg_t* seg = &sub->firstline[(surf.numVerts - 1) - j];
		FVector2 v1 = ToFVector2(seg->v1->fPos());

		verts[j].X = v1.X;
		verts[j].Y = v1.Y;
		verts[j].Z = (float)plane.ZatPoint(verts[j]);

		uvs[j] = (mat * FVector4(v1.X / 64.f, -v1.Y / 64.f, 0.f, 1.f)).XY(); // The magic 64.f and negative Y is based on SetFlatVertex
	}

	surf.Type = ST_FLOOR;
	surf.TypeIndex = typeIndex;
	surf.sampleDimension = (controlSector ? controlSector : sector)->planes[IntSector::floor].LightmapSampleDistance;
	surf.ControlSector = controlSector;
	surf.plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.D);
	surf.sectorGroup = sectorGroup[sector->Index()];

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateCeilingSurface(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, IntSector* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.Submesh = this;

	Plane plane;
	if (!controlSector)
	{
		plane = sector->ceilingplane;
		surf.bSky = IsSkySector(sector, IntSector::ceiling);
	}
	else
	{
		plane = controlSector->floorplane;
		plane.FlipVert();
		surf.bSky = false;
	}

	surf.numVerts = sub->numlines;
	surf.startVertIndex = MeshVertices.Size();
	surf.texture = (controlSector ? controlSector : sector)->planes[IntSector::ceiling].Texture;

	FGameTexture* txt = TexMan.GetGameTexture(surf.texture);
	float w = txt->GetDisplayWidth();
	float h = txt->GetDisplayHeight();
	VSMatrix mat = GetPlaneTextureRotationMatrix(txt, sector, IntSector::ceiling);

	MeshVertices.Resize(surf.startVertIndex + surf.numVerts);
	MeshVertexUVs.Resize(surf.startVertIndex + surf.numVerts);

	FVector3* verts = &MeshVertices[surf.startVertIndex];
	FVector2* uvs = &MeshVertexUVs[surf.startVertIndex];

	for (int j = 0; j < surf.numVerts; j++)
	{
		MapSegGLEx* seg = &doomMap.GLSegs[sub->firstline + j];
		FVector2 v1 = ToFVector2(seg->v1->fPos());

		verts[j].X = v1.X;
		verts[j].Y = v1.Y;
		verts[j].Z = (float)plane.ZatPoint(verts[j]);

		uvs[j] = (mat * FVector4(v1.X / 64.f, -v1.Y / 64.f, 0.f, 1.f)).XY(); // The magic 64.f and negative Y is based on SetFlatVertex
	}

	surf.Type = ST_CEILING;
	surf.TypeIndex = typeIndex;
	surf.sampleDimension = (controlSector ? controlSector : sector)->planes[IntSector::ceiling].LightmapSampleDistance;
	surf.ControlSector = controlSector;
	surf.plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.D);
	surf.sectorGroup = sectorGroup[sector->Index()];

	Surfaces.Push(surf);
}

void DoomLevelSubmesh::CreateSubsectorSurfaces(FLevel& doomMap)
{
	for (unsigned int i = 0; i < doomMap.GLSubsectors.Size(); i++)
	{
		MapSubsectorEx* sub = &doomMap.GLSubsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		IntSector* sector = sub->sector;
		if (!sector)
			continue;

		CreateFloorSurface(doomMap, sub, sector, nullptr, i);
		CreateCeilingSurface(doomMap, sub, sector, nullptr, i);

		for (unsigned int j = 0; j < sector->e->XFloor.ffloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector, sector->e->XFloor.ffloors[j]->model, i);
			CreateCeilingSurface(doomMap, sub, sector, sector->e->XFloor.ffloors[j]->model, i);
		}
	}
}

bool DoomLevelSubmesh::IsTopSideSky(IntSector* frontsector, IntSector* backsector, IntSideDef* side)
{
	return IsSkySector(frontsector, IntSector::ceiling) && IsSkySector(backsector, IntSector::ceiling);
}

bool DoomLevelSubmesh::IsTopSideVisible(IntSideDef* side)
{
	auto tex = TexMan.GetGameTexture(side->GetTexture(ETexpart::Top), true);
	return tex && tex->isValid();
}

bool DoomLevelSubmesh::IsBottomSideVisible(IntSideDef* side)
{
	auto tex = TexMan.GetGameTexture(side->GetTexture(ETexpart::Bottom), true);
	return tex && tex->isValid();
}

bool DoomLevelSubmesh::IsSkySector(IntSector* sector, int plane)
{
	// plane is either IntSector::ceiling or IntSector::floor
	return sector->GetTexture(plane) == skyflatnum;
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

void DoomLevelSubmesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
	auto f = fopen(objFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");
	fprintf(f, "# MeshVertices: %u, MeshElements: %u, Surfaces: %u\n", MeshVertices.Size(), MeshElements.Size(), Surfaces.Size());
	fprintf(f, "mtllib %s\n", mtlFilename.GetChars());

	double scale = 1 / 10.0;

	for (const auto& v : MeshVertices)
	{
		fprintf(f, "v %f %f %f\n", v.X * scale, v.Y * scale, v.Z * scale);
	}

	{
		for (const auto& uv : LightmapUvs)
		{
			fprintf(f, "vt %f %f\n", uv.X, uv.Y);
		}
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
		case ST_UNKNOWN:
			return "unknown";
		default:
			break;
		}
		return "error";
		};


	uint32_t lastSurfaceIndex = -1;


	bool useErrorMaterial = false;
	int highestUsedAtlasPage = -1;

	for (unsigned i = 0, count = MeshElements.Size(); i + 2 < count; i += 3)
	{
		auto index = MeshSurfaceIndexes[i / 3];

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
				fprintf(f, "o Surface[%d] %s %d%s\n", index, name(surface.Type), surface.TypeIndex, surface.bSky ? " sky" : "");
				fprintf(f, "usemtl lightmap%d\n", surface.AtlasTile.ArrayIndex);

				if (surface.AtlasTile.ArrayIndex > highestUsedAtlasPage)
				{
					highestUsedAtlasPage = surface.AtlasTile.ArrayIndex;
				}
			}
		}

		// fprintf(f, "f %d %d %d\n", MeshElements[i] + 1, MeshElements[i + 1] + 1, MeshElements[i + 2] + 1);
		fprintf(f, "f %d/%d %d/%d %d/%d\n",
			MeshElements[i + 0] + 1, MeshElements[i + 0] + 1,
			MeshElements[i + 1] + 1, MeshElements[i + 1] + 1,
			MeshElements[i + 2] + 1, MeshElements[i + 2] + 1);

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

void DoomLevelSubmesh::SetupLightmapUvs(FLevel& doomMap)
{
	LMTextureSize = 1024; // TODO cvar

	for (auto& surface : Surfaces)
	{
		BuildSurfaceParams(LMTextureSize, LMTextureSize, surface);
	}
}

void DoomLevelSubmesh::PackLightmapAtlas(int lightmapStartIndex)
{
	std::vector<LevelMeshSurface*> sortedSurfaces;
	sortedSurfaces.reserve(Surfaces.Size());

	for (auto& surface : Surfaces)
	{
		sortedSurfaces.push_back(&surface);
	}

	std::sort(sortedSurfaces.begin(), sortedSurfaces.end(), [](LevelMeshSurface* a, LevelMeshSurface* b) { return a->AtlasTile.Height != b->AtlasTile.Height ? a->AtlasTile.Height > b->AtlasTile.Height : a->AtlasTile.Width > b->AtlasTile.Width; });

	RectPacker packer(LMTextureSize, LMTextureSize, RectPacker::Spacing(0));

	for (LevelMeshSurface* surf : sortedSurfaces)
	{
		int sampleWidth = surf->AtlasTile.Width;
		int sampleHeight = surf->AtlasTile.Height;

		auto result = packer.insert(sampleWidth, sampleHeight);
		int x = result.pos.x, y = result.pos.y;

		surf->AtlasTile.X = x;
		surf->AtlasTile.Y = y;
		surf->AtlasTile.ArrayIndex = lightmapStartIndex + (int)result.pageIndex;

		// calculate final texture coordinates
		for (int i = 0; i < (int)surf->numVerts; i++)
		{
			auto& u = LightmapUvs[surf->startUvIndex + i].X;
			auto& v = LightmapUvs[surf->startUvIndex + i].Y;
			u = (u + x) / (float)LMTextureSize;
			v = (v + y) / (float)LMTextureSize;
		}
	}

	LMTextureCount = (int)packer.getNumPages();
}

BBox DoomLevelSubmesh::GetBoundsFromSurface(const LevelMeshSurface& surface) const
{
	constexpr float M_INFINITY = 1e30f; // TODO cleanup

	FVector3 low(M_INFINITY, M_INFINITY, M_INFINITY);
	FVector3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

	for (int i = int(surface.startVertIndex); i < int(surface.startVertIndex) + surface.numVerts; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (MeshVertices[i][j] < low[j])
			{
				low[j] = MeshVertices[i][j];
			}
			if (MeshVertices[i][j] > hi[j])
			{
				hi[j] = MeshVertices[i][j];
			}
		}
	}

	BBox bounds;
	bounds.Clear();
	bounds.min = low;
	bounds.max = hi;
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

void DoomLevelSubmesh::BuildSurfaceParams(int lightMapTextureWidth, int lightMapTextureHeight, LevelMeshSurface& surface)
{
	BBox bounds = GetBoundsFromSurface(surface);
	surface.bounds = bounds;

	if (surface.sampleDimension <= 0)
	{
		surface.sampleDimension = LightmapSampleDistance;
	}

	surface.sampleDimension = uint16_t(std::max(int(roundf(float(surface.sampleDimension) / std::max(1.0f / 4, float(lm_scale)))), 1));

	{
		// Round to nearest power of two
		uint32_t n = uint16_t(surface.sampleDimension);
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n = (n + 1) >> 1;
		surface.sampleDimension = uint16_t(n) ? uint16_t(n) : uint16_t(0xFFFF);
	}

	// round off dimensions
	FVector3 roundedSize;
	for (int i = 0; i < 3; i++)
	{
		bounds.min[i] = surface.sampleDimension * (floor(bounds.min[i] / surface.sampleDimension) - 1);
		bounds.max[i] = surface.sampleDimension * (ceil(bounds.max[i] / surface.sampleDimension) + 1);
		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / surface.sampleDimension;
	}

	FVector3 tCoords[2] = { FVector3(0.0f, 0.0f, 0.0f), FVector3(0.0f, 0.0f, 0.0f) };

	PlaneAxis axis = BestAxis(surface.plane);

	int width;
	int height;
	switch (axis)
	{
	default:
	case AXIS_YZ:
		width = (int)roundedSize.Y;
		height = (int)roundedSize.Z;
		tCoords[0].Y = 1.0f / surface.sampleDimension;
		tCoords[1].Z = 1.0f / surface.sampleDimension;
		break;

	case AXIS_XZ:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Z;
		tCoords[0].X = 1.0f / surface.sampleDimension;
		tCoords[1].Z = 1.0f / surface.sampleDimension;
		break;

	case AXIS_XY:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Y;
		tCoords[0].X = 1.0f / surface.sampleDimension;
		tCoords[1].Y = 1.0f / surface.sampleDimension;
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

	surface.translateWorldToLocal = bounds.min;
	surface.projLocalToU = tCoords[0];
	surface.projLocalToV = tCoords[1];

	surface.startUvIndex = AllocUvs(surface.numVerts);

	for (int i = 0; i < surface.numVerts; i++)
	{
		FVector3 tDelta = MeshVertices[surface.startVertIndex + i] - surface.translateWorldToLocal;

		LightmapUvs[surface.startUvIndex + i].X = (tDelta | surface.projLocalToU);
		LightmapUvs[surface.startUvIndex + i].Y = (tDelta | surface.projLocalToV);
	}

	// project tCoords so they lie on the plane
	const FVector4& plane = surface.plane;
	float d = ((bounds.min | FVector3(plane.X, plane.Y, plane.Z)) - plane.W) / plane[axis]; //d = (plane->PointToDist(bounds.min)) / plane->Normal()[axis];
	for (int i = 0; i < 2; i++)
	{
		tCoords[i].MakeUnit();
		d = (tCoords[i] | FVector3(plane.X, plane.Y, plane.Z)) / plane[axis]; //d = dot(tCoords[i], plane->Normal()) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface.AtlasTile.Width = width;
	surface.AtlasTile.Height = height;
}

#endif
