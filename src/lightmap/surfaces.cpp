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

#include "math/mathlib.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include "framework/binfile.h"
#include "level/level.h"
#include "surfaces.h"
#include "pngwriter.h"
#include <map>

extern float GridSize;

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

LevelMesh::LevelMesh(FLevel &doomMap, int sampleDistance, int textureSize)
{
	map = &doomMap;
	samples = sampleDistance;
	textureWidth = textureSize;
	textureHeight = textureSize;

	printf("------------- Building side surfaces -------------\n");

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
		printf("sides: %i / %i\r", i + 1, doomMap.Sides.Size());
	}

	printf("\nSide surfaces: %i\n", (int)surfaces.size());

	CreateSubsectorSurfaces(doomMap);

	printf("Surfaces total: %i\n\n", (int)surfaces.size());

	printf("Building level mesh..\n\n");

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		const auto &s = surfaces[i];
		int numVerts = s->numVerts;
		unsigned int pos = MeshVertices.Size();

		for (int j = 0; j < numVerts; j++)
		{
			MeshVertices.Push(s->verts[j]);
			MeshUVIndex.Push(j);
		}

		if (s->type == ST_FLOOR || s->type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(s->verts[0], s->verts[j - 1], s->verts[j]))
				{
					MeshElements.Push(pos);
					MeshElements.Push(pos + j - 1);
					MeshElements.Push(pos + j);
					MeshSurfaces.Push(i);
				}
			}
		}
		else if (s->type == ST_MIDDLESIDE || s->type == ST_UPPERSIDE || s->type == ST_LOWERSIDE)
		{
			if (!IsDegenerate(s->verts[0], s->verts[1], s->verts[2]))
			{
				MeshElements.Push(pos + 0);
				MeshElements.Push(pos + 1);
				MeshElements.Push(pos + 2);
				MeshSurfaces.Push(i);
			}
			if (!IsDegenerate(s->verts[1], s->verts[2], s->verts[3]))
			{
				MeshElements.Push(pos + 3);
				MeshElements.Push(pos + 2);
				MeshElements.Push(pos + 1);
				MeshSurfaces.Push(i);
			}
		}
	}

	CreateLightProbes(doomMap);

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		BuildSurfaceParams(surfaces[i].get());
	}
}

// Determines a lightmap block in which to map to the lightmap texture.
// Width and height of the block is calcuated and steps are computed to determine where each texel will be positioned on the surface
void LevelMesh::BuildSurfaceParams(Surface* surface)
{
	Plane* plane;
	BBox bounds;
	Vec3 roundedSize;
	int i;
	Plane::PlaneAxis axis;
	Vec3 tCoords[2];
	Vec3 tOrigin;
	int width;
	int height;
	float d;

	plane = &surface->plane;
	bounds = GetBoundsFromSurface(surface);

	// round off dimentions
	for (i = 0; i < 3; i++)
	{
		bounds.min[i] = samples * Math::Floor(bounds.min[i] / samples);
		bounds.max[i] = samples * Math::Ceil(bounds.max[i] / samples);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / samples + 1;
	}

	tCoords[0].Clear();
	tCoords[1].Clear();

	axis = plane->BestAxis();

	switch (axis)
	{
	case Plane::AXIS_YZ:
		width = (int)roundedSize.y;
		height = (int)roundedSize.z;
		tCoords[0].y = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case Plane::AXIS_XZ:
		width = (int)roundedSize.x;
		height = (int)roundedSize.z;
		tCoords[0].x = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case Plane::AXIS_XY:
		width = (int)roundedSize.x;
		height = (int)roundedSize.y;
		tCoords[0].x = 1.0f / samples;
		tCoords[1].y = 1.0f / samples;
		break;
	}

	// clamp width
	if (width > textureWidth)
	{
		tCoords[0] *= ((float)textureWidth / (float)width);
		width = textureWidth;
	}

	// clamp height
	if (height > textureHeight)
	{
		tCoords[1] *= ((float)textureHeight / (float)height);
		height = textureHeight;
	}

	surface->lightmapCoords.resize(surface->numVerts * 2);

	surface->textureCoords[0] = tCoords[0];
	surface->textureCoords[1] = tCoords[1];

	tOrigin = bounds.min;

	// project tOrigin and tCoords so they lie on the plane
	d = (plane->Distance(bounds.min) - plane->d) / plane->Normal()[axis];
	tOrigin[axis] -= d;

	for (i = 0; i < 2; i++)
	{
		tCoords[i].Normalize();
		d = plane->Distance(tCoords[i]) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface->bounds = bounds;
	surface->lightmapDims[0] = width;
	surface->lightmapDims[1] = height;
	surface->lightmapOrigin = tOrigin;
	surface->lightmapSteps[0] = tCoords[0] * (float)samples;
	surface->lightmapSteps[1] = tCoords[1] * (float)samples;

	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];
	surface->samples.resize(sampleWidth * sampleHeight);
	surface->indirect.resize(sampleWidth * sampleHeight);
}

