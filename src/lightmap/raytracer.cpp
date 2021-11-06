
#include "math/mathlib.h"
#include "surfaces.h"
#include "level/level.h"
#include "raytracer.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>

extern int LightBounce;

Raytracer::Raytracer()
{
}

Raytracer::~Raytracer()
{
}

void Raytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	printf("Tracing light probes\n");

	Worker::RunJob((int)mesh->lightProbes.size(), [=](int id) {
		RaytraceProbeSample(&mesh->lightProbes[id]);
	});

	printf("Tracing surfaces (%d bounces)\n", LightBounce);

	struct SurfaceTask
	{
		int surf, x, y;
	};
	std::vector<SurfaceTask> tasks;

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();
		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		for (int y = 0; y < sampleHeight; y++)
		{
			for (int x = 0; x < sampleWidth; x++)
			{
				SurfaceTask task;
				task.surf = (int)i;
				task.x = x;
				task.y = y;
				tasks.push_back(task);
			}
		}
	}

	Worker::RunJob((int)tasks.size(), [=](int id) {
		const SurfaceTask& task = tasks[id];
		RaytraceSurfaceSample(mesh->surfaces[task.surf].get(), task.x, task.y);
	});

	printf("Raytrace complete\n");
}

void Raytracer::RaytraceProbeSample(LightProbeSample* probe)
{
	Vec3 incoming(0.0f, 0.0f, 0.0f);

	if (LightBounce > 0)
	{
		Vec3 directions[6] =
		{
			{  1.0f,  0.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f },
			{  0.0f,  1.0f,  0.0f },
			{  0.0f, -1.0f,  0.0f },
			{  0.0f,  0.0f,  1.0f, },
			{  0.0f,  0.0f, -1.0f, }
		};
		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			const Vec3& normal = directions[i % 6];
			Vec2 Xi = Hammersley(i, SAMPLE_COUNT);
			Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
			Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);
			incoming += TracePath(probe->Position, L, i);
		}
		incoming = incoming / (float)SAMPLE_COUNT / (float)LightBounce;
	}

	for (ThingLight& light : mesh->map->ThingLights)
	{
		Vec3 lightOrigin = light.LightOrigin();
		float lightRadius = light.LightRadius();

		if (probe->Position.DistanceSq(lightOrigin) > (lightRadius * lightRadius))
			continue;

		if (mesh->TraceAnyHit(lightOrigin, probe->Position))
			continue; // this light is occluded by something

		Vec3 dir = (lightOrigin - probe->Position);
		float dist = dir.Unit();
		dir.Normalize();

		incoming += light.rgb * (light.SpotAttenuation(dir) * light.DistAttenuation(dist) * light.intensity);
	}

	const Vec3& sundir = mesh->map->GetSunDirection();
	LevelTraceHit trace = mesh->Trace(probe->Position, probe->Position + sundir * 32768.0f);
	if (trace.fraction != 1.0f && trace.hitSurface->bSky)
		incoming += mesh->map->GetSunColor();

	probe->Color = incoming;
}

void Raytracer::RaytraceSurfaceSample(Surface* surface, int x, int y)
{
	Vec3 normal = surface->plane.Normal();
	Vec3 pos = surface->lightmapOrigin + normal + surface->lightmapSteps[0] * (float)x + surface->lightmapSteps[1] * (float)y;

	Vec3 incoming(0.0f, 0.0f, 0.0f);
	if (LightBounce > 0)
	{
		float totalWeight = 0.0f;
		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			Vec2 Xi = Hammersley(i, SAMPLE_COUNT);
			Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
			Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);
			float NdotL = std::max(Vec3::Dot(normal, L), 0.0f);
			if (NdotL > 0.0f)
			{
				incoming += TracePath(pos, L, i) * NdotL;
				totalWeight += NdotL;
			}
		}
		incoming = incoming / totalWeight / (float)LightBounce;
	}

	incoming = incoming + GetSurfaceEmittance(surface, 0.0f) + GetLightEmittance(surface, pos);

	const Vec3& sundir = mesh->map->GetSunDirection();
	float attenuation = normal.Dot(sundir);
	if (attenuation > 0.0f)
	{
		LevelTraceHit trace = mesh->Trace(pos, pos + sundir * 32768.0f);
		if (trace.fraction != 1.0f && trace.hitSurface->bSky)
			incoming += mesh->map->GetSunColor() * attenuation;
	}

	size_t sampleWidth = surface->lightmapDims[0];
	surface->samples[x + y * sampleWidth] = incoming;
}

