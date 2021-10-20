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

#pragma once

#include "surfaces.h"
#include "framework/tarray.h"
#include <mutex>

class FWadWriter;
class SurfaceLight;

class TraceTask
{
public:
	TraceTask() { }
	TraceTask(int surface, int offset) : surface(surface), offset(offset) { }

	int surface = 0;
	int offset = 0;

	static const int tasksize = 64;
};

class DLightRaytracer
{
public:
	DLightRaytracer();
	~DLightRaytracer();

	void Raytrace(LevelMesh* level);

private:
	Vec3 LightTexelSample(const Vec3 &origin, Surface *surface);
	bool EmitFromCeiling(const Surface *surface, const Vec3 &origin, const Vec3 &normal, Vec3 &color);

	void TraceSurface(Surface *surface, int offset);
	void TraceIndirectLight(Surface *surface, int offset);
	void LightProbe(int probeid);
	void CreateTraceTasks();
	void LightSurface(const int taskid);
	void LightIndirect(const int taskid);

	void CreateSurfaceLights();

	void SetupTaskProcessed(const char *name, int total);
	void PrintTaskProcessed();

	LevelMesh* mesh = nullptr;
	std::vector<std::unique_ptr<SurfaceLight>> surfaceLights;
	std::vector<TraceTask> traceTasks;
	int tracedTexels = 0;

	std::mutex mutex;
	int processed = 0;
	int progresstotal = 0;
};
