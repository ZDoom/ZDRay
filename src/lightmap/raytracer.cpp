
#include "math/mathlib.h"
#include "surfaces.h"
#include "level/level.h"
#include "raytracer.h"
#include "surfacelight.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>

extern int Multisample;
extern int LightBounce;
extern float GridSize;

Raytracer::Raytracer()
{
}

Raytracer::~Raytracer()
{
}

void Raytracer::Raytrace(LevelMesh* level)
{
	mesh = level;
}
