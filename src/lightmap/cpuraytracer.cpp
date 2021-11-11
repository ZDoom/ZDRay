
#include "math/mathlib.h"
#include "surfaces.h"
#include "level/level.h"
#include "cpuraytracer.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>

extern int LightBounce;
extern bool VKDebug;

CPURaytracer::CPURaytracer()
{
}

CPURaytracer::~CPURaytracer()
{
}

void CPURaytracer::Raytrace(LevelMesh* level)
{
	mesh = level;

	std::vector<CPUTraceTask> tasks;
	for (size_t i = 0; i < mesh->lightProbes.size(); i++)
	{
		CPUTraceTask task;
		task.id = -(int)(i + 2);
		task.x = 0;
		task.y = 0;
		tasks.push_back(task);
	}

	for (size_t i = 0; i < mesh->surfaces.size(); i++)
	{
		Surface* surface = mesh->surfaces[i].get();
		int sampleWidth = surface->lightmapDims[0];
		int sampleHeight = surface->lightmapDims[1];
		for (int y = 0; y < sampleHeight; y++)
		{
			for (int x = 0; x < sampleWidth; x++)
			{
				CPUTraceTask task;
				task.id = (int)i;
				task.x = x;
				task.y = y;
				tasks.push_back(task);
			}
		}
	}

	CollisionMesh = std::make_unique<TriangleMeshShape>(mesh->MeshVertices.Data(), mesh->MeshVertices.Size(), mesh->MeshElements.Data(), mesh->MeshElements.Size());
	CreateHemisphereVectors();
	CreateLights();

	printf("Ray tracing with %d bounce(s)\n", LightBounce);

	Worker::RunJob((int)tasks.size(), [=](int id) { RaytraceTask(tasks[id]); });

	printf("\nRaytrace complete\n");
}

void CPURaytracer::RaytraceTask(const CPUTraceTask& task)
{
	CPUTraceState state;
	state.EndTrace = false;

	if (task.id >= 0)
	{
		Surface* surface = mesh->surfaces[task.id].get();
		Vec3 pos = surface->lightmapOrigin + surface->lightmapSteps[0] * (float)task.x + surface->lightmapSteps[1] * (float)task.y;
		state.StartPosition = pos;
		state.StartSurface = surface;
	}
	else
	{
		LightProbeSample& probe = mesh->lightProbes[(size_t)(-task.id) - 2];
		state.StartPosition = probe.Position;
		state.StartSurface = nullptr;
	}

	state.SampleDistance = (float)mesh->samples;
	state.LightCount = mesh->map->ThingLights.Size();
	state.SunDir = mesh->map->GetSunDirection();
	state.SunColor = mesh->map->GetSunColor();
	state.SunIntensity = 1.0f;

	state.PassType = 0;
	state.SampleIndex = 0;
	state.SampleCount = bounceSampleCount;
	RunBounceTrace(state);

	state.SampleCount = coverageSampleCount;
	RunLightTrace(state);

	for (uint32_t i = 0; i < (uint32_t)bounceSampleCount && !state.EndTrace; i++)
	{
		state.PassType = 1;
		state.SampleIndex = i;
		state.SampleCount = bounceSampleCount;
		state.HemisphereVec = HemisphereVectors[state.SampleIndex];
		RunBounceTrace(state);

		for (int bounce = 0; bounce < LightBounce && !state.EndTrace; bounce++)
		{
			state.SampleCount = coverageSampleCount;
			RunLightTrace(state);

			state.PassType = 2;
			state.SampleIndex = (i + bounce) % state.SampleCount;
			state.SampleCount = bounceSampleCount;
			state.HemisphereVec = HemisphereVectors[state.SampleIndex];
			RunBounceTrace(state);
		}
	}

	if (task.id >= 0)
	{
		Surface* surface = mesh->surfaces[task.id].get();
		size_t sampleWidth = surface->lightmapDims[0];
		surface->samples[task.x + task.y * sampleWidth] = state.Output;
	}
	else
	{
		LightProbeSample& probe = mesh->lightProbes[(size_t)(-task.id) - 2];
		probe.Color = state.Output;
	}
}