BBox LevelMesh::GetBoundsFromSurface(const Surface* surface)
{
	Vec3 low(M_INFINITY, M_INFINITY, M_INFINITY);
	Vec3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

	BBox bounds;
	bounds.Clear();

	for (int i = 0; i < surface->numVerts; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (surface->verts[i][j] < low[j])
			{
				low[j] = surface->verts[i][j];
			}
			if (surface->verts[i][j] > hi[j])
			{
				hi[j] = surface->verts[i][j];
			}
		}
	}

	bounds.min = low;
	bounds.max = hi;

	return bounds;
}

void LevelMesh::CreateTextures()
{
	for (auto& surf : surfaces)
	{
		FinishSurface(surf.get());
	}
}

void LevelMesh::FinishSurface(Surface* surface)
{
	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];
	Vec3* colorSamples = surface->samples.data();

	if (!surface->indirect.empty())
	{
		Vec3* indirect = surface->indirect.data();
		for (int i = 0; i < sampleHeight; i++)
		{
			for (int j = 0; j < sampleWidth; j++)
			{
				colorSamples[i * sampleWidth + j] += indirect[i * sampleWidth + j] * 0.5f;
			}
		}
	}

	// SVE redraws the scene for lightmaps, so for optimizations,
	// tell the engine to ignore this surface if completely black
	bool bShouldLookupTexture = false;
	for (int i = 0; i < sampleHeight; i++)
	{
		for (int j = 0; j < sampleWidth; j++)
		{
			const auto& c = colorSamples[i * sampleWidth + j];
			if (c.x > 0.0f || c.y > 0.0f || c.z > 0.0f)
			{
				bShouldLookupTexture = true;
				break;
			}
		}
	}

	if (bShouldLookupTexture == false)
	{
		surface->lightmapNum = -1;
	}
	else
	{
		int x = 0, y = 0;
		uint16_t* currentTexture = AllocTextureRoom(surface, &x, &y);

		// calculate texture coordinates
		for (int i = 0; i < surface->numVerts; i++)
		{
			Vec3 tDelta = surface->verts[i] - surface->bounds.min;
			surface->lightmapCoords[i * 2 + 0] = (tDelta.Dot(surface->textureCoords[0]) + x + 0.5f) / (float)textureWidth;
			surface->lightmapCoords[i * 2 + 1] = (tDelta.Dot(surface->textureCoords[1]) + y + 0.5f) / (float)textureHeight;
		}

		surface->lightmapOffs[0] = x;
		surface->lightmapOffs[1] = y;

#if 1
		// store results to lightmap texture
		float weights[9] = { 0.125f, 0.25f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.25f, 0.125f };
		for (int y = 0; y < sampleHeight; y++)
		{
			Vec3* src = &colorSamples[y * sampleWidth];
			for (int x = 0; x < sampleWidth; x++)
			{
				// gaussian blur with a 3x3 kernel
				Vec3 color = { 0.0f };
				for (int yy = -1; yy <= 1; yy++)
				{
					int yyy = clamp(y + yy, 0, sampleHeight - 1) - y;
					for (int xx = -1; xx <= 1; xx++)
					{
						int xxx = clamp(x + xx, 0, sampleWidth - 1);
						color += src[yyy * sampleWidth + xxx] * weights[4 + xx + yy * 3];
					}
				}
				color *= 0.5f;

				// get texture offset
				int offs = (((textureWidth * (y + surface->lightmapOffs[1])) + surface->lightmapOffs[0]) * 3);

				// convert RGB to bytes
				currentTexture[offs + x * 3 + 0] = floatToHalf(colorSamples[y * sampleWidth + x].x);
				currentTexture[offs + x * 3 + 1] = floatToHalf(colorSamples[y * sampleWidth + x].y);
				currentTexture[offs + x * 3 + 2] = floatToHalf(colorSamples[y * sampleWidth + x].z);
			}
		}
#else
		// store results to lightmap texture
		for (int i = 0; i < sampleHeight; i++)
		{
			for (int j = 0; j < sampleWidth; j++)
			{
				// get texture offset
				int offs = (((textureWidth * (i + surface->lightmapOffs[1])) + surface->lightmapOffs[0]) * 3);

				// convert RGB to bytes
				currentTexture[offs + j * 3 + 0] = floatToHalf(colorSamples[i * sampleWidth + j].x);
				currentTexture[offs + j * 3 + 1] = floatToHalf(colorSamples[i * sampleWidth + j].y);
				currentTexture[offs + j * 3 + 2] = floatToHalf(colorSamples[i * sampleWidth + j].z);
			}
		}
#endif
	}
}

