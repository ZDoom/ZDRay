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
#include "levelmesh.h"
#include "pngwriter.h"
#include <map>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

LevelMesh::LevelMesh(FLevel &doomMap, int sampleDistance, int textureSize)
{
	map = &doomMap;
	defaultSamples = sampleDistance;
	textureWidth = textureSize;
	textureHeight = textureSize;

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
		printf("   Sides: %i / %i\r", i + 1, doomMap.Sides.Size());
	}

	printf("\n   Side surfaces: %i\n", (int)surfaces.size());

	CreateSubsectorSurfaces(doomMap);

	printf("   Surfaces total: %i\n", (int)surfaces.size());

	// Update sector group of the surfacesaa
	for (auto& surface : surfaces)
	{
		surface->sectorGroup = surface->type == ST_CEILING || surface->type == ST_FLOOR ?
			doomMap.GetSectorFromSubSector(&doomMap.GLSubsectors[surface->typeIndex])->group : (doomMap.Sides[surface->typeIndex].GetSectorGroup());
	}

	printf("   Building level mesh...\n");

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		const auto &s = surfaces[i];
		int numVerts = s->verts.size();
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

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		BuildSurfaceParams(surfaces[i].get());
	}

	printf("   Finding smoothing groups...\n");
	BuildSmoothingGroups(doomMap);
	printf("   Building collision data...\n");

	Collision = std::make_unique<TriangleMeshShape>(MeshVertices.Data(), MeshVertices.Size(), MeshElements.Data(), MeshElements.Size());

	BuildLightLists(doomMap);
	/*
	std::map<int, int> lightStats;
	for (auto& surface : surfaces)
		lightStats[surface->LightList.size()]++;
	for (auto& it : lightStats)
		printf("   %d lights: %d surfaces\n", it.first, it.second);
	printf("\n");
	*/
}

void LevelMesh::BuildSmoothingGroups(FLevel& doomMap)
{
	size_t lastPercentage = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		size_t percentage = i * 100 / surfaces.size();
		if (lastPercentage != percentage)
			printf("   Surfaces: %i%%\r", (int)percentage);

		// Is this surface in the same plane as an existing smoothing group?
		int smoothingGroupIndex = -1;

		auto surface = surfaces[i].get();

		for (size_t j = 0; j < smoothingGroups.size(); j++)
		{
			if (surface->sectorGroup == smoothingGroups[j].sectorGroup)
			{
				float direction = std::abs(dot(smoothingGroups[j].plane.Normal(), surface->plane.Normal()));
				if (direction >= 0.9999f && direction <= 1.001f)
				{
					float dist = std::abs(smoothingGroups[j].plane.Distance(surface->plane.Normal() * surface->plane.d));
					if (dist <= 0.01f)
					{
						smoothingGroupIndex = (int)j;
						break;
					}
				}
			}
		}

		// Surface is in a new plane. Create a smoothing group for it
		if (smoothingGroupIndex == -1)
		{
			smoothingGroupIndex = smoothingGroups.size();

			SmoothingGroup group;
			group.plane = surface->plane;
			group.sectorGroup = surface->sectorGroup;
			smoothingGroups.push_back(group);
		}

		smoothingGroups[smoothingGroupIndex].surfaces.push_back(surface);
		surface->smoothingGroupIndex = smoothingGroupIndex;
	}

	printf("   Created %d smoothing groups for %d surfaces\n", (int)smoothingGroups.size(), (int)surfaces.size());
}

