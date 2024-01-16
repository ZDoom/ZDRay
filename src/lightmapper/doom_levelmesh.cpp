
#include "doom_levelmesh.h"
#include "doom_levelsubmesh.h"
#include "level/level.h"
#include "framework/halffloat.h"
#include "framework/binfile.h"
#include <algorithm>
#include <map>
#include <set>

DoomLevelMesh::DoomLevelMesh(FLevel& doomMap)
{
	SunColor = doomMap.defaultSunColor; // TODO keep only one copy?
	SunDirection = doomMap.defaultSunDirection;

	BuildSectorGroups(doomMap);
	CreatePortals(doomMap);

	StaticMesh = std::make_unique<DoomLevelSubmesh>(this, doomMap, true);
	DynamicMesh = std::make_unique<DoomLevelSubmesh>(this, doomMap, false);

	BuildLightLists(doomMap);
}

void DoomLevelMesh::BeginFrame(FLevel& doomMap)
{
	static_cast<DoomLevelSubmesh*>(DynamicMesh.get())->Update(doomMap, static_cast<DoomLevelSubmesh*>(StaticMesh.get())->LMTextureCount);
}

bool DoomLevelMesh::TraceSky(const FVector3& start, FVector3 direction, float dist)
{
	FVector3 end = start + direction * dist;
	auto surface = Trace(start, direction, dist);
	return surface && surface->IsSky;
}