uint16_t* LevelMesh::AllocTextureRoom(Surface* surface, int* x, int* y)
{
	int width = surface->lightmapDims[0];
	int height = surface->lightmapDims[1];
	int numTextures = textures.size();

	int k;
	for (k = 0; k < numTextures; ++k)
	{
		if (textures[k]->MakeRoomForBlock(width, height, x, y))
		{
			break;
		}
	}

	if (k == numTextures)
	{
		textures.push_back(std::make_unique<LightmapTexture>(textureWidth, textureHeight));
		if (!textures[k]->MakeRoomForBlock(width, height, x, y))
		{
			throw std::runtime_error("Lightmap allocation failed");
		}
	}

	surface->lightmapNum = k;
	return textures[surface->lightmapNum]->Pixels();
}

void LevelMesh::CreateLightProbes(FLevel& map)
{
	float minX = std::floor(map.MinX / 65536.0f);
	float minY = std::floor(map.MinY / 65536.0f);
	float maxX = std::floor(map.MaxX / 65536.0f) + 1.0f;
	float maxY = std::floor(map.MaxY / 65536.0f) + 1.0f;

	float halfGridSize = GridSize * 0.5f;
	float doubleGridSize = GridSize * 2.0f;

	for (float y = minY; y < maxY; y += GridSize)
	{
		for (float x = minX; x < maxX; x += GridSize)
		{
			MapSubsectorEx* ssec = map.PointInSubSector((int)x, (int)y);
			IntSector* sec = ssec ? map.GetSectorFromSubSector(ssec) : nullptr;
			if (sec)
			{
				float z0 = sec->floorplane.zAt(x, y);
				float z1 = sec->ceilingplane.zAt(x, y);
				float delta = z1 - z0;
				if (delta > doubleGridSize)
				{
					LightProbeSample p[3];
					p[0].Position = Vec3(x, y, z0 + halfGridSize);
					p[1].Position = Vec3(x, y, z0 + (z1 - z0) * 0.5f);
					p[2].Position = Vec3(x, y, z1 - halfGridSize);

					for (int i = 0; i < 3; i++)
					{
						lightProbes.push_back(p[i]);
					}
				}
				else if (delta > 0.0f)
				{
					LightProbeSample probe;
					probe.Position.x = x;
					probe.Position.y = y;
					probe.Position.z = z0 + (z1 - z0) * 0.5f;
					lightProbes.push_back(probe);
				}
			}
		}
	}

	for (unsigned int i = 0; i < map.ThingLightProbes.Size(); i++)
	{
		LightProbeSample probe;
		probe.Position = map.GetLightProbePosition(i);
		lightProbes.push_back(probe);
	}
}