void LevelMesh::PropagateLight(FLevel& doomMap, ThingLight *light)
{
	if (lightPropagationRecursiveDepth > 32)
	{
		return;
	}

	SphereShape sphere;
	sphere.center = light->LightRelativeOrigin();
	sphere.radius = light->LightRadius();
	lightPropagationRecursiveDepth++;
	std::set<Portal, RecursivePortalComparator> portalsToErase;
	for (int triangleIndex : TriangleMeshShape::find_all_hits(Collision.get(), &sphere))
	{
		Surface* surface = surfaces[MeshSurfaces[triangleIndex]].get();

		// skip any surface which isn't physically connected to the sector group in which the light resides
		if (light->sectorGroup == surface->sectorGroup)
		{
			if (surface->portalIndex >= 0)
			{			
				auto portal = portals[surface->portalIndex].get();

				if (touchedPortals.insert(*portal).second)
				{
					auto fakeLight = std::make_unique<ThingLight>(*light);

					fakeLight->relativePosition.emplace(portal->TransformPosition(light->LightRelativeOrigin()));
					fakeLight->sectorGroup = portal->targetSectorGroup;

					PropagateLight(doomMap, fakeLight.get());
					portalsToErase.insert(*portal);
					portalLights.push_back(std::move(fakeLight));
				}
			}

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
	
	for (auto& portal : portalsToErase)
	{
		touchedPortals.erase(portal);
	}

	lightPropagationRecursiveDepth--;
}

void LevelMesh::BuildLightLists(FLevel& doomMap)
{
	for (unsigned i = 0; i < map->ThingLights.Size(); ++i)
	{
		printf("   Building light lists: %u / %u\r", i, map->ThingLights.Size());
		PropagateLight(doomMap, &map->ThingLights[i]);
	}

	printf("   Building light lists: %u / %u\n", map->ThingLights.Size(), map->ThingLights.Size());
}

// Determines a lightmap block in which to map to the lightmap texture.
// Width and height of the block is calcuated and steps are computed to determine where each texel will be positioned on the surface
void LevelMesh::BuildSurfaceParams(Surface* surface)
{
	Plane* plane;
	BBox bounds;
	vec3 roundedSize;
	int i;
	Plane::PlaneAxis axis;
	vec3 tCoords[2];
	vec3 tOrigin;
	int width;
	int height;
	float d;

	plane = &surface->plane;
	bounds = GetBoundsFromSurface(surface);
	surface->bounds = bounds;

	if (surface->sampleDimension < 0) surface->sampleDimension = 1;
	surface->sampleDimension = Math::RoundPowerOfTwo(surface->sampleDimension);

	// round off dimensions
	for (i = 0; i < 3; i++)
	{
		bounds.min[i] = surface->sampleDimension * (Math::Floor(bounds.min[i] / surface->sampleDimension) - 1);
		bounds.max[i] = surface->sampleDimension * (Math::Ceil(bounds.max[i] / surface->sampleDimension) + 1);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / surface->sampleDimension;
	}

	tCoords[0] = vec3(0.0f);
	tCoords[1] = vec3(0.0f);

	axis = plane->BestAxis();

	switch (axis)
	{
	case Plane::AXIS_YZ:
		width = (int)roundedSize.y;
		height = (int)roundedSize.z;
		tCoords[0].y = 1.0f / surface->sampleDimension;
		tCoords[1].z = 1.0f / surface->sampleDimension;
		break;

	case Plane::AXIS_XZ:
		width = (int)roundedSize.x;
		height = (int)roundedSize.z;
		tCoords[0].x = 1.0f / surface->sampleDimension;
		tCoords[1].z = 1.0f / surface->sampleDimension;
		break;

	case Plane::AXIS_XY:
		width = (int)roundedSize.x;
		height = (int)roundedSize.y;
		tCoords[0].x = 1.0f / surface->sampleDimension;
		tCoords[1].y = 1.0f / surface->sampleDimension;
		break;
	}

	// clamp width
	if (width > textureWidth - 2)
	{
		tCoords[0] *= ((float)(textureWidth - 2) / (float)width);
		width = (textureWidth - 2);
	}

	// clamp height
	if (height > textureHeight - 2)
	{
		tCoords[1] *= ((float)(textureHeight - 2) / (float)height);
		height = (textureHeight - 2);
	}

	surface->translateWorldToLocal = bounds.min;
	surface->projLocalToU = tCoords[0];
	surface->projLocalToV = tCoords[1];

	surface->lightUV.resize(surface->verts.size());
	for (i = 0; i < (int)surface->verts.size(); i++)
	{
		vec3 tDelta = surface->verts[i] - surface->translateWorldToLocal;
		surface->lightUV[i].x = dot(tDelta, surface->projLocalToU);
		surface->lightUV[i].y = dot(tDelta, surface->projLocalToV);
	}

	tOrigin = bounds.min;

	// project tOrigin and tCoords so they lie on the plane
	d = (plane->Distance(bounds.min)) / plane->Normal()[axis];
	tOrigin[axis] -= d;

	for (i = 0; i < 2; i++)
	{
		tCoords[i] = normalize(tCoords[i]);
		d = dot(tCoords[i], plane->Normal()) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface->texWidth = width;
	surface->texHeight = height;
	surface->texPixels.resize(width * height);
	surface->worldOrigin = tOrigin;
	surface->worldStepX = tCoords[0] * (float)surface->sampleDimension;
	surface->worldStepY = tCoords[1] * (float)surface->sampleDimension;
}

BBox LevelMesh::GetBoundsFromSurface(const Surface* surface)
{
	vec3 low(M_INFINITY, M_INFINITY, M_INFINITY);
	vec3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

	BBox bounds;
	bounds.Clear();

	for (int i = 0; i < (int)surface->verts.size(); i++)
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
	BlurSurfaces();

	std::vector<Surface*> sortedSurfaces;
	sortedSurfaces.reserve(surfaces.size());

	for (auto& surface : surfaces)
	{
		int sampleWidth = surface->texWidth;
		int sampleHeight = surface->texHeight;
		vec3* colorSamples = surface->texPixels.data();

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

		if (bShouldLookupTexture)
		{
			sortedSurfaces.push_back(surface.get());
		}
		else
		{
			surface->atlasPageIndex = -1;
		}
	}

	std::sort(sortedSurfaces.begin(), sortedSurfaces.end(), [](Surface* a, Surface* b) { return a->texHeight != b->texHeight ? a->texHeight > b->texHeight : a->texWidth > b->texWidth; });

	RectPacker packer(textureWidth, textureHeight, RectPacker::Spacing(0));

	for (Surface* surf : sortedSurfaces)
	{
		FinishSurface(packer, surf);
	}
}

void LevelMesh::BlurSurfaces()
{
	static const float weights[9] = { 0.125f, 0.25f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.25f, 0.125f };
	std::vector<vec3> tempBuffer;

	for (auto& surface : surfaces)
	{
		int texWidth = surface->texWidth;
		int texHeight = surface->texHeight;
		vec3* texPixels = surface->texPixels.data();

		tempBuffer.resize(std::max(tempBuffer.size(), (size_t)texWidth * texHeight));
		vec3* tempPixels = tempBuffer.data();

		// gaussian blur with a 3x3 kernel
		for (int y = 0; y < texHeight; y++)
		{
			vec3* src = &texPixels[y * texWidth];
			vec3* dst = &tempPixels[y * texWidth];
			for (int x = 0; x < texWidth; x++)
			{
				vec3 color = { 0.0f };
				for (int yy = -1; yy <= 1; yy++)
				{
					int yyy = clamp(y + yy, 0, texHeight - 1) - y;
					for (int xx = -1; xx <= 1; xx++)
					{
						int xxx = clamp(x + xx, 0, texWidth - 1);
						color += src[yyy * texWidth + xxx] * weights[4 + xx + yy * 3];
					}
				}
				dst[x] = color * 0.5f;
			}
		}

		memcpy(texPixels, tempPixels, texWidth * texHeight * sizeof(vec3));
	}
}

void LevelMesh::FinishSurface(RectPacker& packer, Surface* surface)
{
	int sampleWidth = surface->texWidth;
	int sampleHeight = surface->texHeight;
	vec3* colorSamples = surface->texPixels.data();

	auto result = packer.insert(sampleWidth, sampleHeight);
	int x = result.pos.x, y = result.pos.y;
	surface->atlasPageIndex = result.pageIndex;

	while (result.pageIndex >= textures.size())
	{
		textures.push_back(std::make_unique<LightmapTexture>(textureWidth, textureHeight));
	}

	uint16_t* currentTexture = textures[surface->atlasPageIndex]->Pixels();

	// calculate final texture coordinates
	for (int i = 0; i < (int)surface->verts.size(); i++)
	{
		auto& u = surface->lightUV[i].x;
		auto& v = surface->lightUV[i].y;
		u = (u + x) / (float)textureWidth;
		v = (v + y) / (float)textureHeight;
	}

	surface->atlasX = x;
	surface->atlasY = y;

	// store results to lightmap texture
	for (int i = 0; i < sampleHeight; i++)
	{
		for (int j = 0; j < sampleWidth; j++)
		{
			// get texture offset
			int offs = ((textureWidth * (i + surface->atlasY)) + surface->atlasX) * 3;

			// convert RGB to bytes
			currentTexture[offs + j * 3 + 0] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].x, 0.0f, 65000.0f));
			currentTexture[offs + j * 3 + 1] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].y, 0.0f, 65000.0f));
			currentTexture[offs + j * 3 + 2] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].z, 0.0f, 65000.0f));
		}
	}
}

