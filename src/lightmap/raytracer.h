
#pragma once

class LevelMesh;

class Raytracer
{
public:
	Raytracer();
	~Raytracer();

	void Raytrace(LevelMesh* level);

private:
	LevelMesh* mesh = nullptr;
};
