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
#include "surfaces.h"
#include "level/level.h"
#include "lightmap.h"
#include "surfacelight.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

extern int Multisample;
extern int LightBounce;

LightmapBuilder::LightmapBuilder()
{
}

LightmapBuilder::~LightmapBuilder()
{
}

void LightmapBuilder::CreateLightmaps(FLevel &doomMap, int sampleDistance, int textureSize)
{
	map = &doomMap;
	samples = sampleDistance;
	textureWidth = textureSize;
	textureHeight = textureSize;

	mesh = std::make_unique<LevelMesh>(doomMap);

	CreateSurfaceLights();
	CreateTraceTasks();

	SetupLightCellGrid();

	SetupTaskProcessed("Tracing cells", grid.blocks.size());
	Worker::RunJob(grid.blocks.size(), [=](int id) {
		LightBlock(id);
		PrintTaskProcessed();
	});
	printf("Cells traced: %i \n\n", tracedTexels);

	SetupTaskProcessed("Tracing surfaces", traceTasks.size());
	Worker::RunJob(traceTasks.size(), [=](int id) {
		LightSurface(id);
		PrintTaskProcessed();
	});
	printf("Texels traced: %i \n\n", tracedTexels);

	if (LightBounce > 0)
	{
		SetupTaskProcessed("Tracing indirect", traceTasks.size());
		Worker::RunJob(traceTasks.size(), [=](int id) {
			LightIndirect(id);
			PrintTaskProcessed();
		});
		printf("Texels traced: %i \n\n", tracedTexels);
	}

	for (auto &surf : mesh->surfaces)
	{
		FinishSurface(surf.get());
	}
}

BBox LightmapBuilder::GetBoundsFromSurface(const Surface *surface)
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

// Traces to the ceiling surface. Will emit light if the surface that was traced is a sky
bool LightmapBuilder::EmitFromCeiling(const Surface *surface, const Vec3 &origin, const Vec3 &normal, Vec3 &color)
{
	float attenuation = surface ? normal.Dot(map->GetSunDirection()) : 1.0f;

	if (attenuation <= 0)
	{
		// plane is not even facing the sunlight
		return false;
	}

	LevelTraceHit trace = mesh->Trace(origin, origin + (map->GetSunDirection() * 32768.0f));

	if (trace.fraction == 1.0f)
	{
		// nothing was hit
		//color.x += 1.0f;
		return false;
	}

	if (trace.hitSurface->bSky == false)
	{
		if (trace.hitSurface->type == ST_CEILING)
			return false;

		// not a ceiling/sky surface
		return false;
	}

	color += map->GetSunColor() * attenuation;

	return true;
}

