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
#include "lightsurface.h"

kexLightSurface::kexLightSurface(const surfaceLightDef &lightSurfaceDef, surface_t *surface)
{
	this->intensity = lightSurfaceDef.intensity;
	this->distance = lightSurfaceDef.distance;
	this->rgb = lightSurfaceDef.rgb;
	this->surface = surface;
}

kexLightSurface::~kexLightSurface()
{
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
bool kexLightSurface::SubdivideRecursion(vertexBatch_t &surfPoints, float divide, std::vector<std::unique_ptr<vertexBatch_t>> &points)
{
	kexBBox bounds;
	kexVec3 splitNormal;
	float dist;

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

			auto frontPoints = std::make_unique<vertexBatch_t>();
			auto backPoints = std::make_unique<vertexBatch_t>();

			// start clipping
			Clip(surfPoints, splitNormal, dist, frontPoints.get(), backPoints.get());

			if (!SubdivideRecursion(*frontPoints, divide, points))
			{
				points.push_back(std::move(frontPoints));
			}

			if (!SubdivideRecursion(*backPoints, divide, points))
			{
				points.push_back(std::move(backPoints));
			}

			return true;
		}
	}

	return false;
}

void kexLightSurface::Subdivide(const float divide)
{
	if (surface->type == ST_CEILING || surface->type == ST_FLOOR)
	{
		std::vector<std::unique_ptr<vertexBatch_t>> points;
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
			vertexBatch_t *vb = points[i].get();
			kexVec3 center;

			for (unsigned int j = 0; j < vb->size(); ++j)
			{
				center += (*vb)[j];
			}

			origins.push_back(center / (float)vb->size());
		}
	}
	else
	{
		float length = surface->verts[0].Distance(surface->verts[1]);
		if (length < divide)
		{
			kexVec3 top = surface->verts[0] * 0.5f + surface->verts[1] * 0.5f;
			kexVec3 bottom = surface->verts[2] * 0.5f + surface->verts[3] * 0.5f;
			origins.push_back(top * 0.5f + bottom * 0.5f);
		}
		else
		{
			float frac = length / divide;
			frac = frac - std::floor(frac);

			float start = divide * 0.5f + frac * 0.5f * divide;
			for (float i = start; i < length; i += divide)
			{
				float t = i / length;

				kexVec3 top = surface->verts[0] * (1.0f - t) + surface->verts[1] * t;
				kexVec3 bottom = surface->verts[2] * (1.0f - t) + surface->verts[3] * t;

				float length2 = top.Distance(bottom);
				if (length2 < divide)
				{
					origins.push_back(top * 0.5f + bottom * 0.5f);
				}
				else
				{
					float frac2 = length2 / divide;
					frac2 = frac2 - std::floor(frac2);

					float start2 = divide * 0.5f + frac2 * 0.5f * divide;
					for (float j = start2; j < length2; j += divide)
					{
						float t2 = j / length2;
						origins.push_back(top * (1.0f - t2) + bottom * t2);
					}
				}
			}
		}
	}
}

float kexLightSurface::TraceSurface(FLevel *map, const surface_t *fragmentSurface, const kexVec3 &fragmentPos)
{
	if (fragmentSurface == surface)
		return 1.0f; // light surface will always be fullbright

	kexVec3 lightSurfaceNormal = surface->plane.Normal();
	kexVec3 fragmentNormal = fragmentSurface ? fragmentSurface->plane.Normal() : kexVec3(0.0f, 0.0f, 0.0f);

	float gzdoomRadiusScale = 2.0f; // 2.0 because gzdoom's dynlights do this and we want them to match

	float total = 0.0f;
	int count = 0;
	float closestDistance = distance * gzdoomRadiusScale;
	float maxDistanceSqr = closestDistance * closestDistance;
	for (size_t i = 0; i < origins.size(); ++i)
	{
		kexVec3 lightPos = origins[i];
		kexVec3 lightDir = (lightPos - fragmentPos);

		float dsqr = kexVec3::Dot(lightDir, lightDir);
		if (dsqr > maxDistanceSqr)
			continue; // out of range

		count++;

		float attenuation = fragmentSurface ? kexVec3::Dot(lightDir, fragmentNormal) : 1.0f;
		if (attenuation <= 0.0f)
			continue; // not even facing the light surface

		float d = std::sqrt(dsqr);
		attenuation /= d;

		if (surface->type != ST_CEILING && surface->type != ST_FLOOR)
		{
			if (fragmentPos.z >= surface->verts[0].z && fragmentPos.z <= surface->verts[2].z)
			{
				// since walls are always vertically straight, we can cheat a little by adjusting
				// the sampling point height. this also allows us to do accurate light emitting
				// while just using one sample point
				lightPos.z = fragmentPos.z;
			}
		}

		// trace the origin to the center of the light surface. nudge by the normals in
		// case the start/end points are directly on or inside the surface
		if (map->TraceAnyHit(lightPos + lightSurfaceNormal, fragmentPos + fragmentNormal))
			continue; // something is obstructing it

		if (d < closestDistance)
			closestDistance = d;
		total += attenuation;
	}

	if (count == 0)
		return 0.0f;

	float attenuation = 1.0f - closestDistance / (distance * gzdoomRadiusScale);
	return attenuation * total / count;
}
