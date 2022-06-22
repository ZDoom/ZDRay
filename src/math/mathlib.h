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

#include <math.h>
#include <cstdint>
#include <cstddef>
#include "vec.h"
#include "mat.h"

#ifdef M_PI
#undef M_PI
#endif

#define M_PI 3.1415926535897932384626433832795f
#define M_RAD (M_PI / 180.0f)
#define M_DEG (180.0f / M_PI)
#define M_INFINITY 1e30f

#define DEG2RAD(x) ((x) * M_RAD)
#define RAD2DEG(x) ((x) * M_DEG)

#define FLOATSIGNBIT(f) (reinterpret_cast<const unsigned int&>(f) >> 31)

class Angle;

class Math
{
public:
	static float Sin(float x);
	static float Cos(float x);
	static float Tan(float x);
	static float ATan2(float x, float y);
	static float ACos(float x);
	static float Sqrt(float x);
	static float Pow(float x, float y);
	static float Log(float x);
	static float Floor(float x);
	static float Ceil(float x);
	static float Deg2Rad(float x);
	static float Rad2Deg(float x);

	static int Abs(int x);
	static float Fabs(float x);
	static int RoundPowerOfTwo(int x);
	static float InvSqrt(float x);
	static void Clamp(float &f, const float min, const float max);
	static void Clamp(int &i, const int min, const int max);
	static void Clamp(uint8_t &b, const uint8_t min, const uint8_t max);
	static void Clamp(vec3 &f, const float min, const float max);

	static void CubicCurve(const vec3 &start, const vec3 &end, const float time, const vec3 &point, vec3 *vec);
	static void QuadraticCurve(const vec3 &start, const vec3 &end, const float time, const vec3 &pt1, const vec3 &pt2, vec3 *vec);
};

class Plane
{
public:
	Plane();
	Plane(const float a, const float b, const float c, const float d);
	Plane(const vec3 &pt1, const vec3 &pt2, const vec3 &pt3);
	Plane(const vec3 &normal, const vec3 &point);
	Plane(const Plane &plane);

	enum PlaneAxis
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	void Set(float a, float b, float c, float d);
	const vec3 &Normal() const;
	vec3 &Normal();
	Plane &SetNormal(const vec3 &normal);
	Plane& SetNormal(const vec3& pt1, const vec3& pt2, const vec3& pt3);
	Plane& SetNormal(const vec3& v1, const vec3& v2, const vec3& v3, const vec3& v4);
	float Distance(const vec3 &point);
	Plane &SetDistance(const vec3 &point);
	bool IsFacing(const float yaw);
	float ToYaw();
	float ToPitch();
	const vec4 &ToVec4() const;
	vec4 &ToVec4();
	const PlaneAxis BestAxis() const;
	vec3 GetInclination();

	static Plane Inverse(const Plane &p) { return Plane(-p.a, -p.b, -p.c, -p.d); }

	float zAt(float x, float y) const { return (d - a * x - b * y) / c; }

	float a;
	float b;
	float c;
	float d;
};

class Angle
{
public:
	Angle();
	Angle(const float yaw, const float pitch, const float roll);
	Angle(const vec3 &vector);
	Angle(const Angle &an);

	Angle &Round();
	Angle &Clamp180();
	Angle &Clamp180Invert();
	Angle &Clamp180InvertSum(const Angle &angle);
	Angle Diff(Angle &angle);
	void ToAxis(vec3 *forward, vec3 *up, vec3 *right);
	vec3 ToForwardAxis();
	vec3 ToUpAxis();
	vec3 ToRightAxis();
	const vec3 &ToVec3() const;
	vec3 &ToVec3();

	Angle operator+(const Angle &angle);
	Angle operator-(const Angle &angle);
	Angle &operator+=(const Angle &angle);
	Angle &operator-=(const Angle &angle);
	Angle &operator=(const Angle &angle);
	Angle &operator=(const vec3 &vector);
	Angle &operator=(const float *vecs);
	Angle operator-();
	float operator[](int index) const;
	float &operator[](int index);