template<class T>
T smoothstep(const T edge0, const T edge1, const T x)
{
	auto t = clamp<T>((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

static float radians(float degrees)
{
	return degrees * 3.14159265359f / 180.0f;
}

// Traces a line from the texel's origin to the sunlight direction and against all nearby thing lights
Vec3 LightmapBuilder::LightTexelSample(const Vec3 &origin, Surface *surface)
{
	Plane plane;
	if (surface)
		plane = surface->plane;

	Vec3 color(0.0f, 0.0f, 0.0f);

	// check all thing lights
	for (unsigned int i = 0; i < map->ThingLights.Size(); i++)
	{
		ThingLight *tl = &map->ThingLights[i];

		float originZ;
		if (!tl->bCeiling)
			originZ = tl->sector->floorplane.zAt(tl->origin.x, tl->origin.y) + tl->height;
		else
			originZ = tl->sector->ceilingplane.zAt(tl->origin.x, tl->origin.y) - tl->height;

		Vec3 lightOrigin(tl->origin.x, tl->origin.y, originZ);

		if (surface && plane.Distance(lightOrigin) - plane.d < 0)
		{
			// completely behind the plane
			continue;
		}

		float radius = tl->radius * 2.0f; // 2.0 because gzdoom's dynlights do this and we want them to match
		float intensity = tl->intensity;

		if (origin.DistanceSq(lightOrigin) > (radius*radius))
		{
			// not within range
			continue;
		}

		Vec3 dir = (lightOrigin - origin);
		float dist = dir.Unit();
		dir.Normalize();

		float spotAttenuation = 1.0f;
		if (tl->outerAngleCos > -1.0f)
		{
			float negPitch = -radians(tl->mapThing->pitch);
			float xyLen = std::cos(negPitch);
			Vec3 spotDir;
			spotDir.x = std::sin(radians(tl->mapThing->angle)) * xyLen;
			spotDir.y = std::cos(radians(tl->mapThing->angle)) * xyLen;
			spotDir.z = -std::sin(negPitch);
			float cosDir = Vec3::Dot(dir, spotDir);
			spotAttenuation = smoothstep(tl->outerAngleCos, tl->innerAngleCos, cosDir);
			if (spotAttenuation <= 0.0f)
			{
				// outside spot light
				continue;
			}
		}

		if (mesh->TraceAnyHit(lightOrigin, origin))
		{
			// this light is occluded by something
			continue;
		}

		float attenuation = 1.0f - (dist / radius);
		attenuation *= spotAttenuation;
		if (surface)
			attenuation *= plane.Normal().Dot(dir);
		attenuation *= intensity;

		// accumulate results
		color += tl->rgb * attenuation;

		tracedTexels++;
	}

	if (!surface || surface->type != ST_CEILING)
	{
		// see if it's exposed to sunlight
		if (EmitFromCeiling(surface, origin, surface ? plane.Normal() : Vec3::vecUp, color))
			tracedTexels++;
	}

	// trace against surface lights
	for (size_t i = 0; i < surfaceLights.size(); ++i)
	{
		SurfaceLight *surfaceLight = surfaceLights[i].get();

		float attenuation = surfaceLight->TraceSurface(mesh.get(), surface, origin);
		if (attenuation > 0.0f)
		{
			color += surfaceLight->GetRGB() * surfaceLight->Intensity() * attenuation;
			tracedTexels++;
		}
	}

	return color;
}

// Determines a lightmap block in which to map to the lightmap texture.
// Width and height of the block is calcuated and steps are computed to determine where each texel will be positioned on the surface
void LightmapBuilder::BuildSurfaceParams(Surface *surface)
{
	Plane *plane;
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
}

// Steps through each texel and traces a line to the world.
void LightmapBuilder::TraceSurface(Surface *surface, int offset)
{
	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];
	Vec3 normal = surface->plane.Normal();
	int multisampleCount = Multisample;
	Vec3 *colorSamples = surface->samples.data();

	int offsetend = std::min(offset + TraceTask::tasksize, sampleWidth * sampleHeight);
	for (int pos = offset; pos < offsetend; pos++)
	{
		int i = pos / sampleWidth;
		int j = pos % sampleWidth;

		Vec3 c(0.0f, 0.0f, 0.0f);

		int totalsamples = (multisampleCount * 2 + 1);
		float scale = 0.5f / totalsamples;
		for (int yy = -multisampleCount; yy <= multisampleCount; yy++)
		{
			for (int xx = -multisampleCount; xx <= multisampleCount; xx++)
			{
				Vec2 multisamplePos((float)j + xx * scale, (float)i + yy * scale);

				// convert the texel into world-space coordinates.
				// this will be the origin in which a line will be traced from
				Vec3 pos = surface->lightmapOrigin + normal + (surface->lightmapSteps[0] * multisamplePos.x) + (surface->lightmapSteps[1] * multisamplePos.y);

				c += LightTexelSample(pos, surface);
			}
		}

		c /= totalsamples * totalsamples;

		colorSamples[i * sampleWidth + j] = c;
	}
}

void LightmapBuilder::FinishSurface(Surface *surface)
{
	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];
	Vec3 *colorSamples = surface->samples.data();

	if (!surface->indirect.empty())
	{
		Vec3 *indirect = surface->indirect.data();
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
			const auto &c = colorSamples[i * sampleWidth + j];
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
		uint16_t *currentTexture = AllocTextureRoom(surface, &x, &y);

		// calculate texture coordinates
		for (int i = 0; i < surface->numVerts; i++)
		{
			Vec3 tDelta = surface->verts[i] - surface->bounds.min;
			surface->lightmapCoords[i * 2 + 0] = (tDelta.Dot(surface->textureCoords[0]) + x + 0.5f) / (float)textureWidth;
			surface->lightmapCoords[i * 2 + 1] = (tDelta.Dot(surface->textureCoords[1]) + y + 0.5f) / (float)textureHeight;
		}

		surface->lightmapOffs[0] = x;
		surface->lightmapOffs[1] = y;

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
	}
}

uint16_t *LightmapBuilder::AllocTextureRoom(Surface *surface, int *x, int *y)
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

static float RadicalInverse_VdC(uint32_t bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

static Vec2 Hammersley(uint32_t i, uint32_t N)
{
	return Vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

static Vec3 ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * M_PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a*a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	Vec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

	// from tangent-space vector to world-space sample vector
	Vec3 up = std::abs(N.z) < 0.999f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
	Vec3 tangent = Vec3::Normalize(Vec3::Cross(up, N));
	Vec3 bitangent = Vec3::Cross(N, tangent);

	Vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return Vec3::Normalize(sampleVec);
}

void LightmapBuilder::TraceIndirectLight(Surface *surface, int offset)
{
	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];
	Vec3 normal = surface->plane.Normal();
	Vec3 *indirect = surface->indirect.data();

	int offsetend = std::min(offset + TraceTask::tasksize, sampleWidth * sampleHeight);
	for (int offpos = offset; offpos < offsetend; offpos++)
	{
		int i = offpos / sampleWidth;
		int j = offpos % sampleWidth;

		Vec3 pos = surface->lightmapOrigin + normal +
			(surface->lightmapSteps[0] * (float)j) +
			(surface->lightmapSteps[1] * (float)i);

		const int SAMPLE_COUNT = 128;// 1024;

		float totalWeight = 0.0f;
		Vec3 c(0.0f, 0.0f, 0.0f);

		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			Vec2 Xi = Hammersley(i, SAMPLE_COUNT);
			Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
			Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);

			float NdotL = std::max(Vec3::Dot(normal, L), 0.0f);
			if (NdotL > 0.0f)
			{
				tracedTexels++;
				LevelTraceHit hit = mesh->Trace(pos, pos + L * 1000.0f);
				if (hit.fraction < 1.0f)
				{
					Vec3 surfaceLight;
					if (hit.hitSurface->bSky)
					{
						surfaceLight = { 0.5f, 0.5f, 0.5f };
					}
					else
					{
						float u =
							hit.hitSurface->lightmapCoords[hit.indices[0] * 2] * (1.0f - hit.b - hit.c) +
							hit.hitSurface->lightmapCoords[hit.indices[1] * 2] * hit.b +
							hit.hitSurface->lightmapCoords[hit.indices[2] * 2] * hit.c;

						float v =
							hit.hitSurface->lightmapCoords[hit.indices[0] * 2 + 1] * (1.0f - hit.b - hit.c) +
							hit.hitSurface->lightmapCoords[hit.indices[1] * 2 + 1] * hit.b +
							hit.hitSurface->lightmapCoords[hit.indices[2] * 2 + 1] * hit.c;

						int hitTexelX = clamp((int)(u + 0.5f), 0, hit.hitSurface->lightmapDims[0] - 1);
						int hitTexelY = clamp((int)(v + 0.5f), 0, hit.hitSurface->lightmapDims[1] - 1);

						Vec3 *hitTexture = hit.hitSurface->samples.data();
						const Vec3 &hitPixel = hitTexture[hitTexelX + hitTexelY * hit.hitSurface->lightmapDims[0]];

						float attenuation = (1.0f - hit.fraction);
						surfaceLight = hitPixel * attenuation;
					}
					c += surfaceLight * NdotL;
				}
				totalWeight += NdotL;
			}
		}

		c = c / totalWeight;

		indirect[i * sampleWidth + j] = c;
	}
}