void LevelMesh::CreateSideSurfaces(FLevel &doomMap, IntSideDef *side)
{
	IntSector *front;
	IntSector *back;

	front = doomMap.GetFrontSector(side);
	back = doomMap.GetBackSector(side);

	if (front->controlsector)
		return;

	FloatVertex v1 = doomMap.GetSegVertex(side->line->v1);
	FloatVertex v2 = doomMap.GetSegVertex(side->line->v2);

	if (side->line->sidenum[0] != (ptrdiff_t)(side - &doomMap.Sides[0]))
	{
		std::swap(v1, v2);
	}

	float v1Top = front->ceilingplane.zAt(v1.x, v1.y);
	float v1Bottom = front->floorplane.zAt(v1.x, v1.y);
	float v2Top = front->ceilingplane.zAt(v2.x, v2.y);
	float v2Bottom = front->floorplane.zAt(v2.x, v2.y);

	int typeIndex = side - &doomMap.Sides[0];

	Vec2 dx(v2.x - v1.x, v2.y - v1.y);
	float distance = std::sqrt(dx.Dot(dx));

	if (back)
	{
		for (unsigned int j = 0; j < front->x3dfloors.Size(); j++)
		{
			IntSector *xfloor = front->x3dfloors[j];

			// Don't create a line when both sectors have the same 3d floor
			bool bothSides = false;
			for (unsigned int k = 0; k < back->x3dfloors.Size(); k++)
			{
				if (back->x3dfloors[k] == xfloor)
				{
					bothSides = true;
					break;
				}
			}
			if (bothSides)
				continue;

			float texWidth = 128.0f;
			float texHeight = 128.0f;

			auto surf = std::make_unique<Surface>();
			surf->material = "texture";
			surf->type = ST_MIDDLESIDE;
			surf->typeIndex = typeIndex;
			surf->controlSector = xfloor;
			surf->numVerts = 4;
			surf->verts.resize(4);
			surf->verts[0].x = surf->verts[2].x = v2.x;
			surf->verts[0].y = surf->verts[2].y = v2.y;
			surf->verts[1].x = surf->verts[3].x = v1.x;
			surf->verts[1].y = surf->verts[3].y = v1.y;
			surf->verts[0].z = xfloor->floorplane.zAt(v2.x, v2.y);
			surf->verts[1].z = xfloor->floorplane.zAt(v1.x, v1.y);
			surf->verts[2].z = xfloor->ceilingplane.zAt(v2.x, v2.y);
			surf->verts[3].z = xfloor->ceilingplane.zAt(v1.x, v1.y);
			surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
			surf->plane.SetDistance(surf->verts[0]);

			float texZ = surf->verts[0].z;

			surf->uvs.resize(4);
			surf->uvs[0].x = 0.0f;
			surf->uvs[1].x = distance / texWidth;
			surf->uvs[2].x = 0.0f;
			surf->uvs[3].x = distance / texWidth;
			surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
			surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
			surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
			surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

			surfaces.push_back(std::move(surf));
		}

		float v1TopBack = back->ceilingplane.zAt(v1.x, v1.y);
		float v1BottomBack = back->floorplane.zAt(v1.x, v1.y);
		float v2TopBack = back->ceilingplane.zAt(v2.x, v2.y);
		float v2BottomBack = back->floorplane.zAt(v2.x, v2.y);

		if (v1Top == v1TopBack && v1Bottom == v1BottomBack && v2Top == v2TopBack && v2Bottom == v2BottomBack)
		{
			return;
		}

		// bottom seg
		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			if (side->bottomtexture[0] != '-')
			{
				float texWidth = 128.0f;
				float texHeight = 128.0f;

				auto surf = std::make_unique<Surface>();
				surf->material = side->bottomtexture;
				surf->numVerts = 4;
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1Bottom;
				surf->verts[1].z = v2Bottom;
				surf->verts[2].z = v1BottomBack;
				surf->verts[3].z = v2BottomBack;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_LOWERSIDE;
				surf->typeIndex = typeIndex;
				surf->controlSector = nullptr;

				float texZ = surf->verts[0].z;

				surf->uvs.resize(4);
				surf->uvs[0].x = 0.0f;
				surf->uvs[1].x = distance / texWidth;
				surf->uvs[2].x = 0.0f;
				surf->uvs[3].x = distance / texWidth;
				surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

				surfaces.push_back(std::move(surf));
			}

			v1Bottom = v1BottomBack;
			v2Bottom = v2BottomBack;
		}

		// top seg
		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			bool bSky = false;

			if (front->skySector && back->skySector)
			{
				if (front->data.ceilingheight != back->data.ceilingheight && side->toptexture[0] == '-')
				{
					bSky = true;
				}
			}

			if (side->toptexture[0] != '-' || bSky)
			{
				float texWidth = 128.0f;
				float texHeight = 128.0f;

				auto surf = std::make_unique<Surface>();
				surf->material = side->toptexture;
				surf->numVerts = 4;
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1TopBack;
				surf->verts[1].z = v2TopBack;
				surf->verts[2].z = v1Top;
				surf->verts[3].z = v2Top;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_UPPERSIDE;
				surf->typeIndex = typeIndex;
				surf->bSky = bSky;
				surf->controlSector = nullptr;

				float texZ = surf->verts[0].z;

				surf->uvs.resize(4);
				surf->uvs[0].x = 0.0f;
				surf->uvs[1].x = distance / texWidth;
				surf->uvs[2].x = 0.0f;
				surf->uvs[3].x = distance / texWidth;
				surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

				surfaces.push_back(std::move(surf));
			}

			v1Top = v1TopBack;
			v2Top = v2TopBack;
		}
	}

	// middle seg
	if (back == nullptr)
	{
		float texWidth = 128.0f;
		float texHeight = 128.0f;

		auto surf = std::make_unique<Surface>();
		surf->material = side->midtexture;
		surf->numVerts = 4;
		surf->verts.resize(4);

		surf->verts[0].x = surf->verts[2].x = v1.x;
		surf->verts[0].y = surf->verts[2].y = v1.y;
		surf->verts[1].x = surf->verts[3].x = v2.x;
		surf->verts[1].y = surf->verts[3].y = v2.y;
		surf->verts[0].z = v1Bottom;
		surf->verts[1].z = v2Bottom;
		surf->verts[2].z = v1Top;
		surf->verts[3].z = v2Top;

		surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
		surf->plane.SetDistance(surf->verts[0]);
		surf->type = ST_MIDDLESIDE;
		surf->typeIndex = typeIndex;
		surf->controlSector = nullptr;

		float texZ = surf->verts[0].z;

		surf->uvs.resize(4);
		surf->uvs[0].x = 0.0f;
		surf->uvs[1].x = distance / texWidth;
		surf->uvs[2].x = 0.0f;
		surf->uvs[3].x = distance / texWidth;
		surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
		surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
		surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
		surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

		surfaces.push_back(std::move(surf));
	}
}