int LevelMesh::CreateLinePortal(FLevel& doomMap, const IntLineDef& srcLine, const IntLineDef& dstLine)
{
	auto portal = std::make_unique<Portal>();

	// Calculate portal transformation
	{
		FloatVertex srcV1 = doomMap.GetSegVertex(srcLine.v1);
		FloatVertex srcV2 = doomMap.GetSegVertex(srcLine.v2);
		FloatVertex dstV1 = doomMap.GetSegVertex(dstLine.v1);
		FloatVertex dstV2 = doomMap.GetSegVertex(dstLine.v2);

		int alignment = srcLine.args[3];

		double srcAZ = 0;
		double srcBZ = 0;
		double dstAZ = 0;
		double dstBZ = 0;

		const auto* srcFront = srcLine.frontsector;
		const auto* dstFront = dstLine.frontsector;

		if (alignment == 1) // floor
		{
			srcAZ = srcFront->floorplane.zAt(srcV1.x, srcV1.y);
			srcBZ = srcFront->floorplane.zAt(srcV2.x, srcV2.y);
			dstAZ = dstFront->floorplane.zAt(dstV1.x, dstV1.y);
			dstBZ = dstFront->floorplane.zAt(dstV2.x, dstV2.y);
		}
		else if (alignment == 2) // ceiling
		{
			srcAZ = srcFront->ceilingplane.zAt(srcV1.x, srcV1.y);
			srcBZ = srcFront->ceilingplane.zAt(srcV2.x, srcV2.y);
			dstAZ = dstFront->ceilingplane.zAt(dstV1.x, dstV1.y);
			dstBZ = dstFront->ceilingplane.zAt(dstV2.x, dstV2.y);
		}

		const vec3 vecSrcA = vec3(vec2(srcV1.x, srcV1.y), srcAZ);
		const vec3 vecSrcB = vec3(vec2(srcV2.x, srcV2.y), srcAZ);
		const vec3 vecDstA = vec3(vec2(dstV1.x, dstV1.y), dstAZ);
		const vec3 vecDstB = vec3(vec2(dstV2.x, dstV2.y), dstBZ);

		// Translation
		vec3 originSrc = (vecSrcB + vecSrcA) * 0.5f;
		vec3 originDst = (vecDstB + vecDstA) * 0.5f;

		vec3 translation = originDst - originSrc;

		// Rotation
		// TODO :(

		// printf("   Portal translation: %.3f %.3f %.3f\n", translation.x, translation.y, translation.z);

		portal->transformation = mat4::translate(translation);
		portal->sourceSectorGroup = srcLine.GetSectorGroup();
		portal->targetSectorGroup = dstLine.GetSectorGroup();
	}

	// Deduplicate portals
	auto it = portalCache.find(*portal);

	if (it == portalCache.end())
	{
		int id = int(portals.size());
		portalCache.emplace(*portal, id);
		portals.push_back(std::move(portal));
		return id;
	}
	return it->second;
}