void LightmapBuilder::CreateTraceTasks()
{
	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface *surface = mesh->surfaces[i].get();

		BuildSurfaceParams(surface);

		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		surface->samples.resize(sampleWidth * sampleHeight);

		if (LightBounce > 0)
			surface->indirect.resize(sampleWidth * sampleHeight);

		int total = sampleWidth * sampleHeight;
		int count = (total + TraceTask::tasksize - 1) / TraceTask::tasksize;
		for (int j = 0; j < count; j++)
			traceTasks.push_back(TraceTask(i, j * TraceTask::tasksize));
	}
}

void LightmapBuilder::LightSurface(const int taskid)
{
	const TraceTask &task = traceTasks[taskid];
	TraceSurface(mesh->surfaces[task.surface].get(), task.offset);
}

void LightmapBuilder::LightIndirect(const int taskid)
{
	const TraceTask &task = traceTasks[taskid];
	TraceIndirectLight(mesh->surfaces[task.surface].get(), task.offset);
}

void LightmapBuilder::CreateSurfaceLights()
{
	for (size_t j = 0; j < mesh->surfaces.size(); ++j)
	{
		Surface *surface = mesh->surfaces[j].get();

		if (surface->type >= ST_MIDDLESIDE && surface->type <= ST_LOWERSIDE)
		{
			int lightdefidx = map->Sides[surface->typeIndex].lightdef;
			if (lightdefidx != -1)
			{
				auto surfaceLight = std::make_unique<SurfaceLight>(map->SurfaceLights[lightdefidx], surface);
				surfaceLight->Subdivide(16);
				surfaceLights.push_back(std::move(surfaceLight));
			}
		}
		else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
		{
			MapSubsectorEx *sub = &map->GLSubsectors[surface->typeIndex];
			IntSector *sector = map->GetSectorFromSubSector(sub);

			if (sector && surface->numVerts > 0)
			{
				if (sector->floorlightdef != -1 && surface->type == ST_FLOOR)
				{
					auto surfaceLight = std::make_unique<SurfaceLight>(map->SurfaceLights[sector->floorlightdef], surface);
					surfaceLight->Subdivide(16);
					surfaceLights.push_back(std::move(surfaceLight));
				}
				else if (sector->ceilinglightdef != -1 && surface->type == ST_CEILING)
				{
					auto surfaceLight = std::make_unique<SurfaceLight>(map->SurfaceLights[sector->ceilinglightdef], surface);
					surfaceLight->Subdivide(16);
					surfaceLights.push_back(std::move(surfaceLight));
				}
			}
		}
	}
}