void LevelMesh::CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor)
{
	auto surf = std::make_unique<Surface>();
	surf->material = sector->data.floorpic;
	surf->numVerts = sub->numlines;
	surf->verts.resize(surf->numVerts);
	surf->uvs.resize(surf->numVerts);

	if (!is3DFloor)
	{
		surf->plane = sector->floorplane;
	}
	else
	{
		surf->plane = Plane::Inverse(sector->ceilingplane);
	}

	for (int j = 0; j < surf->numVerts; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + (surf->numVerts - 1) - j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->uvs[j].x = v1.x / 64.0f;
		surf->uvs[j].y = v1.y / 64.0f;
	}

	surf->type = ST_FLOOR;
	surf->typeIndex = typeIndex;
	surf->controlSector = is3DFloor ? sector : nullptr;

	surfaces.push_back(std::move(surf));
}

void LevelMesh::CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor)
{
	auto surf = std::make_unique<Surface>();
	surf->material = sector->data.ceilingpic;
	surf->numVerts = sub->numlines;
	surf->verts.resize(surf->numVerts);
	surf->uvs.resize(surf->numVerts);
	surf->bSky = sector->skySector;

	if (!is3DFloor)
	{
		surf->plane = sector->ceilingplane;
	}
	else
	{
		surf->plane = Plane::Inverse(sector->floorplane);
	}

	for (int j = 0; j < surf->numVerts; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->uvs[j].x = v1.x / 64.0f;
		surf->uvs[j].y = v1.y / 64.0f;
	}

	surf->type = ST_CEILING;
	surf->typeIndex = typeIndex;
	surf->controlSector = is3DFloor ? sector : nullptr;

	surfaces.push_back(std::move(surf));
}