void CPURaytracer::RunBounceTrace(CPUTraceState& state)
{
	Vec3 origin;
	Surface* surface;
	if (state.PassType == 2)
	{
		origin = state.Position;
		surface = state.Surface;
	}
	else
	{
		origin = state.StartPosition;
		surface = state.StartSurface;
	}

	Vec3 incoming(0.0f, 0.0f, 0.0f);
	float incomingAttenuation = 1.0f;

	if (state.PassType == 0)
	{
		if (surface)
		{
			CPUEmissiveSurface emissive = GetEmissive(surface);
			incoming = emissive.Color * emissive.Intensity;
		}
	}
	else
	{
		incoming = state.Output;
		incomingAttenuation = state.OutputAttenuation;

		if (state.PassType == 1)
			incomingAttenuation = 1.0f / float(state.SampleCount);

		Vec3 normal;
		if (surface)
		{
			normal = surface->plane.Normal();
		}
		else
		{
			switch (state.SampleIndex % 6)
			{
			case 0: normal = Vec3( 1.0f,  0.0f,  0.0f); break;
			case 1: normal = Vec3(-1.0f,  0.0f,  0.0f); break;
			case 2: normal = Vec3( 0.0f,  1.0f,  0.0f); break;
			case 3: normal = Vec3( 0.0f, -1.0f,  0.0f); break;
			case 4: normal = Vec3( 0.0f,  0.0f,  1.0f); break;
			case 5: normal = Vec3( 0.0f,  0.0f, -1.0f); break;
			}
		}

		Vec3 H = ImportanceSample(state.HemisphereVec, normal);
		Vec3 L = Vec3::Normalize(H * (2.0f * Vec3::Dot(normal, H)) - normal);

		float NdotL = std::max(Vec3::Dot(normal, L), 0.0f);

		const float p = (float)(1 / (2 * 3.14159265359));
		incomingAttenuation *= NdotL / p;

		state.EndTrace = true;
		if (NdotL > 0.0f)
		{
			Vec3 start = origin + normal * 0.1f;
			Vec3 end = start + L * 32768.0f;
			LevelTraceHit hit = Trace(start, end);
			if (hit.fraction < 1.0f)
			{
				state.EndTrace = false;
				surface = hit.hitSurface;
				Vec3 hitPosition = start * (1.0f - hit.fraction) + end * hit.fraction;

				CPUEmissiveSurface emissive = GetEmissive(surface);
				if (emissive.Distance > 0.0f)
				{
					float hitDistance = (hitPosition - origin).Length();
					float attenuation = std::max(1.0f - (hitDistance / emissive.Distance), 0.0f);
					incoming += emissive.Color * (emissive.Intensity * attenuation * incomingAttenuation);
				}

				origin = hitPosition;
			}
		}

		incomingAttenuation *= 0.25; // the amount of incoming light the surfaces emit
	}

	state.Position = origin;
	state.Surface = surface;
	state.Output = incoming;
	state.OutputAttenuation = incomingAttenuation;
}

void CPURaytracer::RunLightTrace(CPUTraceState& state)
{
	Vec3 incoming = state.Output;
	float incomingAttenuation = state.OutputAttenuation;
	if (incomingAttenuation <= 0.0f)
		return;

	Surface* surface = state.Surface;

	Vec3 origin = state.Position;
	Vec3 normal;
	if (surface)
	{
		normal = surface->plane.Normal();
		origin += normal * 0.1f;
	}

	const float minDistance = 0.01f;

	// Sun light
	{
		const float dist = 32768.0f;

		float attenuation = 0.0f;
		if (state.PassType == 0 && surface)
		{
			Vec3 e0 = Vec3::Cross(normal, std::abs(normal.x) < std::abs(normal.y) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f));
			Vec3 e1 = Vec3::Cross(normal, e0);
			e0 = Vec3::Cross(normal, e1);

			for (uint32_t i = 0; i < state.SampleCount; i++)
			{
				Vec2 offset = (Hammersley(i, state.SampleCount) - 0.5) * state.SampleDistance;
				Vec3 origin2 = origin + e0 * offset.x + e1 * offset.y;

				Vec3 start = origin2;
				Vec3 end = start + state.SunDir * dist;
				LevelTraceHit hit = Trace(start, end);
				if (hit.fraction < 1.0f && hit.hitSurface->bSky)
					attenuation += 1.0f;
			}
			attenuation *= 1.0f / float(state.SampleCount);
		}
		else
		{
			Vec3 start = origin;
			Vec3 end = start + state.SunDir * dist;
			LevelTraceHit hit = Trace(start, end);
			attenuation = (hit.fraction < 1.0f && hit.hitSurface->bSky) ? 1.0f : 0.0f;
		}
		incoming += state.SunColor * (attenuation * state.SunIntensity * incomingAttenuation);
	}

	for (uint32_t j = 0; j < state.LightCount; j++)
	{
		const CPULightInfo& light = Lights.data()[j]; // MSVC vector operator[] is very slow

		float dist = (light.Origin - origin).Length();
		if (dist > minDistance && dist < light.Radius)
		{
			Vec3 dir = Vec3::Normalize(light.Origin - origin);

			float distAttenuation = std::max(1.0f - (dist / light.Radius), 0.0f);
			float angleAttenuation = 1.0f;
			if (surface)
			{
				angleAttenuation = std::max(Vec3::Dot(normal, dir), 0.0f);
			}
			float spotAttenuation = 1.0f;
			if (light.OuterAngleCos > -1.0f)
			{
				float cosDir = Vec3::Dot(dir, light.SpotDir);
				spotAttenuation = smoothstep(light.OuterAngleCos, light.InnerAngleCos, cosDir);
				spotAttenuation = std::max(spotAttenuation, 0.0f);
			}

			float attenuation = distAttenuation * angleAttenuation * spotAttenuation;
			if (attenuation > 0.0f)
			{
				float shadowAttenuation = 0.0f;

				if (state.PassType == 0 && surface)
				{
					Vec3 e0 = Vec3::Cross(normal, std::abs(normal.x) < std::abs(normal.y) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f));
					Vec3 e1 = Vec3::Cross(normal, e0);
					e0 = Vec3::Cross(normal, e1);
					for (uint32_t i = 0; i < state.SampleCount; i++)
					{
						Vec2 offset = (Hammersley(i, state.SampleCount) - 0.5) * state.SampleDistance;
						Vec3 origin2 = origin + e0 * offset.x + e1 * offset.y;

						LevelTraceHit hit = Trace(origin2, light.Origin);
						if (hit.fraction == 1.0f)
							shadowAttenuation += 1.0f;
					}
					shadowAttenuation *= 1.0f / float(state.SampleCount);
				}
				else
				{
					LevelTraceHit hit = Trace(origin, light.Origin);
					shadowAttenuation = (hit.fraction == 1.0f) ? 1.0f : 0.0f;
				}

				attenuation *= shadowAttenuation;

				incoming += light.Color * (attenuation * light.Intensity * incomingAttenuation);
			}
		}
	}

	state.Output = incoming;
}

