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

struct FLevel;
struct surfaceLightDef;

class LightSurface
{
public:
	LightSurface(const surfaceLightDef &lightSurfaceDef, Surface *surface);
	~LightSurface();

	void Subdivide(const float divide);
	float TraceSurface(LevelMesh *map, const Surface *surface, const Vec3 &origin);

	const float Distance() const { return distance; }
	const float Intensity() const { return intensity; }
	const Vec3 GetRGB() const { return rgb; }
	const Surface *GetSurface() const { return surface; }

private:
	typedef std::vector<Vec3> VertexBatch;

	bool SubdivideRecursion(VertexBatch &surfPoints, float divide, std::vector<std::unique_ptr<VertexBatch>> &points);
	void Clip(VertexBatch &points, const Vec3 &normal, float dist, VertexBatch *frontPoints, VertexBatch *backPoints);

	float distance;
	float intensity;
	Vec3 rgb;
	VertexBatch origins;
	Surface *surface;
};
