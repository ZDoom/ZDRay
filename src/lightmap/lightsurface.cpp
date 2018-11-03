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
//-----------------------------------------------------------------------------
//
// DESCRIPTION: Special surfaces that contains origin points used for
//              emitting light. Surfaces can be subdivided for more
//              accurate light casting
//
//-----------------------------------------------------------------------------

#include "math/mathlib.h"
#include "level/level.h"
#include "trace.h"
#include "lightsurface.h"

kexLightSurface::kexLightSurface()
{
}

kexLightSurface::~kexLightSurface()
{
}

void kexLightSurface::Init(const surfaceLightDef &lightSurfaceDef, surface_t *surface, const bool bWall)
{
	this->intensity = lightSurfaceDef.intensity;
	this->distance = lightSurfaceDef.distance;
	this->rgb = lightSurfaceDef.rgb;
	this->surface = surface;
	this->bWall = bWall;
}

// Creates a single origin point if we're not intending on subdividing this light surface
void kexLightSurface::CreateCenterOrigin()
{
	if (!bWall)
	{
		kexVec3 center;

		for (int i = 0; i < surface->numVerts; ++i)
		{
			center += surface->verts[i];
		}

		origins.push_back(center / (float)surface->numVerts);
	}
	else
	{
		origins.push_back(kexVec3((surface->verts[1].x + surface->verts[0].x) * 0.5f,
			(surface->verts[1].y + surface->verts[0].y) * 0.5f,
			(surface->verts[2].z + surface->verts[0].z) * 0.5f));
	}
}

// Splits surface vertices into two groups while adding new ones caused by the split
void kexLightSurface::Clip(vertexBatch_t &points, const kexVec3 &normal, float dist, vertexBatch_t *frontPoints, vertexBatch_t *backPoints)
{
	std::vector<float> dists;
	std::vector<char> sides;

	// determines what sides the vertices lies on
	for (size_t i = 0; i < points.size(); ++i)
	{
		float d = points[i].Dot(normal) - dist;

		dists.push_back(d);

		if (d > 0.1f)
		{
			sides.push_back(1);      // front
		}
		else if (d < -0.1f)
		{
			sides.push_back(-1);     // directly on the split plane
		}
		else
		{
			sides.push_back(0);      // back
		}
	}

	// add points
	for (unsigned int i = 0; i < points.size(); ++i)
	{
		int next;
		float frac;
		kexVec3 pt1, pt2, pt3;

		switch (sides[i])
		{
		case -1:
			backPoints->push_back(points[i]);
			break;

		case 1:
			frontPoints->push_back(points[i]);
			break;

		default:
			frontPoints->push_back(points[i]);
			backPoints->push_back(points[i]);

			// point is on the split plane so no new split vertex is needed
			continue;

		}

		// check if the edge crosses the split plane
		next = (i + 1) % points.size();

		if (sides[next] == 0 || sides[next] == sides[i])
		{
			// didn't cross
			continue;
		}

		pt1 = points[i];
		pt2 = points[next];

		// insert a new point caused by the split
		frac = dists[i] / (dists[i] - dists[next]);
		pt3 = pt1.Lerp(pt2, frac);

		frontPoints->push_back(pt3);
		backPoints->push_back(pt3);
	}
}

// Recursively divides the surface
bool kexLightSurface::SubdivideRecursion(vertexBatch_t &surfPoints, float divide, std::vector<vertexBatch_t*> &points)
{
	kexBBox bounds;
	kexVec3 splitNormal;
	float dist;
	vertexBatch_t *frontPoints;
	vertexBatch_t *backPoints;

	// get bounds from current set of points
	for (unsigned int i = 0; i < surfPoints.size(); ++i)
	{
		bounds.AddPoint(surfPoints[i]);
	}

	for (int i = 0; i < 3; ++i)
	{
		// check if its large enough to be divided
		if ((bounds.max[i] - bounds.min[i]) > divide)
		{
			splitNormal.Clear();
			splitNormal[i] = 1;

			dist = (bounds.max[i] + bounds.min[i]) * 0.5f;

			frontPoints = new vertexBatch_t;
			backPoints = new vertexBatch_t;

			// start clipping
			Clip(surfPoints, splitNormal, dist, frontPoints, backPoints);

			if (!SubdivideRecursion(*frontPoints, divide, points))
			{
				points.push_back(frontPoints);
			}
			else
			{
				delete frontPoints;
			}

			if (!SubdivideRecursion(*backPoints, divide, points))
			{
				points.push_back(backPoints);
			}
			else
			{
				delete backPoints;
			}

			return true;
		}
	}

	return false;
}

void kexLightSurface::Subdivide(const float divide)
{
	std::vector<vertexBatch_t*> points;
	vertexBatch_t surfPoints;

	for (int i = 0; i < surface->numVerts; ++i)
	{
		surfPoints.push_back(surface->verts[i]);
	}

	SubdivideRecursion(surfPoints, divide, points);

	// from each group of vertices caused by the split, begin
	// creating a origin point based on the center of that group
	for (size_t i = 0; i < points.size(); ++i)
	{
		vertexBatch_t *vb = points[i];
		kexVec3 center;

		for (unsigned int j = 0; j < vb->size(); ++j)
		{
			center += (*vb)[j];
		}

		origins.push_back(center / (float)vb->size());
	}

	for (size_t i = 0; i < points.size(); ++i)
	{
		vertexBatch_t *vb = points[i];

		delete vb;
	}
}

float kexLightSurface::TraceSurface(FLevel *doomMap, kexTrace &trace, const surface_t *surf, const kexVec3 &origin)
{
	// light surface will always be fullbright
	if (surf == surface)
	{
		return 1.0f;
	}

	kexVec3 lnormal = surface->plane.Normal();

	kexVec3 normal;
	if (surf)
	{
		normal = surf->plane.Normal();

		if (normal.Dot(lnormal) > 0)
		{
			// not facing the light surface
			return 0.0f;
		}
	}
	else
	{
		normal = kexVec3::vecUp;
	}

	float gzdoomRadiusScale = 2.0f; // 2.0 because gzdoom's dynlights do this and we want them to match

	float total = 0.0f;
	float closestDistance = distance * gzdoomRadiusScale;
	for (size_t i = 0; i < origins.size(); ++i)
	{
		kexVec3 center = origins[i];

		if (!bWall && origin.z > center.z)
		{
			// origin is not going to seen or traced by the light surface
			// so don't even bother. this also fixes some bizzare light
			// bleeding issues
			continue;
		}

		kexVec3 dir = (origin - center).Normalize();
		float attenuation = dir.Dot(lnormal);

		if (attenuation <= 0.0f)
			continue; // not even facing the light surface

		if (bWall)
		{
			if (origin.z >= surface->verts[0].z && origin.z <= surface->verts[2].z)
			{
				// since walls are always vertically straight, we can cheat a little by adjusting
				// the sampling point height. this also allows us to do accurate light emitting
				// while just using one sample point
				center.z = origin.z;
			}
		}

		// trace the origin to the center of the light surface. nudge by the normals in
		// case the start/end points are directly on or inside the surface
		trace.Trace(center + lnormal, origin + normal);

		if (trace.fraction < 1.0f)
		{
			// something is obstructing it
			continue;
		}

		float d = origin.Distance(center);
		if (d < closestDistance)
			closestDistance = d;
		total += attenuation;
	}

	float attenuation = 1.0f - closestDistance / (distance * gzdoomRadiusScale);
	return attenuation * total / origins.size();
}