int DoomLevelMesh::AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize)
{
	const DoomLevelMeshSurface* doomsurface = static_cast<const DoomLevelMeshSurface*>(surface);
	int listpos = 0;
	for (ThingLight* light : doomsurface->LightList)
	{
		if (listpos == listMaxSize)
			break;

		LevelMeshLight& meshlight = list[listpos++];
		meshlight.Origin = light->LightOrigin();
		meshlight.RelativeOrigin = light->LightRelativeOrigin();
		meshlight.Radius = light->LightRadius();
		meshlight.Intensity = light->intensity;
		meshlight.InnerAngleCos = light->innerAngleCos;
		meshlight.OuterAngleCos = light->outerAngleCos;
		meshlight.SpotDir = light->SpotDir();
		meshlight.Color = light->rgb;

		/*if (light->sector)
			meshlight.SectorGroup = static_cast<DoomLevelSubmesh*>(StaticMesh.get())->sectorGroup[light->sector->Index(doomMap)];
		else*/
		meshlight.SectorGroup = 0;
	}
	return listpos;
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

	auto submesh = static_cast<DoomLevelSubmesh*>(StaticMesh.get());

	for (unsigned int i = 0; i < submesh->LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &submesh->LightmapTiles[i];
		if (tile->AtlasLocation.ArrayIndex != -1)
		{
			tileCount++;
			pixelCount += tile->AtlasLocation.Area();
		}
	}

	printf("   Writing %u tiles out of %llu\n", tileCount, (size_t)submesh->LightmapTiles.Size());

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

	for (unsigned int i = 0; i < submesh->LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &submesh->LightmapTiles[i];

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
	for (unsigned int i = 0; i < submesh->LightmapTiles.Size(); i++)
	{
		LightmapTile* tile = &submesh->LightmapTiles[i];

		if (tile->AtlasLocation.ArrayIndex == -1)
			continue;

		const uint16_t* pixels = submesh->LMTextureData.Data() + tile->AtlasLocation.ArrayIndex * submesh->LMTextureSize * submesh->LMTextureSize * 4;
		int width = tile->AtlasLocation.Width;
		int height = tile->AtlasLocation.Height;
		for (int y = 0; y < height; y++)
		{
			const uint16_t* srcline = pixels + (tile->AtlasLocation.X + (tile->AtlasLocation.Y + y) * submesh->LMTextureSize) * 4;
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

void DoomLevelMesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
	DoomLevelSubmesh* submesh = static_cast<DoomLevelSubmesh*>(StaticMesh.get());

	auto f = fopen(objFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");
	fprintf(f, "# Vertices: %u, Indexes: %u, Surfaces: %u\n", submesh->Mesh.Vertices.Size(), submesh->Mesh.Indexes.Size(), submesh->Surfaces.Size());
	fprintf(f, "mtllib %s\n", mtlFilename.GetChars());

	double scale = 1 / 10.0;

	for (const auto& v : submesh->Mesh.Vertices)
	{
		fprintf(f, "v %f %f %f\n", v.x * scale, v.y * scale, v.z * scale);
	}

	for (const auto& v : submesh->Mesh.Vertices)
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

	for (unsigned i = 0, count = submesh->Mesh.Indexes.Size(); i + 2 < count; i += 3)
	{
		auto index = submesh->Mesh.SurfaceIndexes[i / 3];

		if (index != lastSurfaceIndex)
		{
			lastSurfaceIndex = index;

			if (unsigned(index) >= submesh->Surfaces.Size())
			{
				fprintf(f, "o Surface[%d] (bad index)\n", index);
				fprintf(f, "usemtl error\n");

				useErrorMaterial = true;
			}
			else
			{
				const auto& surface = submesh->Surfaces[index];
				fprintf(f, "o Surface[%d] %s %d%s\n", index, name(surface.Type), surface.TypeIndex, surface.IsSky ? " sky" : "");

				if (surface.LightmapTileIndex >= 0)
				{
					auto& tile = submesh->LightmapTiles[surface.LightmapTileIndex];
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
			submesh->Mesh.Indexes[i + 0] + 1, submesh->Mesh.Indexes[i + 0] + 1,
			submesh->Mesh.Indexes[i + 1] + 1, submesh->Mesh.Indexes[i + 1] + 1,
			submesh->Mesh.Indexes[i + 2] + 1, submesh->Mesh.Indexes[i + 2] + 1);

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
				VSMatrix transformation;
				transformation.loadIdentity();
				transformation.translate((float)d.X, (float)d.Y, 0.0f);

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
		IntLineDef* sourceLine = &doomMap.Lines[i];
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

			transformation.rotate((float)sourceLine->getPortalAngleDiff().Degrees(), 0.0f, 0.0f, 1.0f);
			transformation.translate((float)(targetXYZ.X - sourceXYZ.X), (float)(targetXYZ.Y - sourceXYZ.Y), (float)z);

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

void DoomLevelMesh::BuildLightLists(FLevel& doomMap)
{
	for (unsigned i = 0; i < doomMap.ThingLights.Size(); ++i)
	{
		printf("   Building light lists: %u / %u\r", i, doomMap.ThingLights.Size());
		PropagateLight(doomMap, &doomMap.ThingLights[i]);
	}

	printf("   Building light lists: %u / %u\n", doomMap.ThingLights.Size(), doomMap.ThingLights.Size());
}

void DoomLevelMesh::PropagateLight(FLevel& doomMap, ThingLight* light, int recursiveDepth)
{
	if (recursiveDepth > 32)
		return;

	auto submesh = static_cast<DoomLevelSubmesh*>(StaticMesh.get());

	SphereShape sphere;
	sphere.center = light->LightRelativeOrigin();
	sphere.radius = light->LightRadius();
	//std::set<Portal, RecursivePortalComparator> portalsToErase;
	for (int triangleIndex : TriangleMeshShape::find_all_hits(submesh->Collision.get(), &sphere))
	{
		DoomLevelMeshSurface* surface = &submesh->Surfaces[submesh->Mesh.SurfaceIndexes[triangleIndex]];

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
			for (ThingLight* light2 : surface->LightList)
			{
				if (light2 == light)
				{
					found = true;
					break;
				}
			}
			if (!found)
				surface->LightList.push_back(light);
		}
	}

	/*for (auto& portal : portalsToErase)
	{
		touchedPortals.erase(portal);
	}*/
}