int LevelMesh::CheckAndMakePortal(FLevel& doomMap, MapSubsectorEx* sub, IntSector* sector, int typeIndex, int plane)
{
	for (const auto line : sector->portals)
	{
		if (line->special == Sector_SetPortal && line->args[0] && line->args[2] == plane && !line->args[3] && sector->HasTag(line->args[0]))
		{
			const IntLineDef* dstLine = nullptr;

			// Find the other portal line
			for (const auto &targetLine : doomMap.Lines)
			{
				if (targetLine.special == Sector_SetPortal && targetLine.args[2] == plane && targetLine.args[3] && line->args[0] == targetLine.args[0])
				{
					dstLine = &targetLine;
					break;
				}
			}

			if (dstLine)
			{
				 return CreatePlanePortal(doomMap, *line, *dstLine);
			}
		}
	}

	return -1;
}

int LevelMesh::CreatePlanePortal(FLevel& doomMap, const IntLineDef& srcLine, const IntLineDef& dstLine)
{
	auto portal = std::make_unique<Portal>();

	// Calculate portal transformation
	{
		FloatVertex srcV1 = doomMap.GetSegVertex(srcLine.v1);
		FloatVertex srcV2 = doomMap.GetSegVertex(srcLine.v2);
		FloatVertex dstV1 = doomMap.GetSegVertex(dstLine.v1);
		FloatVertex dstV2 = doomMap.GetSegVertex(dstLine.v2);

		int alignment = srcLine.args[2];

		double srcAZ = 0;
		double srcBZ = 0;
		double dstAZ = 0;
		double dstBZ = 0;

		const auto* srcFront = srcLine.frontsector;
		const auto* dstFront = dstLine.frontsector;

		if (alignment == 0) // floor
		{
			srcAZ = srcFront->floorplane.zAt(srcV1.x, srcV1.y);
			srcBZ = srcFront->floorplane.zAt(srcV2.x, srcV2.y);
			dstAZ = dstFront->ceilingplane.zAt(dstV1.x, dstV1.y);
			dstBZ = dstFront->ceilingplane.zAt(dstV2.x, dstV2.y);
		}
		else // ceiling
		{
			srcAZ = srcFront->ceilingplane.zAt(srcV1.x, srcV1.y);
			srcBZ = srcFront->ceilingplane.zAt(srcV2.x, srcV2.y);
			dstAZ = dstFront->floorplane.zAt(dstV1.x, dstV1.y);
			dstBZ = dstFront->floorplane.zAt(dstV2.x, dstV2.y);
		}

		const vec3 vecSrcA = vec3(vec2(srcV1.x, srcV1.y), srcAZ);
		const vec3 vecSrcB = vec3(vec2(srcV2.x, srcV2.y), srcBZ);
		const vec3 vecDstA = vec3(vec2(dstV1.x, dstV1.y), dstAZ);
		const vec3 vecDstB = vec3(vec2(dstV2.x, dstV2.y), dstBZ);

		// Translation
		vec3 originSrc = (vecSrcB + vecSrcA) * 0.5f;
		vec3 originDst = (vecDstB + vecDstA) * 0.5f;

		vec3 translation = originDst - originSrc;

		// printf("   Portal translation: %.3f %.3f %.3f\n", translation.x, translation.y, translation.z);

		portal->transformation = mat4::translate(translation);
		portal->sourceSectorGroup = srcLine.GetSectorGroup();
		portal->targetSectorGroup = dstLine.GetSectorGroup();
	}

	// Deduplicate portals
	auto it = portalCache.find(*portal);

	if (it == portalCache.end())
	{
		int id = int(portals.size());
		portalCache.emplace(*portal, id);
		portals.push_back(std::move(portal));
		return id;
	}
	return it->second;
}

