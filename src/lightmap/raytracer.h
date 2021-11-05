
#pragma once

class LevelMesh;

class Raytracer
{
public:
	Raytracer();
	~Raytracer();

	void Raytrace(LevelMesh* level);

private:
	void RaytraceProbeSample(LightProbeSample* probe);
	void RaytraceSurfaceSample(Surface* surface, int x, int y);
	Vec3 TracePath(const Vec3& pos, const Vec3& dir, int sampleIndex, int depth = 0);

	Vec3 GetLightEmittance(Surface* surface, const Vec3& pos);
	Vec3 GetSurfaceEmittance(Surface* surface, float distance);

	static float RadicalInverse_VdC(uint32_t bits);
	static Vec2 Hammersley(uint32_t i, uint32_t N);
	static Vec3 ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness);

	int SAMPLE_COUNT = 1024;

	LevelMesh* mesh = nullptr;
};
