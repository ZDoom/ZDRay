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

DLightRaytracer::DLightRaytracer()
{
}

DLightRaytracer::~DLightRaytracer()
{
}

void DLightRaytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	CreateSurfaceLights();
	CreateTraceTasks();

	SetupTaskProcessed("Tracing light probes", mesh->lightProbes.size());
	Worker::RunJob(mesh->lightProbes.size(), [=](int id) {
		LightProbe(id);
		PrintTaskProcessed();
	});
	printf("Probes traced: %i \n\n", (int)mesh->lightProbes.size());

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
}

// Traces to the ceiling surface. Will emit light if the surface that was traced is a sky
bool DLightRaytracer::EmitFromCeiling(const Surface *surface, const Vec3 &origin, const Vec3 &normal, Vec3 &color)
{
	float attenuation = surface ? normal.Dot(mesh->map->GetSunDirection()) : 1.0f;

	if (attenuation <= 0)
	{
		// plane is not even facing the sunlight
		return false;
	}

	LevelTraceHit trace = mesh->Trace(origin, origin + (mesh->map->GetSunDirection() * 32768.0f));

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

	color += mesh->map->GetSunColor() * attenuation;

	return true;
}

// Traces a line from the texel's origin to the sunlight direction and against all nearby thing lights
Vec3 DLightRaytracer::LightTexelSample(const Vec3 &origin, Surface *surface)
{
	Plane plane;
	if (surface)
		plane = surface->plane;

	Vec3 color(0.0f, 0.0f, 0.0f);

	// check all thing lights
	for (unsigned int i = 0; i < mesh->map->ThingLights.Size(); i++)
	{
		ThingLight *tl = &mesh->map->ThingLights[i];

		Vec3 lightOrigin = tl->LightOrigin();

		if (surface && plane.Distance(lightOrigin) - plane.d < 0)
		{
			// completely behind the plane
			continue;
		}

		float radius = tl->LightRadius();

		if (origin.DistanceSq(lightOrigin) > (radius*radius))
		{
			// not within range
			continue;
		}

		Vec3 dir = (lightOrigin - origin);
		float dist = dir.Unit();
		dir.Normalize();

		float spotAttenuation = tl->SpotAttenuation(dir);
		if (spotAttenuation == 0.0f)
			continue;

		if (mesh->TraceAnyHit(lightOrigin, origin))
		{
			// this light is occluded by something
			continue;
		}

		float attenuation = 1.0f - (dist / radius);
		attenuation *= spotAttenuation;
		if (surface)
			attenuation *= plane.Normal().Dot(dir);
		attenuation *= tl->intensity;

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

		float attenuation = surfaceLight->TraceSurface(mesh, surface, origin);
		if (attenuation > 0.0f)
		{
			color += surfaceLight->GetRGB() * surfaceLight->Intensity() * attenuation;
			tracedTexels++;
		}
	}

	return color;
}

// Steps through each texel and traces a line to the world.
void DLightRaytracer::TraceSurface(Surface *surface, int offset)
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

void DLightRaytracer::TraceIndirectLight(Surface *surface, int offset)
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

void DLightRaytracer::CreateTraceTasks()
{
	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface *surface = mesh->surfaces[i].get();

		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		int total = sampleWidth * sampleHeight;
		int count = (total + TraceTask::tasksize - 1) / TraceTask::tasksize;
		for (int j = 0; j < count; j++)
			traceTasks.push_back(TraceTask(i, j * TraceTask::tasksize));
	}
}

void DLightRaytracer::LightSurface(const int taskid)
{
	const TraceTask &task = traceTasks[taskid];
	TraceSurface(mesh->surfaces[task.surface].get(), task.offset);
}

void DLightRaytracer::LightIndirect(const int taskid)
{
	const TraceTask &task = traceTasks[taskid];
	TraceIndirectLight(mesh->surfaces[task.surface].get(), task.offset);
}

void DLightRaytracer::CreateSurfaceLights()
{
	for (size_t j = 0; j < mesh->surfaces.size(); ++j)
	{
		Surface *surface = mesh->surfaces[j].get();

		if (surface->type >= ST_MIDDLESIDE && surface->type <= ST_LOWERSIDE)
		{
			int lightdefidx = mesh->map->Sides[surface->typeIndex].lightdef;
			if (lightdefidx != -1)
			{
				auto surfaceLight = std::make_unique<SurfaceLight>(mesh->map->SurfaceLights[lightdefidx], surface);
				surfaceLight->Subdivide(16);
				surfaceLights.push_back(std::move(surfaceLight));
			}
		}
		else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
		{
			MapSubsectorEx *sub = &mesh->map->GLSubsectors[surface->typeIndex];
			IntSector *sector = mesh->map->GetSectorFromSubSector(sub);

			if (sector && surface->numVerts > 0)
			{
				if (sector->floorlightdef != -1 && surface->type == ST_FLOOR)
				{
					auto surfaceLight = std::make_unique<SurfaceLight>(mesh->map->SurfaceLights[sector->floorlightdef], surface);
					surfaceLight->Subdivide(16);
					surfaceLights.push_back(std::move(surfaceLight));
				}
				else if (sector->ceilinglightdef != -1 && surface->type == ST_CEILING)
				{
					auto surfaceLight = std::make_unique<SurfaceLight>(mesh->map->SurfaceLights[sector->ceilinglightdef], surface);
					surfaceLight->Subdivide(16);
					surfaceLights.push_back(std::move(surfaceLight));
				}
			}
		}
	}
}

void DLightRaytracer::LightProbe(int id)
{
	mesh->lightProbes[id].Color = LightTexelSample(mesh->lightProbes[id].Position, nullptr);
}

void DLightRaytracer::SetupTaskProcessed(const char *name, int total)
{
	printf("-------------- %s ---------------\n", name);

	processed = 0;
	progresstotal = total;
}

void DLightRaytracer::PrintTaskProcessed()
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
