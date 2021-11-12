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

#include <math.h>
#include "mathlib.h"
#include <assert.h>

BBox::BBox()
{
	Clear();
}

BBox::BBox(const vec3 &vMin, const vec3 &vMax)
{
	this->min = vMin;
	this->max = vMax;
}

void BBox::Clear()
{
	min = vec3(M_INFINITY, M_INFINITY, M_INFINITY);
	max = vec3(-M_INFINITY, -M_INFINITY, -M_INFINITY);
}

void BBox::AddPoint(const vec3 &vec)
{
	float lowx = min.x;
	float lowy = min.y;
	float lowz = min.z;
	float hix = max.x;
	float hiy = max.y;
	float hiz = max.z;

	if (vec.x < lowx) { lowx = vec.x; }
	if (vec.y < lowy) { lowy = vec.y; }
	if (vec.z < lowz) { lowz = vec.z; }
	if (vec.x > hix) { hix = vec.x; }
	if (vec.y > hiy) { hiy = vec.y; }
	if (vec.z > hiz) { hiz = vec.z; }

	min = vec3(lowx, lowy, lowz);
	max = vec3(hix, hiy, hiz);
}

float BBox::Radius() const
{
	int i;
	float r = 0;
	float r1;
	float r2;

	for (i = 0; i < 3; i++)
	{
		r1 = Math::Fabs(min[i]);
		r2 = Math::Fabs(max[i]);

		if (r1 > r2)
		{
			r += r1 * r1;
		}
		else
		{
			r += r2 * r2;
		}
	}

	return Math::Sqrt(r);
}

bool BBox::PointInside(const vec3 &vec) const
{
	return !(vec[0] < min[0] || vec[1] < min[1] || vec[2] < min[2] ||
		vec[0] > max[0] || vec[1] > max[1] || vec[2] > max[2]);
}

bool BBox::IntersectingBox(const BBox &box) const
{
	return !(box.max[0] < min[0] || box.max[1] < min[1] || box.max[2] < min[2] ||
		box.min[0] > max[0] || box.min[1] > max[1] || box.min[2] > max[2]);
}

bool BBox::IntersectingBox2D(const BBox &box) const
{
	return !(box.max[0] < min[0] || box.max[2] < min[2] ||
		box.min[0] > max[0] || box.min[2] > max[2]);
}

float BBox::DistanceToPlane(Plane &plane)
{
	vec3 c;
	float distStart;
	float distEnd;
	float dist = 0;

	c = Center();

	distStart = plane.Distance(c);
	distEnd = Math::Fabs((max.x - c.x) * plane.a) +
		Math::Fabs((max.y - c.y) * plane.b) +
		Math::Fabs((max.z - c.z) * plane.c);

	dist = distStart - distEnd;

	if (dist > 0)
	{
		// in front
		return dist;
	}

	dist = distStart + distEnd;

	if (dist < 0)
	{
		// behind
		return dist;
	}

	return 0;
}

BBox BBox::operator+(const float radius) const
{
	vec3 vmin = min;
	vec3 vmax = max;

	vmin.x -= radius;
	vmin.y -= radius;
	vmin.z -= radius;

	vmax.x += radius;
	vmax.y += radius;
	vmax.z += radius;

	return BBox(vmin, vmax);
}

BBox &BBox::operator+=(const float radius)
{
	min.x -= radius;
	min.y -= radius;
	min.z -= radius;
	max.x += radius;
	max.y += radius;
	max.z += radius;
	return *this;
}

BBox BBox::operator+(const vec3 &vec) const
{
	vec3 vmin = min;
	vec3 vmax = max;

	vmin.x += vec.x;
	vmin.y += vec.y;
	vmin.z += vec.z;

	vmax.x += vec.x;
	vmax.y += vec.y;
	vmax.z += vec.z;

	return BBox(vmin, vmax);
}

BBox BBox::operator-(const float radius) const
{
	vec3 vmin = min;
	vec3 vmax = max;

	vmin.x += radius;
	vmin.y += radius;
	vmin.z += radius;

	vmax.x -= radius;
	vmax.y -= radius;
	vmax.z -= radius;

	return BBox(vmin, vmax);
}

BBox BBox::operator-(const vec3 &vec) const
{
	vec3 vmin = min;
	vec3 vmax = max;

	vmin.x -= vec.x;
	vmin.y -= vec.y;
	vmin.z -= vec.z;

	vmax.x -= vec.x;
	vmax.y -= vec.y;
	vmax.z -= vec.z;

	return BBox(vmin, vmax);
}

BBox &BBox::operator-=(const float radius)
{
	min.x += radius;
	min.y += radius;
	min.z += radius;
	max.x -= radius;
	max.y -= radius;
	max.z -= radius;
	return *this;
}