int LevelMesh::GetSampleDistance(const IntSideDef& sidedef, WallPart part) const
{
	auto sampleDistance = sidedef.GetSampleDistance(part);
	return sampleDistance ? sampleDistance : defaultSamples;
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

	vec2 dx(v2.x - v1.x, v2.y - v1.y);
	float distance = length(dx);

	// line portal
	if (side->line->special == Line_SetPortal && side->line->frontsector == front)
	{
		const unsigned destLineIndex = doomMap.FindFirstLineId(side->line->args[0]);

		if (destLineIndex < doomMap.Lines.Size())
		{
			float texWidth = 128.0f;
			float texHeight = 128.0f;

			auto surf = std::make_unique<Surface>();
			surf->material = side->midtexture;
			surf->verts.resize(4);
			surf->bSky = false;

			surf->verts[0].x = surf->verts[2].x = v1.x;
			surf->verts[0].y = surf->verts[2].y = v1.y;
			surf->verts[1].x = surf->verts[3].x = v2.x;
			surf->verts[1].y = surf->verts[3].y = v2.y;
			surf->verts[0].z = v1Bottom;
			surf->verts[1].z = v2Bottom;
			surf->verts[2].z = v1Top;
			surf->verts[3].z = v2Top;

			surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
			surf->plane.SetDistance(surf->verts[0]);
			surf->type = ST_MIDDLESIDE;
			surf->typeIndex = typeIndex;
			surf->controlSector = nullptr;
			surf->sampleDimension = GetSampleDistance(*side, WallPart::MIDDLE);

			float texZ = surf->verts[0].z;

			surf->texUV.resize(4);
			surf->texUV[0].x = 0.0f;
			surf->texUV[1].x = distance / texWidth;
			surf->texUV[2].x = 0.0f;
			surf->texUV[3].x = distance / texWidth;
			surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
			surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
			surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
			surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

			surf->portalDestinationIndex = destLineIndex;

			surf->portalIndex = CreateLinePortal(doomMap, *side->line, doomMap.Lines[destLineIndex]);

			surfaces.push_back(std::move(surf));
			return;
		}
		else
		{
			// Warn about broken portal?
		}
	}

	// line_horizont consumes everything
	if (side->line->special == Line_Horizon && front != back)
	{
		float texWidth = 128.0f;
		float texHeight = 128.0f;

		auto surf = std::make_unique<Surface>();
		surf->material = side->midtexture;
		surf->verts.resize(4);
		surf->bSky = front->skyFloor || front->skyCeiling;

		surf->verts[0].x = surf->verts[2].x = v1.x;
		surf->verts[0].y = surf->verts[2].y = v1.y;
		surf->verts[1].x = surf->verts[3].x = v2.x;
		surf->verts[1].y = surf->verts[3].y = v2.y;
		surf->verts[0].z = v1Bottom;
		surf->verts[1].z = v2Bottom;
		surf->verts[2].z = v1Top;
		surf->verts[3].z = v2Top;

		surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
		surf->plane.SetDistance(surf->verts[0]);
		surf->type = ST_MIDDLESIDE;
		surf->typeIndex = typeIndex;
		surf->controlSector = nullptr;
		surf->sampleDimension = GetSampleDistance(*side, WallPart::MIDDLE);

		float texZ = surf->verts[0].z;

		surf->texUV.resize(4);
		surf->texUV[0].x = 0.0f;
		surf->texUV[1].x = distance / texWidth;
		surf->texUV[2].x = 0.0f;
		surf->texUV[3].x = distance / texWidth;
		surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
		surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
		surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
		surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

		surfaces.push_back(std::move(surf));
		return;
	}

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

			IntSideDef* otherSide = &doomMap.Sides[side->line->sidenum[0]] == side ? &doomMap.Sides[side->line->sidenum[1]] : &doomMap.Sides[side->line->sidenum[0]];

			auto surf = std::make_unique<Surface>();
			surf->material = "texture";
			surf->type = ST_MIDDLESIDE;
			surf->typeIndex = typeIndex;
			surf->controlSector = xfloor;
			surf->sampleDimension = GetSampleDistance(*side, WallPart::MIDDLE);
			surf->verts.resize(4);
			surf->verts[0].x = surf->verts[2].x = v2.x;
			surf->verts[0].y = surf->verts[2].y = v2.y;
			surf->verts[1].x = surf->verts[3].x = v1.x;
			surf->verts[1].y = surf->verts[3].y = v1.y;
			surf->verts[0].z = xfloor->floorplane.zAt(v2.x, v2.y);
			surf->verts[1].z = xfloor->floorplane.zAt(v1.x, v1.y);
			surf->verts[2].z = xfloor->ceilingplane.zAt(v2.x, v2.y);
			surf->verts[3].z = xfloor->ceilingplane.zAt(v1.x, v1.y);
			surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
			surf->plane.SetDistance(surf->verts[0]);

			float texZ = surf->verts[0].z;

			surf->texUV.resize(4);
			surf->texUV[0].x = 0.0f;
			surf->texUV[1].x = distance / texWidth;
			surf->texUV[2].x = 0.0f;
			surf->texUV[3].x = distance / texWidth;
			surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
			surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
			surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
			surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

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
			bool bSky = false;

			if (front->skyFloor && back->skyFloor)
			{
				if (front->data.floorheight != back->data.floorheight && side->bottomtexture[0] == '-')
				{
					bSky = true;
				}
			}

			if (side->bottomtexture[0] != '-' || bSky)
			{
				float texWidth = 128.0f;
				float texHeight = 128.0f;

				auto surf = std::make_unique<Surface>();
				surf->material = side->bottomtexture;
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1Bottom;
				surf->verts[1].z = v2Bottom;
				surf->verts[2].z = v1BottomBack;
				surf->verts[3].z = v2BottomBack;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_LOWERSIDE;
				surf->typeIndex = typeIndex;
				surf->bSky = bSky;
				surf->controlSector = nullptr;
				surf->sampleDimension = GetSampleDistance(*side, WallPart::BOTTOM);

				float texZ = surf->verts[0].z;

				surf->texUV.resize(4);
				surf->texUV[0].x = 0.0f;
				surf->texUV[1].x = distance / texWidth;
				surf->texUV[2].x = 0.0f;
				surf->texUV[3].x = distance / texWidth;
				surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

				surfaces.push_back(std::move(surf));
			}

			v1Bottom = v1BottomBack;
			v2Bottom = v2BottomBack;
		}

		// top seg
		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			bool bSky = false;

			if (front->skyCeiling && back->skyCeiling)
			{
				if (front->data.ceilingheight != back->data.ceilingheight)
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
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1TopBack;
				surf->verts[1].z = v2TopBack;
				surf->verts[2].z = v1Top;
				surf->verts[3].z = v2Top;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_UPPERSIDE;
				surf->typeIndex = typeIndex;
				surf->bSky = bSky;
				surf->controlSector = nullptr;
				surf->sampleDimension = GetSampleDistance(*side, WallPart::TOP);

				float texZ = surf->verts[0].z;

				surf->texUV.resize(4);
				surf->texUV[0].x = 0.0f;
				surf->texUV[1].x = distance / texWidth;
				surf->texUV[2].x = 0.0f;
				surf->texUV[3].x = distance / texWidth;
				surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

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
		surf->verts.resize(4);

		surf->verts[0].x = surf->verts[2].x = v1.x;
		surf->verts[0].y = surf->verts[2].y = v1.y;
		surf->verts[1].x = surf->verts[3].x = v2.x;
		surf->verts[1].y = surf->verts[3].y = v2.y;
		surf->verts[0].z = v1Bottom;
		surf->verts[1].z = v2Bottom;
		surf->verts[2].z = v1Top;
		surf->verts[3].z = v2Top;

		surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2], surf->verts[3]);
		surf->plane.SetDistance(surf->verts[0]);
		surf->type = ST_MIDDLESIDE;
		surf->typeIndex = typeIndex;
		surf->controlSector = nullptr;
		surf->sampleDimension = GetSampleDistance(*side, WallPart::MIDDLE);

		float texZ = surf->verts[0].z;

		surf->texUV.resize(4);
		surf->texUV[0].x = 0.0f;
		surf->texUV[1].x = distance / texWidth;
		surf->texUV[2].x = 0.0f;
		surf->texUV[3].x = distance / texWidth;
		surf->texUV[0].y = (surf->verts[0].z - texZ) / texHeight;
		surf->texUV[1].y = (surf->verts[1].z - texZ) / texHeight;
		surf->texUV[2].y = (surf->verts[2].z - texZ) / texHeight;
		surf->texUV[3].y = (surf->verts[3].z - texZ) / texHeight;

		surfaces.push_back(std::move(surf));
	}
}

