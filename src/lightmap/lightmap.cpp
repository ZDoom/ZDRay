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
//-----------------------------------------------------------------------------
//
// DESCRIPTION: Lightmap and lightgrid building module
//
//-----------------------------------------------------------------------------

#include "math/mathlib.h"
#include "surfaces.h"
#include "level/level.h"
#include "lightmap.h"
#include "lightsurface.h"
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
extern thread_local kexVec3 *colorSamples;

kexLightmapBuilder::kexLightmapBuilder()
{
}

kexLightmapBuilder::~kexLightmapBuilder()
{
}

void kexLightmapBuilder::NewTexture()
{
	numTextures++;

	allocBlocks.push_back(std::vector<int>(textureWidth));
	textures.push_back(std::vector<uint16_t>(textureWidth * textureHeight * 3));
}

// Determines where to map a new block on to the lightmap texture
bool kexLightmapBuilder::MakeRoomForBlock(const int width, const int height, int *x, int *y, int *num)
{
	int i;
	int j;
	int k;
	int bestRow1;
	int bestRow2;

	*num = -1;

	if (allocBlocks.empty())
	{
		return false;
	}

	for (k = 0; k < numTextures; ++k)
	{
		bestRow1 = textureHeight;

		for (i = 0; i <= textureWidth - width; i++)
		{
			bestRow2 = 0;

			for (j = 0; j < width; j++)
			{
				if (allocBlocks[k][i + j] >= bestRow1)
				{
					break;
				}

				if (allocBlocks[k][i + j] > bestRow2)
				{
					bestRow2 = allocBlocks[k][i + j];
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
			continue;
		}

		for (i = 0; i < width; i++)
		{
			// store row offset
			allocBlocks[k][*x + i] = bestRow1 + height;
		}

		*num = k;
		return true;
	}

	return false;
}

kexBBox kexLightmapBuilder::GetBoundsFromSurface(const surface_t *surface)
{
	kexVec3 low(M_INFINITY, M_INFINITY, M_INFINITY);
	kexVec3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

	kexBBox bounds;
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
bool kexLightmapBuilder::EmitFromCeiling(const surface_t *surface, const kexVec3 &origin, const kexVec3 &normal, kexVec3 &color)
{
	float attenuation = surface ? normal.Dot(map->GetSunDirection()) : 1.0f;

	if (attenuation <= 0)
	{
		// plane is not even facing the sunlight
		return false;
	}

	LevelTraceHit trace = map->Trace(origin, origin + (map->GetSunDirection() * 32768.0f));

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
kexVec3 kexLightmapBuilder::LightTexelSample(const kexVec3 &origin, surface_t *surface)
{
	kexPlane plane;
	if (surface)
		plane = surface->plane;

	kexVec3 color(0.0f, 0.0f, 0.0f);

	// check all thing lights
	for (size_t i = 0; i < map->thingLights.size(); i++)
	{
		thingLight_t *tl = map->thingLights[i].get();

		float originZ;
		if (!tl->bCeiling)
			originZ = tl->sector->floorplane.zAt(tl->origin.x, tl->origin.y) + tl->height;
		else
			originZ = tl->sector->ceilingplane.zAt(tl->origin.x, tl->origin.y) - tl->height;

		kexVec3 lightOrigin(tl->origin.x, tl->origin.y, originZ);

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

		kexVec3 dir = (lightOrigin - origin);
		float dist = dir.Unit();
		dir.Normalize();

		float spotAttenuation = 1.0f;
		if (tl->outerAngleCos > -1.0f)
		{
			float negPitch = -radians(tl->mapThing->pitch);
			float xyLen = std::cos(negPitch);
			kexVec3 spotDir;
			spotDir.x = std::sin(radians(tl->mapThing->angle)) * xyLen;
			spotDir.y = std::cos(radians(tl->mapThing->angle)) * xyLen;
			spotDir.z = -std::sin(negPitch);
			float cosDir = kexVec3::Dot(dir, spotDir);
			spotAttenuation = smoothstep(tl->outerAngleCos, tl->innerAngleCos, cosDir);
			if (spotAttenuation <= 0.0f)
			{
				// outside spot light
				continue;
			}
		}

		LevelTraceHit trace = map->Trace(lightOrigin, origin);

		if (trace.fraction != 1)
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
		if (EmitFromCeiling(surface, origin, surface ? plane.Normal() : kexVec3::vecUp, color))
			tracedTexels++;
	}

	// trace against surface lights
	for (size_t i = 0; i < map->lightSurfaces.size(); ++i)
	{
		kexLightSurface *surfaceLight = map->lightSurfaces[i].get();

		float attenuation = surfaceLight->TraceSurface(map, surface, origin);
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
void kexLightmapBuilder::BuildSurfaceParams(surface_t *surface)
{
	kexPlane *plane;
	kexBBox bounds;
	kexVec3 roundedSize;
	int i;
	kexPlane::planeAxis_t axis;
	kexVec3 tCoords[2];
	kexVec3 tOrigin;
	int width;
	int height;
	float d;

	plane = &surface->plane;
	bounds = GetBoundsFromSurface(surface);

	// round off dimentions
	for (i = 0; i < 3; i++)
	{
		bounds.min[i] = samples * kexMath::Floor(bounds.min[i] / samples);
		bounds.max[i] = samples * kexMath::Ceil(bounds.max[i] / samples);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / samples + 1;
	}

	tCoords[0].Clear();
	tCoords[1].Clear();

	axis = plane->BestAxis();

	switch (axis)
	{
	case kexPlane::AXIS_YZ:
		width = (int)roundedSize.y;
		height = (int)roundedSize.z;
		tCoords[0].y = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case kexPlane::AXIS_XZ:
		width = (int)roundedSize.x;
		height = (int)roundedSize.z;
		tCoords[0].x = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case kexPlane::AXIS_XY:
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
// For each non-occluded trace, color is accumulated and saved off into the lightmap texture based on what block is mapped to
void kexLightmapBuilder::TraceSurface(surface_t *surface)
{
	int sampleWidth;
	int sampleHeight;
	kexVec3 normal;
	kexVec3 pos;
	kexVec3 tDelta;
	int i;
	int j;
	uint16_t *currentTexture;
	bool bShouldLookupTexture = false;

	sampleWidth = surface->lightmapDims[0];
	sampleHeight = surface->lightmapDims[1];

	normal = surface->plane.Normal();

	int multisampleCount = Multisample;

	// start walking through each texel
	for (i = 0; i < sampleHeight; i++)
	{
		for (j = 0; j < sampleWidth; j++)
		{
			kexVec3 c(0.0f, 0.0f, 0.0f);

			for (int k = 0; k < multisampleCount; k++)
			{
				kexVec2 multisamplePos((float)j, (float)i);
				if (k > 0)
				{
					multisamplePos.x += rand() / (float)RAND_MAX - 0.5f;
					multisamplePos.y += rand() / (float)RAND_MAX - 0.5f;
					multisamplePos.x = std::max(multisamplePos.x, 0.0f);
					multisamplePos.y = std::max(multisamplePos.y, 0.0f);
					multisamplePos.x = std::min(multisamplePos.x, (float)sampleWidth);
					multisamplePos.y = std::min(multisamplePos.y, (float)sampleHeight);
				}

				// convert the texel into world-space coordinates.
				// this will be the origin in which a line will be traced from
				pos = surface->lightmapOrigin + normal +
					(surface->lightmapSteps[0] * multisamplePos.x) +
					(surface->lightmapSteps[1] * multisamplePos.y);

				c += LightTexelSample(pos, surface);
			}

			c /= multisampleCount;

			// if nothing at all was traced and color is completely black
			// then this surface will not go through the extra rendering
			// step in rendering the lightmap
			if (c.x > 0.0f || c.y > 0.0f || c.z > 0.0f)
			{
				bShouldLookupTexture = true;
			}

			colorSamples[i * LIGHTMAP_MAX_SIZE + j] = c;
		}
	}

	// SVE redraws the scene for lightmaps, so for optimizations,
	// tell the engine to ignore this surface if completely black
	/*if (bShouldLookupTexture == false)
	{
		surface->lightmapNum = -1;
		return;
	}
	else*/
	{
		int x = 0, y = 0;
		int width = surface->lightmapDims[0];
		int height = surface->lightmapDims[1];

		std::unique_lock<std::mutex> lock(mutex);

		// now that we know the width and height of this block, see if we got
		// room for it in the light map texture. if not, then we must allocate
		// a new texture
		if (!MakeRoomForBlock(width, height, &x, &y, &surface->lightmapNum))
		{
			// allocate a new texture for this block
			NewTexture();

			if (!MakeRoomForBlock(width, height, &x, &y, &surface->lightmapNum))
			{
				throw std::runtime_error("Lightmap allocation failed");
			}
		}

		lock.unlock();

		// calculate texture coordinates
		for (i = 0; i < surface->numVerts; i++)
		{
			tDelta = surface->verts[i] - surface->bounds.min;
			surface->lightmapCoords[i * 2 + 0] =
				(tDelta.Dot(surface->textureCoords[0]) + x + 0.5f) / (float)textureWidth;
			surface->lightmapCoords[i * 2 + 1] =
				(tDelta.Dot(surface->textureCoords[1]) + y + 0.5f) / (float)textureHeight;
		}

		surface->lightmapOffs[0] = x;
		surface->lightmapOffs[1] = y;
	}

	std::unique_lock<std::mutex> lock(mutex);
	currentTexture = textures[surface->lightmapNum].data();
	lock.unlock();

	// store results to lightmap texture
	for (i = 0; i < sampleHeight; i++)
	{
		for (j = 0; j < sampleWidth; j++)
		{
			// get texture offset
			int offs = (((textureWidth * (i + surface->lightmapOffs[1])) + surface->lightmapOffs[0]) * 3);

			// convert RGB to bytes
			currentTexture[offs + j * 3 + 0] = floatToHalf(colorSamples[i * LIGHTMAP_MAX_SIZE + j].x);
			currentTexture[offs + j * 3 + 1] = floatToHalf(colorSamples[i * LIGHTMAP_MAX_SIZE + j].y);
			currentTexture[offs + j * 3 + 2] = floatToHalf(colorSamples[i * LIGHTMAP_MAX_SIZE + j].z);
		}
	}
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

static kexVec2 Hammersley(uint32_t i, uint32_t N)
{
	return kexVec2(float(i) / float(N), RadicalInverse_VdC(i));
}

static kexVec3 ImportanceSampleGGX(kexVec2 Xi, kexVec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * M_PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a*a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	kexVec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

	// from tangent-space vector to world-space sample vector
	kexVec3 up = std::abs(N.z) < 0.999f ? kexVec3(0.0f, 0.0f, 1.0f) : kexVec3(1.0f, 0.0f, 0.0f);
	kexVec3 tangent = kexVec3::Normalize(kexVec3::Cross(up, N));
	kexVec3 bitangent = kexVec3::Cross(N, tangent);

	kexVec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return kexVec3::Normalize(sampleVec);
}

void kexLightmapBuilder::TraceIndirectLight(surface_t *surface)
{
	if (surface->lightmapNum == -1)
		return;

	int sampleWidth = surface->lightmapDims[0];
	int sampleHeight = surface->lightmapDims[1];

	kexVec3 normal = surface->plane.Normal();

	uint16_t *currentTexture = &indirectoutput[surface->lightmapNum * textureWidth * textureHeight * 3];

	for (int i = 0; i < sampleHeight; i++)
	{
		for (int j = 0; j < sampleWidth; j++)
		{
			kexVec3 pos = surface->lightmapOrigin + normal +
				(surface->lightmapSteps[0] * (float)j) +
				(surface->lightmapSteps[1] * (float)i);

			const int SAMPLE_COUNT = 128;// 1024;

			float totalWeight = 0.0f;
			kexVec3 c(0.0f, 0.0f, 0.0f);

			for (int i = 0; i < SAMPLE_COUNT; i++)
			{
				kexVec2 Xi = Hammersley(i, SAMPLE_COUNT);
				kexVec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
				kexVec3 L = kexVec3::Normalize(H * (2.0f * kexVec3::Dot(normal, H)) - normal);

				float NdotL = std::max(kexVec3::Dot(normal, L), 0.0f);
				if (NdotL > 0.0f)
				{
					tracedTexels++;
					LevelTraceHit hit = map->Trace(pos, pos + L * 1000.0f);
					if (hit.fraction < 1.0f)
					{
						kexVec3 surfaceLight;
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

							int hitTexelX = clamp((int)(u * textureWidth + 0.5f), 0, textureWidth - 1);
							int hitTexelY = clamp((int)(v * textureHeight + 0.5f), 0, textureHeight - 1);

							uint16_t *hitTexture = textures[hit.hitSurface->lightmapNum].data();
							uint16_t *hitPixel = hitTexture + (hitTexelX + hitTexelY * textureWidth) * 3;

							float attenuation = (1.0f - hit.fraction);
							surfaceLight.x = halfToFloat(hitPixel[0]) * attenuation;
							surfaceLight.y = halfToFloat(hitPixel[1]) * attenuation;
							surfaceLight.z = halfToFloat(hitPixel[2]) * attenuation;
						}
						c += surfaceLight * NdotL;
					}
					totalWeight += NdotL;
				}
			}

			c = c / totalWeight;

			// convert RGB to bytes
			int tx = j + surface->lightmapOffs[0];
			int ty = i + surface->lightmapOffs[1];
			uint16_t *pixel = currentTexture + (tx + ty * textureWidth) * 3;
			pixel[0] = floatToHalf(c.x);
			pixel[1] = floatToHalf(c.y);
			pixel[2] = floatToHalf(c.z);
		}
	}
}

void kexLightmapBuilder::LightSurface(const int surfid)
{
	BuildSurfaceParams(surfaces[surfid].get());
	TraceSurface(surfaces[surfid].get());

	std::unique_lock<std::mutex> lock(mutex);

	int numsurfs = surfaces.size();
	int lastproc = processed * 100 / numsurfs;
	processed++;
	int curproc = processed * 100 / numsurfs;
	if (lastproc != curproc || processed == 1)
	{
		float remaining = (float)processed / (float)numsurfs;
		printf("%i%c surfaces done\r", (int)(remaining * 100.0f), '%');
	}
}

void kexLightmapBuilder::LightIndirect(const int surfid)
{
	TraceIndirectLight(surfaces[surfid].get());

	int numsurfs = surfaces.size();
	int lastproc = processed * 100 / numsurfs;
	processed++;
	int curproc = processed * 100 / numsurfs;
	if (lastproc != curproc || processed == 1)
	{
		float remaining = (float)processed / (float)numsurfs;
		printf("%i%c surfaces done\r", (int)(remaining * 100.0f), '%');
	}
}

void kexLightmapBuilder::CreateLightmaps(FLevel &doomMap)
{
	map = &doomMap;

	printf("-------------- Tracing cells ---------------\n");

	SetupLightCellGrid();

	processed = 0;
	tracedTexels = 0;
	kexWorker::RunJob(grid.blocks.size(), [=](int id) {
		LightBlock(id);
	});

	printf("Cells traced: %i \n\n", tracedTexels);

	printf("------------- Tracing surfaces -------------\n");

	tracedTexels = 0;
	processed = 0;
	kexWorker::RunJob(surfaces.size(), [=](int id) {
		LightSurface(id);
	});

	printf("Texels traced: %i \n\n", tracedTexels);

	printf("------------- Tracing indirect -------------\n");

	indirectoutput.resize(textures.size() * textureWidth * textureHeight * 3);

	tracedTexels = 0;
	processed = 0;
	kexWorker::RunJob(surfaces.size(), [=](int id) {
		LightIndirect(id);
	});

	for (size_t i = 0; i < textures.size(); i++)
	{
		uint16_t *tex = textures[i].data();
		uint16_t *indirect = &indirectoutput[i * textureWidth * textureHeight * 3];
		int count = textureWidth * textureHeight * 3;
		for (int j = 0; j < count; j++)
		{
			tex[j] = floatToHalf(halfToFloat(tex[j]) + halfToFloat(indirect[j]));
		}
	}

	printf("Texels traced: %i \n\n", tracedTexels);
}

void kexLightmapBuilder::SetupLightCellGrid()
{
	kexBBox worldBBox = map->CollisionMesh->get_bbox();
	float blockWorldSize = LIGHTCELL_BLOCK_SIZE * LIGHTCELL_SIZE;
	grid.x = static_cast<int>(std::floor(worldBBox.min.x / blockWorldSize));
	grid.y = static_cast<int>(std::floor(worldBBox.min.y / blockWorldSize));
	grid.width = static_cast<int>(std::ceil(worldBBox.max.x / blockWorldSize)) - grid.x;
	grid.height = static_cast<int>(std::ceil(worldBBox.max.y / blockWorldSize)) - grid.y;
	grid.blocks.resize(grid.width * grid.height);
}

void kexLightmapBuilder::LightBlock(int id)
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

					kexVec3 color;
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

	std::unique_lock<std::mutex> lock(mutex);

	int numblocks = grid.blocks.size();
	int lastproc = processed * 100 / numblocks;
	processed++;
	int curproc = processed * 100 / numblocks;
	if (lastproc != curproc || processed == 1)
	{
		float remaining = (float)processed / (float)numblocks;
		printf("%i%c cells done\r", (int)(remaining * 100.0f), '%');
	}

}

void kexLightmapBuilder::AddLightmapLump(FWadWriter &wadFile)
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
	kexBinFile lumpFile;
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
		for (unsigned int j = 0; j < count; j++)
		{
			lumpFile.Write16(textures[i][j]);
		}
	}

#if 0
	// Apply compression predictor
	uint8_t *texBytes = lumpFile.BufferAt() - texDataSize;
	for (int i = texDataSize - 1; i > 0; i--)
	{
		texBytes[i] -= texBytes[i - 1];
	}
#endif

	// Compress and store in lump
	ZLibOut zout(wadFile);
	wadFile.StartWritingLump("LIGHTMAP");
	zout.Write(buffer.data(), lumpFile.BufferAt() - lumpFile.Buffer());
}

/*
void kexLightmapBuilder::WriteTexturesToTGA()
{
	kexBinFile file;

	for (unsigned int i = 0; i < textures.size(); i++)
	{
		file.Create(Va("lightmap_%02d.tga", i));
		file.Write16(0);
		file.Write16(2);
		file.Write16(0);
		file.Write16(0);
		file.Write16(0);
		file.Write16(0);
		file.Write16(textureWidth);
		file.Write16(textureHeight);
		file.Write16(24);

		for (int j = 0; j < (textureWidth * textureHeight) * 3; j += 3)
		{
			file.Write8(textures[i][j + 2]);
			file.Write8(textures[i][j + 1]);
			file.Write8(textures[i][j + 0]);
		}
		file.Close();
	}
}
*/

void kexLightmapBuilder::WriteMeshToOBJ()
{
	FILE *f = fopen("mesh.obj", "w");

	std::map<int, std::vector<surface_t*>> sortedSurfs;

	for (unsigned int i = 0; i < surfaces.size(); i++)
		sortedSurfs[surfaces[i]->lightmapNum].push_back(surfaces[i].get());

	for (const auto &it : sortedSurfs)
	{
		for (const auto &s : it.second)
		{
			for (int j = 0; j < s->numVerts; j++)
			{
				fprintf(f, "v %f %f %f\n", s->verts[j].x, s->verts[j].z + 100.0f, s->verts[j].y);
			}
		}
	}

	for (const auto &it : sortedSurfs)
	{
		for (const auto &s : it.second)
		{
			for (int j = 0; j < s->numVerts; j++)
			{
				fprintf(f, "vt %f %f\n", s->lightmapCoords[j * 2], s->lightmapCoords[j * 2 + 1]);
			}
		}
	}

	int voffset = 1;
	for (const auto &it : sortedSurfs)
	{
		int lightmapNum = it.first;

		if (lightmapNum != -1)
			fprintf(f, "usemtl lightmap_%02d\n", lightmapNum);
		else
			fprintf(f, "usemtl black\n");

		for (const auto &s : it.second)
		{
			switch (s->type)
			{
			case ST_FLOOR:
				for (int j = 2; j < s->numVerts; j++)
				{
					fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j, voffset + j, voffset + j - 1, voffset + j - 1, voffset, voffset);
				}
				break;
			case ST_CEILING:
				for (int j = 2; j < s->numVerts; j++)
				{
					fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset, voffset, voffset + j - 1, voffset + j - 1, voffset + j, voffset + j);
				}
				break;
			default:
				for (int j = 2; j < s->numVerts; j++)
				{
					if (j % 2 == 0)
						fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j - 2, voffset + j - 2, voffset + j - 1, voffset + j - 1, voffset + j, voffset + j);
					else
						fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j, voffset + j, voffset + j - 1, voffset + j - 1, voffset + j - 2, voffset + j - 2);
				}
				break;
			}

			voffset += s->numVerts;
		}
	}

	fclose(f);
}