void LightmapBuilder::SetupLightCellGrid()
{
	BBox worldBBox = mesh->CollisionMesh->get_bbox();
	float blockWorldSize = LIGHTCELL_BLOCK_SIZE * LIGHTCELL_SIZE;
	grid.x = static_cast<int>(std::floor(worldBBox.min.x / blockWorldSize));
	grid.y = static_cast<int>(std::floor(worldBBox.min.y / blockWorldSize));
	grid.width = static_cast<int>(std::ceil(worldBBox.max.x / blockWorldSize)) - grid.x;
	grid.height = static_cast<int>(std::ceil(worldBBox.max.y / blockWorldSize)) - grid.y;
	grid.blocks.resize(grid.width * grid.height);
}

void LightmapBuilder::LightBlock(int id)
{
	float blockWorldSize = LIGHTCELL_BLOCK_SIZE * LIGHTCELL_SIZE;

	// Locate block in world
	LightCellBlock &block = grid.blocks[id];
	int x = grid.x + id % grid.width;
	int y = grid.y + id / grid.height;
	float worldX = blockWorldSize * x + 0.5f * LIGHTCELL_SIZE;
	float worldY = blockWorldSize * y + 0.5f * LIGHTCELL_SIZE;

	// Analyze for cells
	IntSector *sectors[LIGHTCELL_BLOCK_SIZE * LIGHTCELL_BLOCK_SIZE];
	float ceilings[LIGHTCELL_BLOCK_SIZE * LIGHTCELL_BLOCK_SIZE];
	float floors[LIGHTCELL_BLOCK_SIZE * LIGHTCELL_BLOCK_SIZE];
	float maxCeiling = -M_INFINITY;
	float minFloor = M_INFINITY;
	for (int yy = 0; yy < LIGHTCELL_BLOCK_SIZE; yy++)
	{
		for (int xx = 0; xx < LIGHTCELL_BLOCK_SIZE; xx++)
		{
			int idx = xx + yy * LIGHTCELL_BLOCK_SIZE;
			float cellWorldX = worldX + xx * LIGHTCELL_SIZE;
			float cellWorldY = worldY + yy * LIGHTCELL_SIZE;
			MapSubsectorEx *subsector = map->PointInSubSector(cellWorldX, cellWorldY);
			if (subsector)
			{
				IntSector *sector = map->GetSectorFromSubSector(subsector);

				float ceiling = sector->ceilingplane.zAt(cellWorldX, cellWorldY);
				float floor = sector->floorplane.zAt(cellWorldX, cellWorldY);

				sectors[idx] = sector;
				ceilings[idx] = ceiling;
				floors[idx] = floor;
				maxCeiling = std::max(maxCeiling, ceiling);
				minFloor = std::min(minFloor, floor);
			}
			else
			{
				sectors[idx] = nullptr;
				ceilings[idx] = -M_INFINITY;
				floors[idx] = M_INFINITY;
			}
		}
	}

	if (minFloor != M_INFINITY)
	{
		// Allocate space for the cells
		block.z = static_cast<int>(std::floor(minFloor / LIGHTCELL_SIZE));
		block.layers = static_cast<int>(std::ceil(maxCeiling / LIGHTCELL_SIZE)) - block.z;
		block.cells.Resize(LIGHTCELL_BLOCK_SIZE * LIGHTCELL_BLOCK_SIZE * block.layers);

		// Ray trace the cells
		for (int yy = 0; yy < LIGHTCELL_BLOCK_SIZE; yy++)
		{
			for (int xx = 0; xx < LIGHTCELL_BLOCK_SIZE; xx++)
			{
				int idx = xx + yy * LIGHTCELL_BLOCK_SIZE;
				float cellWorldX = worldX + xx * LIGHTCELL_SIZE;
				float cellWorldY = worldY + yy * LIGHTCELL_SIZE;

				for (int zz = 0; zz < block.layers; zz++)
				{
					float cellWorldZ = (block.z + zz + 0.5f) * LIGHTCELL_SIZE;

					Vec3 color;
					if (cellWorldZ > floors[idx] && cellWorldZ < ceilings[idx])
					{
						color = LightTexelSample({ cellWorldX, cellWorldY, cellWorldZ }, nullptr);
					}
					else
					{
						color = { 0.0f, 0.0f, 0.0f };
					}

					block.cells[idx + zz * LIGHTCELL_BLOCK_SIZE * LIGHTCELL_BLOCK_SIZE] = color;
				}
			}
		}
	}
	else
	{
		// Entire block is outside the map
		block.z = 0;
		block.layers = 0;
	}
}