Vec3 CPURaytracer::ImportanceSample(const Vec3& HemisphereVec, Vec3 N)
{
	// from tangent-space vector to world-space sample vector
	Vec3 up = std::abs(N.x) < std::abs(N.y) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);
	Vec3 tangent = Vec3::Normalize(Vec3::Cross(up, N));
	Vec3 bitangent = Vec3::Cross(N, tangent);

	Vec3 sampleVec = tangent * HemisphereVec.x + bitangent * HemisphereVec.y + N * HemisphereVec.z;
	return Vec3::Normalize(sampleVec);
}

Vec2 CPURaytracer::Hammersley(uint32_t i, uint32_t N)
{
	return Vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

float CPURaytracer::RadicalInverse_VdC(uint32_t bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

void CPURaytracer::CreateHemisphereVectors()
{
	if (HemisphereVectors.empty())
	{
		HemisphereVectors.reserve(bounceSampleCount);
		for (int i = 0; i < bounceSampleCount; i++)
		{
			Vec2 Xi = Hammersley(i, bounceSampleCount);
			Vec3 H;
			H.x = Xi.x * 2.0f - 1.0f;
			H.y = Xi.y * 2.0f - 1.0f;
			H.z = RadicalInverse_VdC(i) + 0.01f;
			H.Normalize();
			HemisphereVectors.push_back(H);
		}
	}
}

void CPURaytracer::CreateLights()
{
	Lights.clear();
	for (ThingLight& light : mesh->map->ThingLights)
	{
		CPULightInfo info;
		info.Origin = light.LightOrigin();
		info.Radius = light.LightRadius();
		info.Intensity = light.intensity;
		info.InnerAngleCos = light.innerAngleCos;
		info.OuterAngleCos = light.outerAngleCos;
		info.SpotDir = light.SpotDir();
		info.Color = light.rgb;
		Lights.push_back(info);
	}
}

CPUEmissiveSurface CPURaytracer::GetEmissive(Surface* surface)
{
	CPUEmissiveSurface info;

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

	if (def)
	{
		info.Distance = def->distance + def->distance;
		info.Intensity = def->intensity;
		info.Color = def->rgb;
	}
	else
	{
		info.Distance = 0.0f;
		info.Intensity = 0.0f;
		info.Color = Vec3(0.0f, 0.0f, 0.0f);
	}

	return info;
}

LevelTraceHit CPURaytracer::Trace(const Vec3& startVec, const Vec3& endVec)
{
	TraceHit hit = TriangleMeshShape::find_first_hit(CollisionMesh.get(), startVec, endVec);

	LevelTraceHit trace;
	trace.start = startVec;
	trace.end = endVec;
	trace.fraction = hit.fraction;
	if (trace.fraction < 1.0f)
	{
		int elementIdx = hit.triangle * 3;
		trace.hitSurface = mesh->surfaces[mesh->MeshSurfaces[hit.triangle]].get();
		trace.indices[0] = mesh->MeshUVIndex[mesh->MeshElements[elementIdx]];
		trace.indices[1] = mesh->MeshUVIndex[mesh->MeshElements[elementIdx + 1]];
		trace.indices[2] = mesh->MeshUVIndex[mesh->MeshElements[elementIdx + 2]];
		trace.b = hit.b;
		trace.c = hit.c;
	}
	else
	{
		trace.hitSurface = nullptr;
		trace.indices[0] = 0;
		trace.indices[1] = 0;
		trace.indices[2] = 0;
		trace.b = 0.0f;
		trace.c = 0.0f;
	}
	return trace;
}

bool CPURaytracer::TraceAnyHit(const Vec3& startVec, const Vec3& endVec)
{
	return TriangleMeshShape::find_any_hit(CollisionMesh.get(), startVec, endVec);
}