void LevelMesh::CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor)
{
	auto surf = std::make_unique<Surface>();
	surf->sampleDimension = sector->sampleDistanceFloor ? sector->sampleDistanceFloor : defaultSamples;
	surf->material = sector->data.floorpic;
	surf->verts.resize(sub->numlines);
	surf->texUV.resize(sub->numlines);
	surf->bSky = sector->skyFloor;

	if (!is3DFloor)
	{
		surf->plane = sector->floorplane;
		surf->portalIndex = CheckAndMakePortal(doomMap, sub, sector, typeIndex, PLANE_FLOOR);
	}
	else
	{
		surf->plane = Plane::Inverse(sector->ceilingplane);
	}

	for (int j = 0; j < (int)sub->numlines; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + (sub->numlines - 1) - j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->texUV[j].x = v1.x / 64.0f;
		surf->texUV[j].y = v1.y / 64.0f;
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
	surf->sampleDimension = sector->sampleDistanceCeiling ? sector->sampleDistanceCeiling : defaultSamples;
	surf->verts.resize(sub->numlines);
	surf->texUV.resize(sub->numlines);
	surf->bSky = sector->skyCeiling;

	if (!is3DFloor)
	{
		surf->plane = sector->ceilingplane;
		surf->portalIndex = CheckAndMakePortal(doomMap, sub, sector, typeIndex, PLANE_CEILING);
	}
	else
	{
		surf->plane = Plane::Inverse(sector->floorplane);
	}

	for (int j = 0; j < (int)sub->numlines; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->texUV[j].x = v1.x / 64.0f;
		surf->texUV[j].y = v1.y / 64.0f;
	}

	surf->type = ST_CEILING;
	surf->typeIndex = typeIndex;
	surf->controlSector = is3DFloor ? sector : nullptr;

	surfaces.push_back(std::move(surf));
}

void LevelMesh::CreateSubsectorSurfaces(FLevel &doomMap)
{
	for (int i = 0; i < doomMap.NumGLSubsectors; i++)
	{
		printf("   Subsectors: %i / %i\r", i + 1, doomMap.NumGLSubsectors);

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

	printf("\n   Leaf surfaces: %i\n", (int)surfaces.size() - doomMap.NumGLSubsectors);
}

bool LevelMesh::IsDegenerate(const vec3 &v0, const vec3 &v1, const vec3 &v2)
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
		if (surfaces[i]->atlasPageIndex != -1)
		{
			numTexCoords += surfaces[i]->verts.size();
			numSurfaces++;
		}
	}

	int version = 1;
	int headerSize = 5 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
	int surfacesSize = surfaces.size() * 5 * sizeof(uint32_t);
	int texCoordsSize = numTexCoords * 2 * sizeof(float);
	int texDataSize = textures.size() * textureWidth * textureHeight * 3 * 2;
	int lumpSize = headerSize + surfacesSize + texCoordsSize + texDataSize;

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
	lumpFile.Write32(map->NumGLSubsectors);
	lumpFile.WriteFloat(map->GetSunDirection().x);
	lumpFile.WriteFloat(map->GetSunDirection().y);
	lumpFile.WriteFloat(map->GetSunDirection().z);
	lumpFile.WriteFloat(map->GetSunColor().r);
	lumpFile.WriteFloat(map->GetSunColor().g);
	lumpFile.WriteFloat(map->GetSunColor().b);

	// Write surfaces
	int coordOffsets = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->atlasPageIndex == -1)
			continue;

		lumpFile.Write32(surfaces[i]->type);
		lumpFile.Write32(surfaces[i]->typeIndex);
		lumpFile.Write32(surfaces[i]->controlSector ? (uint32_t)(surfaces[i]->controlSector - &map->Sectors[0]) : 0xffffffff);
		lumpFile.Write32(surfaces[i]->atlasPageIndex);
		lumpFile.Write32(coordOffsets);
		coordOffsets += surfaces[i]->verts.size();
	}

	// Write texture coordinates
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->atlasPageIndex == -1)
			continue;

		int count = surfaces[i]->verts.size();
		if (surfaces[i]->type == ST_FLOOR)
		{
			for (int j = count - 1; j >= 0; j--)
			{
				lumpFile.WriteFloat(surfaces[i]->lightUV[j].x);
				lumpFile.WriteFloat(surfaces[i]->lightUV[j].y);
			}
		}
		else if (surfaces[i]->type == ST_CEILING)
		{
			for (int j = 0; j < count; j++)
			{
				lumpFile.WriteFloat(surfaces[i]->lightUV[j].x);
				lumpFile.WriteFloat(surfaces[i]->lightUV[j].y);
			}
		}
		else
		{
			// zdray uses triangle strip internally, lump/gzd uses triangle fan

			lumpFile.WriteFloat(surfaces[i]->lightUV[0].x);
			lumpFile.WriteFloat(surfaces[i]->lightUV[0].y);

			lumpFile.WriteFloat(surfaces[i]->lightUV[2].x);
			lumpFile.WriteFloat(surfaces[i]->lightUV[2].y);

			lumpFile.WriteFloat(surfaces[i]->lightUV[3].x);
			lumpFile.WriteFloat(surfaces[i]->lightUV[3].y);

			lumpFile.WriteFloat(surfaces[i]->lightUV[1].x);
			lumpFile.WriteFloat(surfaces[i]->lightUV[1].y);
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
	printf("   Exporting mesh \"%s\"\n", filename.c_str());

	// This is so ugly! I had nothing to do with it! ;)
	std::string mtlfilename = filename;
	for (int i = 0; i < 3; i++) mtlfilename.pop_back();
	mtlfilename += "mtl";

	TArray<vec3> outvertices;
	TArray<vec2> outuv;
	TArray<vec3> outnormal;
	TArray<int> outface;
	TArray<int> outLightmapId;

	outvertices.Resize(MeshVertices.Size());
	outuv.Resize(MeshVertices.Size());
	outnormal.Resize(MeshVertices.Size());
	outLightmapId.Resize(MeshElements.Size() / 3);

	for (unsigned int surfidx = 0; surfidx < MeshElements.Size() / 3; surfidx++)
	{
		Surface* surface = surfaces[MeshSurfaces[surfidx]].get();

		outLightmapId[surfidx] = surface->atlasPageIndex;

		for (int i = 0; i < 3; i++)
		{
			int elementidx = surfidx * 3 + i;
			int vertexidx = MeshElements[elementidx];
			int uvindex = MeshUVIndex[vertexidx];

			outvertices[vertexidx] = MeshVertices[vertexidx];
			outuv[vertexidx] = surface->lightUV[uvindex];
			outnormal[vertexidx] = surface->plane.Normal();
			outface.Push(vertexidx);
		}
	}

	std::string buffer;
	buffer.reserve(16 * 1024 * 1024);

	buffer += "# zdray exported mesh\r\n";

	buffer += "mtllib ";
	buffer += mtlfilename;
	buffer += "\r\n";

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

	int prevLightmap = -1;

	for (unsigned int i = 0; i < outface.Size(); i += 3)
	{
		if (prevLightmap != outLightmapId[i/3])
		{
			prevLightmap = outLightmapId[i/3];

			if (prevLightmap < 0)
			{
				buffer += "usemtl lightmap_none\r\n";
			}
			else
			{
				buffer += "usemtl lightmap" + std::to_string(prevLightmap) + "\r\n";
			}
		}

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

	std::string mtl = "newtml lightmap_none\r\n"
		"Ka 0 0 0\r\n"
		"Kd 0 0 0\r\n"
		"ks 0 0 0\r\n"
		"map_Kd lightmap0.png\r\n";

	for (int i = 0; i < textures.size(); i++)
	{
		mtl += "\r\nnewmtl lightmap" + std::to_string(i) + "\r\n";
		mtl +=
			"Ka 1 1 1\r\n"
			"Kd 1 1 1\r\n"
			"Ks 0 0 0\r\n";
		mtl += "map_Ka lightmap" + std::to_string(i) + ".png\r\n";
		mtl += "map_Kd lightmap" + std::to_string(i) + ".png\r\n";
	}

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

	printf("   Export complete\n");
}