void LightmapBuilder::AddLightmapLump(FWadWriter &wadFile)
{
	const auto &surfaces = mesh->surfaces;

	// Calculate size of lump
	int numTexCoords = 0;
	int numSurfaces = 0;
	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum != -1)
		{
			numTexCoords += surfaces[i]->numVerts;
			numSurfaces++;
		}
	}
	int numCells = 0;
	for (size_t i = 0; i < grid.blocks.size(); i++)
		numCells += grid.blocks[i].cells.Size();
	int version = 0;
	int headerSize = 4 * sizeof(uint32_t) + 6 * sizeof(uint16_t);
	int cellBlocksSize = grid.blocks.size() * 2 * sizeof(uint16_t);
	int cellsSize = numCells * sizeof(float) * 3;
	int surfacesSize = surfaces.size() * 5 * sizeof(uint32_t);
	int texCoordsSize = numTexCoords * 2 * sizeof(float);
	int texDataSize = textures.size() * textureWidth * textureHeight * 3 * 2;
	int lumpSize = headerSize + cellBlocksSize + cellsSize + surfacesSize + texCoordsSize + texDataSize;

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
	lumpFile.Write16(grid.x);
	lumpFile.Write16(grid.y);
	lumpFile.Write16(grid.width);
	lumpFile.Write16(grid.height);
	lumpFile.Write32(numCells);

	// Write cell blocks
	for (size_t i = 0; i < grid.blocks.size(); i++)
	{
		lumpFile.Write16(grid.blocks[i].z);
		lumpFile.Write16(grid.blocks[i].layers);
	}

	// Write cells
	for (size_t i = 0; i < grid.blocks.size(); i++)
	{
		const auto &cells = grid.blocks[i].cells;
		for (unsigned int j = 0; j < cells.Size(); j++)
		{
			lumpFile.WriteFloat(cells[j].x);
			lumpFile.WriteFloat(cells[j].y);
			lumpFile.WriteFloat(cells[j].z);
		}
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
		uint16_t *pixels = textures[i]->Pixels();
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

void LightmapBuilder::SetupTaskProcessed(const char *name, int total)
{
	printf("-------------- %s ---------------\n", name);

	processed = 0;
	progresstotal = total;
}

void LightmapBuilder::PrintTaskProcessed()
{
	std::unique_lock<std::mutex> lock(mutex);

	int lastproc = processed * 100 / progresstotal;
	processed++;
	int curproc = processed * 100 / progresstotal;
	if (lastproc != curproc || processed == 1)
	{
		float remaining = (float)processed / (float)progresstotal;
		printf("%i%c done\r", (int)(remaining * 100.0f), '%');
	}
}

/////////////////////////////////////////////////////////////////////////////

LightmapTexture::LightmapTexture(int width, int height) : textureWidth(width), textureHeight(height)
{
	mPixels.resize(width * height * 3);
	allocBlocks.resize(width);
}

bool LightmapTexture::MakeRoomForBlock(const int width, const int height, int *x, int *y)
{
	int bestRow1 = textureHeight;

	for (int i = 0; i <= textureWidth - width; i++)
	{
		int bestRow2 = 0;

		int j;
		for (j = 0; j < width; j++)
		{
			if (allocBlocks[i + j] >= bestRow1)
			{
				break;
			}

			if (allocBlocks[i + j] > bestRow2)
			{
				bestRow2 = allocBlocks[i + j];
			}
		}

		// found a free block
		if (j == width)
		{
			*x = i;
			*y = bestRow1 = bestRow2;
		}
	}

	if (bestRow1 + height > textureHeight)
	{
		// no room
		return false;
	}

	// store row offset
	for (int i = 0; i < width; i++)
	{
		allocBlocks[*x + i] = bestRow1 + height;
	}

	return true;
}
