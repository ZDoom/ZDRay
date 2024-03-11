
#include "hw_levelmesh.h"

LevelMesh::LevelMesh()
{
	// Default portal
	LevelMeshPortal portal;
	Portals.Push(portal);

	AddEmptyMesh();
	UpdateCollision();

	Mesh.MaxVertices = std::max(Mesh.Vertices.Size() * 2, (unsigned int)10000);
	Mesh.MaxIndexes = std::max(Mesh.Indexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxSurfaces = std::max(Mesh.SurfaceIndexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxUniforms = std::max(Mesh.Uniforms.Size() * 2, (unsigned int)10000);
	Mesh.MaxSurfaceIndexes = std::max(Mesh.SurfaceIndexes.Size() * 2, (unsigned int)10000);
	Mesh.MaxNodes = (int)std::max(Collision->get_nodes().size() * 2, (size_t)10000);
	Mesh.MaxLights = 100'000;
	Mesh.MaxLightIndexes = 4 * 1024 * 1024;
}

void LevelMesh::AddEmptyMesh()
{
	// Default empty mesh (we can't make it completely empty since vulkan doesn't like that)
	float minval = -100001.0f;
	float maxval = -100000.0f;
	Mesh.Vertices.Push({ minval, minval, minval });
	Mesh.Vertices.Push({ maxval, minval, minval });
	Mesh.Vertices.Push({ maxval, maxval, minval });
	Mesh.Vertices.Push({ minval, minval, minval });
	Mesh.Vertices.Push({ minval, maxval, minval });
	Mesh.Vertices.Push({ maxval, maxval, minval });
	Mesh.Vertices.Push({ minval, minval, maxval });
	Mesh.Vertices.Push({ maxval, minval, maxval });
	Mesh.Vertices.Push({ maxval, maxval, maxval });
	Mesh.Vertices.Push({ minval, minval, maxval });
	Mesh.Vertices.Push({ minval, maxval, maxval });
	Mesh.Vertices.Push({ maxval, maxval, maxval });

	for (int i = 0; i < 3 * 4; i++)
		Mesh.Indexes.Push(i);
}

LevelMeshSurface* LevelMesh::Trace(const FVector3& start, FVector3 direction, float maxDist)
{
	maxDist = std::max(maxDist - 10.0f, 0.0f);

	FVector3 origin = start;

	LevelMeshSurface* hitSurface = nullptr;

	while (true)
	{
		FVector3 end = origin + direction * maxDist;

		TraceHit hit = TriangleMeshShape::find_first_hit(Collision.get(), origin, end);

		if (hit.triangle < 0)
		{
			return nullptr;
		}

		hitSurface = GetSurface(Mesh.SurfaceIndexes[hit.triangle]);

		int portal = hitSurface->PortalIndex;
		if (!portal)
		{
			break;
		}

		auto& transformation = Portals[portal];

		auto travelDist = hit.fraction * maxDist + 2.0f;
		if (travelDist >= maxDist)
		{
			break;
		}

		origin = transformation.TransformPosition(origin + direction * travelDist);
		direction = transformation.TransformRotation(direction);
		maxDist -= travelDist;
	}

	return hitSurface; // I hit something
}

LevelMeshTileStats LevelMesh::GatherTilePixelStats()
{
	LevelMeshTileStats stats;
	int count = GetSurfaceCount();
	for (const LightmapTile& tile : LightmapTiles)
	{
		auto area = tile.AtlasLocation.Area();

		stats.pixels.total += area;

		if (tile.NeedsUpdate)
		{
			stats.tiles.dirty++;
			stats.pixels.dirty += area;
		}
	}
	stats.tiles.total += LightmapTiles.Size();
	return stats;
}

void LevelMesh::UpdateCollision()
{
	Collision = std::make_unique<TriangleMeshShape>(Mesh.Vertices.Data(), Mesh.Vertices.Size(), Mesh.Indexes.Data(), Mesh.Indexes.Size());
}

struct LevelMeshPlaneGroup
{
	FVector4 plane = FVector4(0, 0, 1, 0);
	int sectorGroup = 0;
	std::vector<LevelMeshSurface*> surfaces;
};

void LevelMesh::BuildTileSurfaceLists()
{
	// Plane group surface is to be rendered with
	TArray<LevelMeshPlaneGroup> PlaneGroups;
	TArray<int> PlaneGroupIndexes(GetSurfaceCount());

	for (int i = 0, count = GetSurfaceCount(); i < count; i++)
	{
		auto surface = GetSurface(i);

		// Is this surface in the same plane as an existing plane group?
		int planeGroupIndex = -1;

		for (size_t j = 0; j < PlaneGroups.Size(); j++)
		{
			if (surface->SectorGroup == PlaneGroups[j].sectorGroup)
			{
				float direction = PlaneGroups[j].plane.XYZ() | surface->Plane.XYZ();
				if (direction >= 0.999f && direction <= 1.01f)
				{
					auto point = (surface->Plane.XYZ() * surface->Plane.W);
					auto planeDistance = (PlaneGroups[j].plane.XYZ() | point) - PlaneGroups[j].plane.W;

					float dist = std::abs(planeDistance);
					if (dist <= 0.1f)
					{
						planeGroupIndex = (int)j;
						break;
					}
				}
			}
		}

		// Surface is in a new plane. Create a plane group for it
		if (planeGroupIndex == -1)
		{
			planeGroupIndex = PlaneGroups.Size();

			LevelMeshPlaneGroup group;
			group.plane = surface->Plane;
			group.sectorGroup = surface->SectorGroup;
			PlaneGroups.Push(group);
		}

		PlaneGroups[planeGroupIndex].surfaces.push_back(surface);
		PlaneGroupIndexes.Push(planeGroupIndex);
	}

	for (auto& tile : LightmapTiles)
		tile.Surfaces.Clear();

	for (int i = 0, count = GetSurfaceCount(); i < count; i++)
	{
		LevelMeshSurface* targetSurface = GetSurface(i);
		if (targetSurface->LightmapTileIndex < 0)
			continue;
		LightmapTile* tile = &LightmapTiles[targetSurface->LightmapTileIndex];
		for (LevelMeshSurface* surface : PlaneGroups[PlaneGroupIndexes[i]].surfaces)
		{
			FVector2 minUV = tile->ToUV(surface->Bounds.min);
			FVector2 maxUV = tile->ToUV(surface->Bounds.max);
			if (surface != targetSurface && (maxUV.X < 0.0f || maxUV.Y < 0.0f || minUV.X > 1.0f || minUV.Y > 1.0f))
				continue; // Bounding box not visible

			tile->Surfaces.Push(GetSurfaceIndex(surface));
		}
	}
}

void LevelMesh::SetupTileTransforms()
{
	for (auto& tile : LightmapTiles)
	{
		tile.SetupTileTransform(LMTextureSize);
	}
}

void LevelMesh::PackLightmapAtlas(int lightmapStartIndex)
{
	std::vector<LightmapTile*> sortedTiles;
	sortedTiles.reserve(LightmapTiles.Size());

	for (auto& tile : LightmapTiles)
	{
		sortedTiles.push_back(&tile);
	}

	std::sort(sortedTiles.begin(), sortedTiles.end(), [](LightmapTile* a, LightmapTile* b) { return a->AtlasLocation.Height != b->AtlasLocation.Height ? a->AtlasLocation.Height > b->AtlasLocation.Height : a->AtlasLocation.Width > b->AtlasLocation.Width; });

	// We do not need to add spacing here as this is already built into the tile size itself.
	RectPacker packer(LMTextureSize, LMTextureSize, RectPacker::Spacing(0), RectPacker::Padding(0));

	for (LightmapTile* tile : sortedTiles)
	{
		auto result = packer.insert(tile->AtlasLocation.Width, tile->AtlasLocation.Height);
		tile->AtlasLocation.X = result.pos.x;
		tile->AtlasLocation.Y = result.pos.y;
		tile->AtlasLocation.ArrayIndex = lightmapStartIndex + (int)result.pageIndex;
	}

	LMTextureCount = (int)packer.getNumPages();

	// Calculate final texture coordinates
	for (int i = 0, count = GetSurfaceCount(); i < count; i++)
	{
		auto surface = GetSurface(i);
		if (surface->LightmapTileIndex >= 0)
		{
			const LightmapTile& tile = LightmapTiles[surface->LightmapTileIndex];
			for (int i = 0; i < surface->MeshLocation.NumVerts; i++)
			{
				auto& vertex = Mesh.Vertices[surface->MeshLocation.StartVertIndex + i];
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
	LMTextureData.Resize(LMTextureSize * LMTextureSize * LMTextureCount * 3);
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
			uint16_t* line = pixels + tile.AtlasLocation.ArrayIndex * LMTextureSize * LMTextureSize + yy * LMTextureSize * 3;
			for (int xx = x; xx < x + w; xx++)
			{
				float gray = (yy - y) / (float)h;
				line[xx * 3] = floatToHalf(color[0] * gray);
				line[xx * 3 + 1] = floatToHalf(color[1] * gray);
				line[xx * 3 + 2] = floatToHalf(color[2] * gray);
			}
		}
	}
	for (int i = 0, count = GetSurfaceCount(); i < count; i++)
	{
		auto surface = GetSurface(i);
		surface->AlwaysUpdate = false;
	}
#endif
}
