
#pragma once

class LevelMesh;

struct CPUTraceTask
{
	int id, x, y;
};

struct CPULightInfo
{
	Vec3 Origin;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	Vec3 SpotDir;
	Vec3 Color;
};

struct CPUTraceState
{
	uint32_t SampleIndex;
	uint32_t SampleCount;
	uint32_t PassType;
	uint32_t LightCount;
	Vec3 SunDir;
	float SampleDistance;
	Vec3 SunColor;
	float SunIntensity;
	Vec3 HemisphereVec;

	Vec3 StartPosition;
	Surface* StartSurface;

	Vec3 Position;
	Surface* Surf;

	Vec3 Output;
	float OutputAttenuation;

	bool EndTrace;
};

struct CPUEmissiveSurface
{
	float Distance;
	float Intensity;
	Vec3 Color;
};

struct LevelTraceHit
{
	Vec3 start;
	Vec3 end;
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

	LevelTraceHit Trace(const Vec3& startVec, const Vec3& endVec);
	bool TraceAnyHit(const Vec3& startVec, const Vec3& endVec);

	static Vec3 ImportanceSample(const Vec3& HemisphereVec, Vec3 N);

	static float RadicalInverse_VdC(uint32_t bits);
	static Vec2 Hammersley(uint32_t i, uint32_t N);

	const int coverageSampleCount = 256;
	const int bounceSampleCount = 2048;

	LevelMesh* mesh = nullptr;
	std::vector<Vec3> HemisphereVectors;
	std::vector<CPULightInfo> Lights;

	std::unique_ptr<TriangleMeshShape> CollisionMesh;
};