BBox BBox::operator*(const vec3 &vec) const
{
	BBox box = *this;

	if (vec.x < 0) { box.min.x += (vec.x - 1); }
	else { box.max.x += (vec.x + 1); }
	if (vec.y < 0) { box.min.y += (vec.y - 1); }
	else { box.max.y += (vec.y + 1); }
	if (vec.z < 0) { box.min.z += (vec.z - 1); }
	else { box.max.z += (vec.z + 1); }

	return box;
}

BBox &BBox::operator*=(const vec3 &vec)
{
	if (vec.x < 0) { min.x += (vec.x - 1); }
	else { max.x += (vec.x + 1); }
	if (vec.y < 0) { min.y += (vec.y - 1); }
	else { max.y += (vec.y + 1); }
	if (vec.z < 0) { min.z += (vec.z - 1); }
	else { max.z += (vec.z + 1); }

	return *this;
}

BBox &BBox::operator=(const BBox &bbox)
{
	min = bbox.min;
	max = bbox.max;

	return *this;
}

vec3 BBox::operator[](int index) const
{
	assert(index >= 0 && index < 2);
	return index == 0 ? min : max;
}

vec3 &BBox::operator[](int index)
{
	assert(index >= 0 && index < 2);
	return index == 0 ? min : max;
}

bool BBox::LineIntersect(const vec3 &start, const vec3 &end)
{
	float ld[3];
	vec3 center = Center();
	vec3 extents = max - center;
	vec3 lineDir = (end - start) * 0.5f;
	vec3 lineCenter = lineDir + start;
	vec3 dir = lineCenter - center;

	ld[0] = Math::Fabs(lineDir.x);
	if (Math::Fabs(dir.x) > extents.x + ld[0]) { return false; }
	ld[1] = Math::Fabs(lineDir.y);
	if (Math::Fabs(dir.y) > extents.y + ld[1]) { return false; }
	ld[2] = Math::Fabs(lineDir.z);
	if (Math::Fabs(dir.z) > extents.z + ld[2]) { return false; }

	vec3 crossprod = cross(lineDir, dir);

	if (Math::Fabs(crossprod.x) > extents.y * ld[2] + extents.z * ld[1]) { return false; }
	if (Math::Fabs(crossprod.y) > extents.x * ld[2] + extents.z * ld[0]) { return false; }
	if (Math::Fabs(crossprod.z) > extents.x * ld[1] + extents.y * ld[0]) { return false; }

	return true;
}

// Assumes points is an array of 24
void BBox::ToPoints(float *points) const
{
	points[0 * 3 + 0] = max[0];
	points[0 * 3 + 1] = min[1];
	points[0 * 3 + 2] = min[2];
	points[1 * 3 + 0] = max[0];
	points[1 * 3 + 1] = min[1];
	points[1 * 3 + 2] = max[2];
	points[2 * 3 + 0] = min[0];
	points[2 * 3 + 1] = min[1];
	points[2 * 3 + 2] = max[2];
	points[3 * 3 + 0] = min[0];
	points[3 * 3 + 1] = min[1];
	points[3 * 3 + 2] = min[2];
	points[4 * 3 + 0] = max[0];
	points[4 * 3 + 1] = max[1];
	points[4 * 3 + 2] = min[2];
	points[5 * 3 + 0] = max[0];
	points[5 * 3 + 1] = max[1];
	points[5 * 3 + 2] = max[2];
	points[6 * 3 + 0] = min[0];
	points[6 * 3 + 1] = max[1];
	points[6 * 3 + 2] = max[2];
	points[7 * 3 + 0] = min[0];
	points[7 * 3 + 1] = max[1];
	points[7 * 3 + 2] = min[2];
}

// Assumes vectors is an array of 8
void BBox::ToVectors(vec3 *vectors) const
{
	vectors[0][0] = max[0];
	vectors[0][1] = min[1];
	vectors[0][2] = min[2];
	vectors[1][0] = max[0];
	vectors[1][1] = min[1];
	vectors[1][2] = max[2];
	vectors[2][0] = min[0];
	vectors[2][1] = min[1];
	vectors[2][2] = max[2];
	vectors[3][0] = min[0];
	vectors[3][1] = min[1];
	vectors[3][2] = min[2];
	vectors[4][0] = max[0];
	vectors[4][1] = max[1];
	vectors[4][2] = min[2];
	vectors[5][0] = max[0];
	vectors[5][1] = max[1];
	vectors[5][2] = max[2];
	vectors[6][0] = min[0];
	vectors[6][1] = max[1];
	vectors[6][2] = max[2];
	vectors[7][0] = min[0];
	vectors[7][1] = max[1];
	vectors[7][2] = min[2];
}
