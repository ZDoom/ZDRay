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
#include <zlib.h>

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

	lightProbes.resize(map->ThingLightProbes.Size(), Vec3(0.0f, 0.0f, 0.0f));

	SetupTaskProcessed("Tracing light probes", lightProbes.size());
	Worker::RunJob(lightProbes.size(), [=](int id) {
		LightProbe(id);
		PrintTaskProcessed();
	});
	printf("Probes traced: %i \n\n", tracedTexels);

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
			spotDir.x = -std::cos(radians(tl->mapThing->angle)) * xyLen;
			spotDir.y = -std::sin(radians(tl->mapThing->angle)) * xyLen;
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

void LightmapBuilder::LightProbe(int id)
{
	int thingIndex = map->ThingLightProbes[id];
	const IntThing& thing = map->Things[thingIndex];
	float x = (float)(thing.x >> FRACBITS);
	float y = (float)(thing.y >> FRACBITS);
	float z = (float)thing.z + thing.height * 0.5f;

	lightProbes[id] = LightTexelSample({ x, y, z }, nullptr);
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

	int version = 0;
	int headerSize = 4 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
	int surfacesSize = surfaces.size() * 5 * sizeof(uint32_t);
	int texCoordsSize = numTexCoords * 2 * sizeof(float);
	int texDataSize = textures.size() * textureWidth * textureHeight * 3 * 2;
	int lightProbesSize = lightProbes.size() * (1 + sizeof(uint32_t) + 3 * sizeof(float));
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

	// Write light probes
	for (size_t i = 0; i < lightProbes.size(); i++)
	{
		lumpFile.Write32(map->ThingLightProbes[i]);
		lumpFile.WriteFloat(lightProbes[i].x);
		lumpFile.WriteFloat(lightProbes[i].y);
		lumpFile.WriteFloat(lightProbes[i].z);
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

class PNGWriter
{
public:
	static void save(const std::string &filename, int width, int height, int bytes_per_pixel, void *pixels)
	{
		PNGImage image;
		image.width = width;
		image.height = height;
		image.bytes_per_pixel = bytes_per_pixel;
		image.pixel_ratio = 1.0f;
		image.data = pixels;

		FILE *file = fopen(filename.c_str(), "wb");
		if (file)
		{
			PNGWriter writer;
			writer.file = file;
			writer.image = &image;
			writer.write_magic();
			writer.write_headers();
			writer.write_data();
			writer.write_chunk("IEND", nullptr, 0);
			fclose(file);
		}
	}

private:
	struct PNGImage
	{
		int width;
		int height;
		int bytes_per_pixel;
		void *data;
		float pixel_ratio;
	};

	struct DataBuffer
	{
		DataBuffer(int size) : size(size) { data = new uint8_t[size]; }
		~DataBuffer() { delete[] data; }
		int size;
		void *data;
	};

	const PNGImage *image;
	FILE *file;

	class PNGCRC32
	{
	public:
		static unsigned long crc(const char name[4], const void *data, int len)
		{
			static PNGCRC32 impl;

			const unsigned char *buf = reinterpret_cast<const unsigned char*>(data);

			unsigned int c = 0xffffffff;

			for (int n = 0; n < 4; n++)
				c = impl.crc_table[(c ^ name[n]) & 0xff] ^ (c >> 8);

			for (int n = 0; n < len; n++)
				c = impl.crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

			return c ^ 0xffffffff;
		}

	private:
		unsigned int crc_table[256];

		PNGCRC32()
		{
			for (unsigned int n = 0; n < 256; n++)
			{
				unsigned int c = n;
				for (unsigned int k = 0; k < 8; k++)
				{
					if ((c & 1) == 1)
						c = 0xedb88320 ^ (c >> 1);
					else
						c = c >> 1;
				}
				crc_table[n] = c;
			}
		}
	};

	void write_magic()
	{
		unsigned char png_magic[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
		write(png_magic, 8);
	}

	void write_headers()
	{
		int ppm = (int)std::round(3800 * image->pixel_ratio);
		int ppm_x = ppm;
		int ppm_y = ppm;

		int width = image->width;
		int height = image->height;
		int bit_depth = image->bytes_per_pixel == 8 ? 16 : 8;
		int color_type = 6;
		int compression_method = 0;
		int filter_method = 0;
		int interlace_method = 0;

		unsigned char idhr[13];
		idhr[0] = (width >> 24) & 0xff;
		idhr[1] = (width >> 16) & 0xff;
		idhr[2] = (width >> 8) & 0xff;
		idhr[3] = width & 0xff;
		idhr[4] = (height >> 24) & 0xff;
		idhr[5] = (height >> 16) & 0xff;
		idhr[6] = (height >> 8) & 0xff;
		idhr[7] = height & 0xff;
		idhr[8] = bit_depth;
		idhr[9] = color_type;
		idhr[10] = compression_method;
		idhr[11] = filter_method;
		idhr[12] = interlace_method;

		//unsigned char srgb[1];
		//srgb[0] = 0;

		unsigned char phys[9];
		phys[0] = (ppm_x >> 24) & 0xff;
		phys[1] = (ppm_x >> 16) & 0xff;
		phys[2] = (ppm_x >> 8) & 0xff;
		phys[3] = ppm_x & 0xff;
		phys[4] = (ppm_y >> 24) & 0xff;
		phys[5] = (ppm_y >> 16) & 0xff;
		phys[6] = (ppm_y >> 8) & 0xff;
		phys[7] = ppm_y & 0xff;
		phys[8] = 1; // pixels per meter

		write_chunk("IHDR", idhr, 13);

		if (ppm != 0)
			write_chunk("pHYs", phys, 9);

		//write_chunk("sRGB", srgb, 1);
	}

	void write_data()
	{
		//int width = image->width;
		int height = image->height;
		int bytes_per_pixel = image->bytes_per_pixel;
		int pitch = image->width * bytes_per_pixel;

		std::vector<unsigned char> scanline_orig;
		std::vector<unsigned char> scanline_filtered;
		scanline_orig.resize((image->width + 1) * bytes_per_pixel);
		scanline_filtered.resize(image->width * bytes_per_pixel + 1);

		auto idat_uncompressed = std::make_shared<DataBuffer>(height * scanline_filtered.size());

		for (int y = 0; y < height; y++)
		{
			// Grab scanline
			memcpy(scanline_orig.data() + bytes_per_pixel, (uint8_t*)image->data + y * pitch, scanline_orig.size() - bytes_per_pixel);

			// Convert to big endian for 16 bit
			if (bytes_per_pixel == 8)
			{
				for (size_t x = 0; x < scanline_orig.size(); x += 2)
				{
					std::swap(scanline_orig[x], scanline_orig[x + 1]);
				}
			}

			// Filter scanline
			/*
			scanline_filtered[0] = 0; // None filter type
			for (int i = bytes_per_pixel; i < scanline_orig.size(); i++)
			{
				scanline_filtered[i - bytes_per_pixel + 1] = scanline_orig[i];
			}
			*/
			scanline_filtered[0] = 1; // Sub filter type
			for (int i = bytes_per_pixel; i < scanline_orig.size(); i++)
			{
				unsigned char a = scanline_orig[i - bytes_per_pixel];
				unsigned char x = scanline_orig[i];
				scanline_filtered[i - bytes_per_pixel + 1] = x - a;
			}

			// Output scanline
			memcpy((uint8_t*)idat_uncompressed->data + y * scanline_filtered.size(), scanline_filtered.data(), scanline_filtered.size());
		}

		auto idat = std::make_unique<DataBuffer>(idat_uncompressed->size * 125 / 100);
		idat->size = compress(idat.get(), idat_uncompressed.get(), false);

		write_chunk("IDAT", idat->data, (int)idat->size);
	}

	void write_chunk(const char name[4], const void *data, int size)
	{
		unsigned char size_data[4];
		size_data[0] = (size >> 24) & 0xff;
		size_data[1] = (size >> 16) & 0xff;
		size_data[2] = (size >> 8) & 0xff;
		size_data[3] = size & 0xff;
		write(size_data, 4);

		write(name, 4);

		write(data, size);
		unsigned int crc32 = PNGCRC32::crc(name, data, size);

		unsigned char crc32_data[4];
		crc32_data[0] = (crc32 >> 24) & 0xff;
		crc32_data[1] = (crc32 >> 16) & 0xff;
		crc32_data[2] = (crc32 >> 8) & 0xff;
		crc32_data[3] = crc32 & 0xff;
		write(crc32_data, 4);
	}

	void write(const void *data, int size)
	{
		fwrite(data, size, 1, file);
	}

	size_t compress(DataBuffer *out, const DataBuffer *data, bool raw)
	{
		if (data->size > (size_t)0xffffffff || out->size > (size_t)0xffffffff)
			throw std::runtime_error("Data is too big");

		const int window_bits = 15;

		int compression_level = 6;
		int strategy = Z_DEFAULT_STRATEGY;

		z_stream zs;
		memset(&zs, 0, sizeof(z_stream));
		int result = deflateInit2(&zs, compression_level, Z_DEFLATED, raw ? -window_bits : window_bits, 8, strategy); // Undocumented: if wbits is negative, zlib skips header check
		if (result != Z_OK)
			throw std::runtime_error("Zlib deflateInit failed");

		zs.next_in = (unsigned char *)data->data;
		zs.avail_in = (unsigned int)data->size;
		zs.next_out = (unsigned char *)out->data;
		zs.avail_out = (unsigned int)out->size;

		size_t outSize = 0;
		try
		{
			int result = deflate(&zs, Z_FINISH);
			if (result == Z_NEED_DICT) throw std::runtime_error("Zlib deflate wants a dictionary!");
			if (result == Z_DATA_ERROR) throw std::runtime_error("Zip data stream is corrupted");
			if (result == Z_STREAM_ERROR) throw std::runtime_error("Zip stream structure was inconsistent!");
			if (result == Z_MEM_ERROR) throw std::runtime_error("Zlib did not have enough memory to compress file!");
			if (result == Z_BUF_ERROR) throw std::runtime_error("Not enough data in buffer when Z_FINISH was used");
			if (result != Z_STREAM_END) throw std::runtime_error("Zlib deflate failed while compressing zip file!");
			outSize = zs.total_out;
		}
		catch (...)
		{
			deflateEnd(&zs);
			throw;
		}
		deflateEnd(&zs);

		return outSize;
	}
};

void LightmapBuilder::ExportMesh(std::string filename)
{
	mesh->Export(filename);

	int index = 0;
	for (const auto &texture : textures)
	{
		int w = texture->Width();
		int h = texture->Height();
		uint16_t *p = texture->Pixels();
#if 1
		std::vector<uint8_t> buf(w * h * 4);
		uint8_t *buffer = buf.data();
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
		uint16_t *buffer = buf.data();
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