	float yaw;
	float pitch;
	float roll;
};

class BBox
{
public:
	BBox();
	BBox(const vec3 &vMin, const vec3 &vMax);

	void Clear();
	vec3 Center() const;
	vec3 Extents() const;
	float Radius() const;
	void AddPoint(const vec3 &vec);
	bool PointInside(const vec3 &vec) const;
	bool IntersectingBox(const BBox &box) const;
	bool IntersectingBox2D(const BBox &box) const;
	float DistanceToPlane(Plane &plane);
	bool LineIntersect(const vec3 &start, const vec3 &end);
	void ToPoints(float *points) const;
	void ToVectors(vec3 *vectors) const;

	BBox operator+(const float radius) const;
	BBox &operator+=(const float radius);
	BBox operator+(const vec3 &vec) const;
	BBox operator-(const float radius) const;
	BBox operator-(const vec3 &vec) const;
	BBox &operator-=(const float radius);
	BBox operator*(const vec3 &vec) const;
	BBox &operator*=(const vec3 &vec);
	BBox &operator=(const BBox &bbox);
	vec3 operator[](int index) const;
	vec3 &operator[](int index);

	vec3 min;
	vec3 max;
};

/////////////////////////////////////////////////////////////////////////////

inline float Math::Sin(float x)
{
	return sinf(x);
}

inline float Math::Cos(float x)
{
	return cosf(x);
}

inline float Math::Tan(float x)
{
	return tanf(x);
}

inline float Math::ATan2(float x, float y)
{
	return atan2f(x, y);
}

inline float Math::ACos(float x)
{
	return acosf(x);
}

inline float Math::Sqrt(float x)
{
	return x * InvSqrt(x);
}

inline float Math::Pow(float x, float y)
{
	return powf(x, y);
}

inline float Math::Log(float x)
{
	return logf(x);
}

inline float Math::Floor(float x)
{
	return floorf(x);
}

inline float Math::Ceil(float x)
{
	return ceilf(x);
}

inline float Math::Deg2Rad(float x)
{
	return DEG2RAD(x);
}

inline float Math::Rad2Deg(float x)
{
	return RAD2DEG(x);
}

inline int Math::Abs(int x)
{
	int y = x >> 31;
	return ((x ^ y) - y);
}

inline float Math::Fabs(float x)
{
	int tmp = *reinterpret_cast<int*>(&x);
	tmp &= 0x7FFFFFFF;
	return *reinterpret_cast<float*>(&tmp);
}

inline float Math::InvSqrt(float x)
{
	unsigned int i;
	float r;
	float y;

	y = x * 0.5f;
	i = *reinterpret_cast<unsigned int*>(&x);
	i = 0x5f3759df - (i >> 1);
	r = *reinterpret_cast<float*>(&i);
	r = r * (1.5f - r * r * y);

	return r;
}

inline void Math::Clamp(float &f, const float min, const float max)
{
	if (f < min) { f = min; }
	if (f > max) { f = max; }
}

inline void Math::Clamp(uint8_t &b, const uint8_t min, const uint8_t max)
{
	if (b < min) { b = min; }
	if (b > max) { b = max; }
}

inline void Math::Clamp(int &i, const int min, const int max)
{
	if (i < min) { i = min; }
	if (i > max) { i = max; }
}

inline void Math::Clamp(vec3 &v, const float min, const float max)
{
	if (v.x < min) { v.x = min; }
	if (v.x > max) { v.x = max; }
	if (v.y < min) { v.y = min; }
	if (v.y > max) { v.y = max; }
	if (v.z < min) { v.z = min; }
	if (v.z > max) { v.z = max; }
}

/////////////////////////////////////////////////////////////////////////////

inline vec3 BBox::Center() const
{
	return vec3(
		(max.x + min.x) * 0.5f,
		(max.y + min.y) * 0.5f,
		(max.z + min.z) * 0.5f);
}

inline vec3 BBox::Extents() const
{
	return vec3(
		(max.x - min.x) * 0.5f,
		(max.y - min.y) * 0.5f,
		(max.z - min.z) * 0.5f);
}
