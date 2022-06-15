
#pragma once

#include <functional>
#include "collision.h"

class LevelMesh;

struct CPUTraceTask
{
	int id, x, y;
};

struct CPULightInfo
{
	vec3 Origin;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	vec3 SpotDir;
	vec3 Color;
};

struct CPUTraceState
{
	uint32_t SampleIndex;
	uint32_t SampleCount;
	uint32_t PassType;
	uint32_t LightCount;
	vec3 SunDir;
	vec3 SunColor;
	float SunIntensity;
	vec3 HemisphereVec;

	vec3 StartPosition;
	Surface* StartSurface;

	vec3 Position;
	Surface* Surf;

	vec3 Output;
	float OutputAttenuation;

	bool EndTrace;
};

struct CPUEmissiveSurface
{
	float Distance;
	float Intensity;
	vec3 Color;
};

struct LevelTraceHit
{
	vec3 start;
	vec3 end;
	float fraction;

	Surface* hitSurface;
	int indices[3];
	float b, c;
};

class CPURaytracer
{
public:
	CPURaytracer();
	~CPURaytracer();

	void Raytrace(LevelMesh* level);

private:
	void RaytraceTask(const CPUTraceTask& task);
	void RunBounceTrace(CPUTraceState& state);
	void RunLightTrace(CPUTraceState& state);

	CPUEmissiveSurface GetEmissive(Surface* surface);

	void CreateHemisphereVectors();
	void CreateLights();

	LevelTraceHit Trace(const vec3& startVec, const vec3& endVec);
	bool TraceAnyHit(const vec3& startVec, const vec3& endVec);

	static vec3 ImportanceSample(const vec3& HemisphereVec, vec3 N);

	static float RadicalInverse_VdC(uint32_t bits);
	static vec2 Hammersley(uint32_t i, uint32_t N);

	static void RunJob(int count, std::function<void(int i)> callback);

	const int coverageSampleCount = 256;
	const int bounceSampleCount = 2048;

	LevelMesh* mesh = nullptr;
	std::vector<vec3> HemisphereVectors;
	std::vector<CPULightInfo> Lights;

	std::unique_ptr<TriangleMeshShape> CollisionMesh;
};