void LevelMesh::CreateSubsectorSurfaces(FLevel &doomMap)
{
	printf("------------- Building subsector surfaces -------------\n");

	for (int i = 0; i < doomMap.NumGLSubsectors; i++)
	{
		printf("subsectors: %i / %i\r", i + 1, doomMap.NumGLSubsectors);

		MapSubsectorEx *sub = &doomMap.GLSubsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		IntSector *sector = doomMap.GetSectorFromSubSector(sub);
		if (!sector || sector->controlsector)
			continue;

		CreateFloorSurface(doomMap, sub, sector, i, false);
		CreateCeilingSurface(doomMap, sub, sector, i, false);

		for (unsigned int j = 0; j < sector->x3dfloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector->x3dfloors[j], i, true);
			CreateCeilingSurface(doomMap, sub, sector->x3dfloors[j], i, true);
		}
	}

	printf("\nLeaf surfaces: %i\n", (int)surfaces.size() - doomMap.NumGLSubsectors);
}

bool LevelMesh::IsDegenerate(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = v1.x - v0.x;
	float ay = v1.y - v0.y;
	float az = v1.z - v0.z;
	float bx = v2.x - v0.x;
	float by = v2.y - v0.y;
	float bz = v2.z - v0.z;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

void LevelMesh::AddLightmapLump(FWadWriter& wadFile)
{
	// Calculate size of lump
	int numTexCoords = 0;
	int numSurfaces = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum != -1)
		{
			numTexCoords += surfaces[i]->numVerts;
			numSurfaces++;
		}
	}

	int version = 0;
	int headerSize = 5 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
	int surfacesSize = surfaces.size() * 5 * sizeof(uint32_t);
	int texCoordsSize = numTexCoords * 2 * sizeof(float);
	int texDataSize = textures.size() * textureWidth * textureHeight * 3 * 2;
	int lightProbesSize = lightProbes.size() * 6 * sizeof(float);
	int lumpSize = headerSize + lightProbesSize + surfacesSize + texCoordsSize + texDataSize;

	// Setup buffer
	std::vector<uint8_t> buffer(lumpSize);
	BinFile lumpFile;
	lumpFile.SetBuffer(buffer.data());

	// Write header
	lumpFile.Write32(version);
	lumpFile.Write16(textureWidth);
	lumpFile.Write16(textures.size());
	lumpFile.Write32(numSurfaces);
	lumpFile.Write32(numTexCoords);
	lumpFile.Write32(lightProbes.size());
	lumpFile.Write32(map->NumGLSubsectors);

	// Write light probes
	for (const LightProbeSample& probe : lightProbes)
	{
		lumpFile.WriteFloat(probe.Position.x);
		lumpFile.WriteFloat(probe.Position.y);
		lumpFile.WriteFloat(probe.Position.z);
		lumpFile.WriteFloat(probe.Color.x);
		lumpFile.WriteFloat(probe.Color.y);
		lumpFile.WriteFloat(probe.Color.z);
	}

	// Write surfaces
	int coordOffsets = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum == -1)
			continue;

		lumpFile.Write32(surfaces[i]->type);
		lumpFile.Write32(surfaces[i]->typeIndex);
		lumpFile.Write32(surfaces[i]->controlSector ? (uint32_t)(surfaces[i]->controlSector - &map->Sectors[0]) : 0xffffffff);
		lumpFile.Write32(surfaces[i]->lightmapNum);
		lumpFile.Write32(coordOffsets);
		coordOffsets += surfaces[i]->numVerts;
	}

	// Write texture coordinates
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum == -1)
			continue;

		int count = surfaces[i]->numVerts;
		if (surfaces[i]->type == ST_FLOOR)
		{
			for (int j = count - 1; j >= 0; j--)
			{
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2]);
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2 + 1]);
			}
		}
		else if (surfaces[i]->type == ST_CEILING)
		{
			for (int j = 0; j < count; j++)
			{
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2]);
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2 + 1]);
			}
		}
		else
		{
			// zdray uses triangle strip internally, lump/gzd uses triangle fan

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[0]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[1]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[4]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[5]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[6]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[7]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[2]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[3]);
		}
	}

	// Write lightmap textures
	for (size_t i = 0; i < textures.size(); i++)
	{
		unsigned int count = (textureWidth * textureHeight) * 3;
		uint16_t* pixels = textures[i]->Pixels();
		for (unsigned int j = 0; j < count; j++)
		{
			lumpFile.Write16(pixels[j]);
		}
	}

	// Compress and store in lump
	ZLibOut zout(wadFile);
	wadFile.StartWritingLump("LIGHTMAP");
	zout.Write(buffer.data(), lumpFile.BufferAt() - lumpFile.Buffer());
}