Vec3 Raytracer::GetLightEmittance(Surface* surface, const Vec3& pos)
{
	Vec3 emittance = Vec3(0.0f);
	for (ThingLight& light : mesh->map->ThingLights)
	{
		Vec3 lightOrigin = light.LightOrigin();
		float lightRadius = light.LightRadius();

		if (surface->plane.Distance(lightOrigin) - surface->plane.d < 0)
			continue; // completely behind the plane

		if (pos.DistanceSq(lightOrigin) > (lightRadius * lightRadius))
			continue; // light too far away

		Vec3 dir = (lightOrigin - pos);
		float dist = dir.Unit();
		dir.Normalize();

		float attenuation = light.SpotAttenuation(dir) * light.DistAttenuation(dist) * surface->plane.Normal().Dot(dir);
		if (attenuation <= 0.0f)
			continue;

		if (mesh->TraceAnyHit(lightOrigin, pos))
			continue; // this light is occluded by something

		emittance += light.rgb * (attenuation * light.intensity);
	}
	return emittance;
}

Vec3 Raytracer::TracePath(const Vec3& pos, const Vec3& dir, int sampleIndex, int depth)
{
	if (depth >= LightBounce)
		return Vec3(0.0f);

	LevelTraceHit hit = mesh->Trace(pos + dir * 0.1f, pos + dir * 2000.0f);
	if (hit.fraction == 1.0f)
		return Vec3(0.0f);

	Vec3 normal = hit.hitSurface->plane.Normal();
	Vec3 hitpos = hit.start * (1.0f - hit.fraction) + hit.end * hit.fraction;

	Vec3 emittance = GetSurfaceEmittance(hit.hitSurface, pos.Distance(hitpos)) + GetLightEmittance(hit.hitSurface, hitpos) * 0.5f;

	const Vec3& sundir = mesh->map->GetSunDirection();
	float attenuation = normal.Dot(sundir);
	if (attenuation > 0.0f)
	{
		Vec3 start = hitpos + normal * 0.1f;
		LevelTraceHit trace = mesh->Trace(start, start + sundir * 32768.0f);
		if (trace.fraction != 1.0f && trace.hitSurface->bSky)
			emittance += mesh->map->GetSunColor() * (attenuation * 0.5f);
	}

	Vec2 Xi = Hammersley(sampleIndex, SAMPLE_COUNT);
	Vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
	Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);

	float NdotL = Vec3::Dot(normal, L);
	if (NdotL <= 0.0f)
		return emittance;

	const float p = 1 / (2 * M_PI);
	Vec3 incoming = TracePath(hitpos, normal, (sampleIndex + depth + 1) % SAMPLE_COUNT, depth + 1);

	return emittance + incoming * NdotL / p;
}

float Raytracer::RadicalInverse_VdC(uint32_t bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

Vec2 Raytracer::Hammersley(uint32_t i, uint32_t N)
{
	return Vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

Vec3 Raytracer::ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * M_PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
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

Vec3 Raytracer::GetSurfaceEmittance(Surface* surface, float distance)
{
	SurfaceLightDef* def = nullptr;
	if (surface->type >= ST_MIDDLESIDE && surface->type <= ST_LOWERSIDE)
	{
		int lightdefidx = mesh->map->Sides[surface->typeIndex].lightdef;
		if (lightdefidx != -1)
		{
			def = &mesh->map->SurfaceLights[lightdefidx];
		}
	}
	else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
	{
		MapSubsectorEx* sub = &mesh->map->GLSubsectors[surface->typeIndex];
		IntSector* sector = mesh->map->GetSectorFromSubSector(sub);

		if (sector && surface->numVerts > 0)
		{
			if (sector->floorlightdef != -1 && surface->type == ST_FLOOR)
			{
				def = &mesh->map->SurfaceLights[sector->floorlightdef];
			}
			else if (sector->ceilinglightdef != -1 && surface->type == ST_CEILING)
			{
				def = &mesh->map->SurfaceLights[sector->ceilinglightdef];
			}
		}
	}

	if (def && distance < def->distance + def->distance)
	{
		float radius = def->distance + def->distance;
		float attenuation = std::max(1.0f - (distance / radius), 0.0f);
		return def->rgb * (attenuation * def->intensity);
	}
	else
	{
		return Vec3(0.0f);
	}
}