void LevelMesh::Export(std::string filename)
{
	// This is so ugly! I had nothing to do with it! ;)
	std::string mtlfilename = filename;
	for (int i = 0; i < 3; i++) mtlfilename.pop_back();
	mtlfilename += "mtl";

	TArray<Vec3> outvertices;
	TArray<Vec2> outuv;
	TArray<Vec3> outnormal;
	TArray<int> outface;

	outvertices.Resize(MeshVertices.Size());
	outuv.Resize(MeshVertices.Size());
	outnormal.Resize(MeshVertices.Size());

	for (unsigned int surfidx = 0; surfidx < MeshElements.Size() / 3; surfidx++)
	{
		Surface* surface = surfaces[MeshSurfaces[surfidx]].get();
		for (int i = 0; i < 3; i++)
		{
			int elementidx = surfidx * 3 + i;
			int vertexidx = MeshElements[elementidx];
			int uvindex = MeshUVIndex[vertexidx];

			outvertices[vertexidx] = MeshVertices[vertexidx];
			outuv[vertexidx] = Vec2(surface->lightmapCoords[uvindex * 2], surface->lightmapCoords[uvindex * 2 + 1]);
			outnormal[vertexidx] = surface->plane.Normal();
			outface.Push(vertexidx);

			//surface->lightmapNum;
		}
	}

	std::string buffer;
	buffer.reserve(16 * 1024 * 1024);

	buffer += "# zdray exported mesh\r\n";

	buffer += "mtllib ";
	buffer += mtlfilename;
	buffer += "\r\n";

	buffer += "usemtl Textured\r\n";

	float scale = 0.01f;

	for (unsigned int i = 0; i < outvertices.Size(); i++)
	{
		buffer += "v ";
		buffer += std::to_string(-outvertices[i].x * scale);
		buffer += " ";
		buffer += std::to_string(outvertices[i].z * scale);
		buffer += " ";
		buffer += std::to_string(outvertices[i].y * scale);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outnormal.Size(); i++)
	{
		buffer += "vn ";
		buffer += std::to_string(-outnormal[i].x);
		buffer += " ";
		buffer += std::to_string(outnormal[i].z);
		buffer += " ";
		buffer += std::to_string(outnormal[i].y);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outuv.Size(); i++)
	{
		buffer += "vt ";
		buffer += std::to_string(outuv[i].x);
		buffer += " ";
		buffer += std::to_string(1.0f - outuv[i].y);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outface.Size(); i += 3)
	{
		std::string e0 = std::to_string(outface[i] + 1);
		std::string e1 = std::to_string(outface[i + 1] + 1);
		std::string e2 = std::to_string(outface[i + 2] + 1);
		buffer += "f ";
		buffer += e0;
		buffer += "/";
		buffer += e0;
		buffer += "/";
		buffer += e0;
		buffer += " ";
		buffer += e1;
		buffer += "/";
		buffer += e1;
		buffer += "/";
		buffer += e1;
		buffer += " ";
		buffer += e2;
		buffer += "/";
		buffer += e2;
		buffer += "/";
		buffer += e2;
		buffer += "\r\n";
	}

	FILE* file = fopen(filename.c_str(), "wb");
	if (file)
	{
		fwrite(buffer.data(), buffer.size(), 1, file);
		fclose(file);
	}

	std::string mtl = R"(newmtl Textured
   Ka 1.000 1.000 1.000
   Kd 1.000 1.000 1.000
   Ks 0.000 0.000 0.000
   map_Ka lightmap0.png
   map_Kd lightmap0.png
)";

	file = fopen(mtlfilename.c_str(), "wb");
	if (file)
	{
		fwrite(mtl.data(), mtl.size(), 1, file);
		fclose(file);
	}

	int index = 0;
	for (const auto& texture : textures)
	{
		int w = texture->Width();
		int h = texture->Height();
		uint16_t* p = texture->Pixels();
#if 1
		std::vector<uint8_t> buf(w * h * 4);
		uint8_t* buffer = buf.data();
		for (int i = 0; i < w * h; i++)
		{
			buffer[i * 4] = (uint8_t)(int)clamp(halfToFloat(p[i * 3]) * 255.0f, 0.0f, 255.0f);
			buffer[i * 4 + 1] = (uint8_t)(int)clamp(halfToFloat(p[i * 3 + 1]) * 255.0f, 0.0f, 255.0f);
			buffer[i * 4 + 2] = (uint8_t)(int)clamp(halfToFloat(p[i * 3 + 2]) * 255.0f, 0.0f, 255.0f);
			buffer[i * 4 + 3] = 0xff;
		}
		PNGWriter::save("lightmap" + std::to_string(index++) + ".png", w, h, 4, buffer);
#else
		std::vector<uint16_t> buf(w * h * 4);
		uint16_t* buffer = buf.data();
		for (int i = 0; i < w * h; i++)
		{
			buffer[i * 4] = (uint16_t)(int)clamp(halfToFloat(p[i * 3]) * 65535.0f, 0.0f, 65535.0f);
			buffer[i * 4 + 1] = (uint16_t)(int)clamp(halfToFloat(p[i * 3 + 1]) * 65535.0f, 0.0f, 65535.0f);
			buffer[i * 4 + 2] = (uint16_t)(int)clamp(halfToFloat(p[i * 3 + 2]) * 65535.0f, 0.0f, 65535.0f);
			buffer[i * 4 + 3] = 0xffff;
		}
		PNGWriter::save("lightmap" + std::to_string(index++) + ".png", w, h, 8, buffer);
#endif
	}
}
