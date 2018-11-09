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

#ifdef M_PI
#undef M_PI
#endif

#define M_PI        3.1415926535897932384626433832795f
#define M_RAD       (M_PI / 180.0f)
#define M_DEG       (180.0f / M_PI)
#define M_INFINITY  1e30f

#define DEG2RAD(x) ((x) * M_RAD)
#define RAD2DEG(x) ((x) * M_DEG)

#define FLOATSIGNBIT(f)  (reinterpret_cast<const unsigned int&>(f) >> 31)

class Vec2;
class Vec3;
class Vec4;
class Mat4;
class Angle;

class Math
{
public:
	static float            Sin(float x);
	static float            Cos(float x);
	static float            Tan(float x);
	static float            ATan2(float x, float y);
	static float            ACos(float x);
	static float            Sqrt(float x);
	static float            Pow(float x, float y);
	static float            Log(float x);
	static float            Floor(float x);
	static float            Ceil(float x);
	static float            Deg2Rad(float x);
	static float            Rad2Deg(float x);

	static int              Abs(int x);
	static float            Fabs(float x);
	static int              RoundPowerOfTwo(int x);
	static float            InvSqrt(float x);
	static void             Clamp(float &f, const float min, const float max);
	static void             Clamp(int &i, const int min, const int max);
	static void             Clamp(uint8_t &b, const uint8_t min, const uint8_t max);
	static void             Clamp(Vec3 &f, const float min, const float max);

	static void             CubicCurve(const Vec3 &start, const Vec3 &end, const float time,
		const Vec3 &point, Vec3 *vec);
	static void             QuadraticCurve(const Vec3 &start, const Vec3 &end, const float time,
		const Vec3 &pt1, const Vec3 &pt2, Vec3 *vec);
};

class kexRand
{
public:
	static void             SetSeed(const int randSeed);
	static int              SysRand();
	static int              Int();
	static int              Max(const int max);
	static float            Float();
	static float            CFloat();

private:
	static int              seed;
};

class Quat
{
public:
	Quat();

	Quat(const float angle, const float x, const float y, const float z);
	Quat(const float angle, Vec3 &vector);
	Quat(const float angle, const Vec3 &vector);

	void                    Set(const float x, const float y, const float z, const float w);
	void                    Clear();
	float                   Dot(const Quat &quat) const;
	float                   UnitSq() const;
	float                   Unit() const;
	Quat                 &Normalize();
	Quat                 Slerp(const Quat &quat, float movement) const;
	Quat                 RotateFrom(const Vec3 &location, const Vec3 &target, float maxAngle);
	Quat                 Inverse() const;

	Quat                 operator+(const Quat &quat);
	Quat                 &operator+=(const Quat &quat);
	Quat                 operator-(const Quat &quat);
	Quat                 &operator-=(const Quat &quat);
	Quat                 operator*(const Quat &quat);
	Quat                 operator*(const float val) const;
	Quat                 &operator*=(const Quat &quat);
	Quat                 &operator*=(const float val);
	Quat                 &operator=(const Quat &quat);
	Quat                 &operator=(const Vec4 &vec);
	Quat                 &operator=(const float *vecs);
	Vec3                 operator|(const Vec3 &vector);

	const Vec3           &ToVec3() const;
	Vec3                 &ToVec3();

	float                   x;
	float                   y;
	float                   z;
	float                   w;
};

class Vec2
{
public:
	Vec2();
	Vec2(const float x, const float y);

	void                    Set(const float x, const float y);
	void                    Clear();
	float                   Dot(const Vec2 &vec) const;
	static float            Dot(const Vec2 &vec1, const Vec2 &vec2);
	float                   CrossScalar(const Vec2 &vec) const;
	Vec2                 Cross(const Vec2 &vec) const;
	Vec2                 &Cross(const Vec2 &vec1, const Vec2 &vec2);
	float                   Dot(const Vec3 &vec) const;
	static float            Dot(const Vec3 &vec1, const Vec3 &vec2);
	Vec2                 Cross(const Vec3 &vec) const;
	Vec2                 &Cross(const Vec3 &vec1, const Vec3 &vec2);
	float                   UnitSq() const;
	float                   Unit() const;
	float                   DistanceSq(const Vec2 &vec) const;
	float                   Distance(const Vec2 &vec) const;
	Vec2                 &Normalize();
	Vec2                 Lerp(const Vec2 &next, float movement) const;
	Vec2                 &Lerp(const Vec2 &next, const float movement);
	Vec2                 &Lerp(const Vec2 &start, const Vec2 &next, float movement);
	float                   ToYaw() const;
	float                   *ToFloatPtr();
	Vec3                 ToVec3();

	Vec2                 operator+(const Vec2 &vec);
	Vec2                 operator+(const Vec2 &vec) const;
	Vec2                 operator+(Vec2 &vec);
	Vec2                 operator-() const;
	Vec2                 operator-(const Vec2 &vec) const;
	Vec2                 operator*(const Vec2 &vec);
	Vec2                 operator*(const float val);
	Vec2                 operator*(const float val) const;
	Vec2                 operator/(const Vec2 &vec);
	Vec2                 operator/(const float val);
	Vec2                 &operator=(Vec3 &vec);
	Vec2                 &operator=(const Vec2 &vec);
	Vec2                 &operator=(const Vec3 &vec);
	Vec2                 &operator=(const float *vecs);
	Vec2                 &operator+=(const Vec2 &vec);
	Vec2                 &operator-=(const Vec2 &vec);
	Vec2                 &operator*=(const Vec2 &vec);
	Vec2                 &operator*=(const float val);
	Vec2                 &operator/=(const Vec2 &vec);
	Vec2                 &operator/=(const float val);
	Vec2                 operator*(const Mat4 &mtx);
	Vec2                 operator*(const Mat4 &mtx) const;
	Vec2                 &operator*=(const Mat4 &mtx);
	float                   operator[](int index) const;
	float                   &operator[](int index);
	bool                    operator==(const Vec2 &vec);

	operator float *() { return reinterpret_cast<float*>(&x); }

	static Vec2          vecZero;
	static const Vec2    vecRight;
	static const Vec2    vecUp;

	float                   x;
	float                   y;
};

class Vec3
{
public:
	Vec3();
	Vec3(const float x, const float y, const float z);

	void                    Set(const float x, const float y, const float z);
	void                    Clear();
	float                   Dot(const Vec3 &vec) const;
	static float            Dot(const Vec3 &vec1, const Vec3 &vec2);
	Vec3                 Cross(const Vec3 &vec) const;
	static Vec3          Cross(const Vec3 &vec1, const Vec3 &vec2);
	float                   UnitSq() const;
	float                   Unit() const;
	float                   LengthSq() const { return UnitSq(); }
	float                   Length() const { return Unit(); }
	float                   DistanceSq(const Vec3 &vec) const;
	float                   Distance(const Vec3 &vec) const;
	Vec3                 &Normalize();
	static Vec3          Normalize(Vec3 a);
	Angle                PointAt(Vec3 &location) const;
	Vec3                 Lerp(const Vec3 &next, float movement) const;
	Vec3                 &Lerp(const Vec3 &start, const Vec3 &next, float movement);
	Quat                 ToQuat();
	float                   ToYaw() const;
	float                   ToPitch() const;
	float                   *ToFloatPtr();
	Vec2                 ToVec2();
	Vec2                 ToVec2() const;
	Vec3                 ScreenProject(Mat4 &proj, Mat4 &model,
		const int width, const int height,
		const int wx, const int wy);

	Vec3                 operator+(const Vec3 &vec);
	Vec3                 operator+(const Vec3 &vec) const;
	Vec3                 operator+(Vec3 &vec);
	Vec3                 operator+(const float val) const;
	Vec3                 operator-() const;
	Vec3                 operator-(const float val) const;
	Vec3                 operator-(const Vec3 &vec) const;
	Vec3                 operator*(const Vec3 &vec);
	Vec3                 operator*(const float val);
	Vec3                 operator*(const float val) const;
	Vec3                 operator/(const Vec3 &vec);
	Vec3                 operator/(const float val);
	Vec3                 operator*(const Quat &quat);
	Vec3                 operator*(const Mat4 &mtx);
	Vec3                 operator*(const Mat4 &mtx) const;
	Vec3                 &operator=(const Vec3 &vec);
	Vec3                 &operator=(const float *vecs);
	Vec3                 &operator+=(const Vec3 &vec);
	Vec3                 &operator+=(const float val);
	Vec3                 &operator-=(const Vec3 &vec);
	Vec3                 &operator-=(const float val);
	Vec3                 &operator*=(const Vec3 &vec);
	Vec3                 &operator*=(const float val);
	Vec3                 &operator/=(const Vec3 &vec);
	Vec3                 &operator/=(const float val);
	Vec3                 &operator*=(const Quat &quat);
	Vec3                 &operator*=(const Mat4 &mtx);
	float                   operator[](int index) const;
	float                   &operator[](int index);

	operator float *() { return reinterpret_cast<float*>(&x); }

	static const Vec3    vecForward;
	static const Vec3    vecUp;
	static const Vec3    vecRight;

	float                   x;
	float                   y;
	float                   z;
};

class Vec4
{
public:
	Vec4();
	Vec4(const float x, const float y, const float z, const float w);
	Vec4(const Vec3 &v, const float w);

	void                    Set(const float x, const float y, const float z, const float w);
	void                    Clear();
	float                   *ToFloatPtr();

	static float Dot(const Vec4 &a, const Vec4 &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	const Vec3           &ToVec3() const;
	Vec3                 &ToVec3();
	Vec4                 operator|(const Mat4 &mtx);
	Vec4                 &operator|=(const Mat4 &mtx);
	Vec4                 operator+(const Vec4 &vec) const;
	Vec4                 operator+(const float val) const;
	Vec4                 operator-(const Vec4 &vec) const;
	Vec4                 operator-(const float val) const;
	Vec4                 operator*(const Vec4 &vec) const;
	Vec4                 operator*(const float val) const;
	Vec4                 operator/(const Vec4 &vec) const;
	Vec4                 operator/(const float val) const;
	Vec4                 &operator+=(const Vec4 &vec);
	Vec4                 &operator+=(const float val);
	Vec4                 &operator-=(const Vec4 &vec);
	Vec4                 &operator-=(const float val);
	Vec4                 &operator*=(const Vec4 &vec);
	Vec4                 &operator*=(const float val);
	Vec4                 &operator/=(const Vec4 &vec);
	Vec4                 &operator/=(const float val);
	float                   operator[](int index) const;
	float                   &operator[](int index);

	float                   x;
	float                   y;
	float                   z;
	float                   w;
};

class Mat4
{
public:
	Mat4();
	Mat4(const Mat4 &mtx);
	Mat4(const float x, const float y, const float z);
	Mat4(const Quat &quat);
	Mat4(const float angle, const int axis);

	Mat4               &Identity();
	Mat4               &Identity(const float x, const float y, const float z);
	Mat4               &SetTranslation(const float x, const float y, const float z);
	Mat4               &SetTranslation(const Vec3 &vector);
	Mat4               &AddTranslation(const float x, const float y, const float z);
	Mat4               &AddTranslation(const Vec3 &vector);
	Mat4               &Scale(const float x, const float y, const float z);
	Mat4               &Scale(const Vec3 &vector);
	static Mat4        Scale(const Mat4 &mtx, const float x, const float y, const float z);
	Mat4               &Transpose();
	static Mat4        Transpose(const Mat4 &mtx);
	static Mat4        Invert(Mat4 &mtx);
	Quat                 ToQuat();
	float                   *ToFloatPtr();
	void                    SetViewProjection(float aspect, float fov, float zNear, float zFar);
	void                    SetOrtho(float left, float right,
		float bottom, float top,
		float zNear, float zFar);

	Mat4               operator*(const Vec3 &vector);
	Mat4               &operator*=(const Vec3 &vector);
	Mat4               operator*(const Mat4 &matrix);
	Mat4               &operator*=(const Mat4 &matrix);
	friend Mat4        operator*(const Mat4 &m1, const Mat4 &m2);
	Mat4               &operator=(const Mat4 &matrix);
	Mat4               &operator=(const float *m);
	Mat4               operator|(const Mat4 &matrix);

	float operator[](size_t i) const { return vectors[i >> 2][i & 3]; }
	float &operator[](size_t i) { return vectors[i >> 2][i & 3]; }

	Vec4                 vectors[4];
};

class kexPluecker
{
public:
	kexPluecker();
	kexPluecker(const Vec3 &start, const Vec3 &end, bool bRay = false);

	void                    Clear();
	void                    SetLine(const Vec3 &start, const Vec3 &end);
	void                    SetRay(const Vec3 &start, const Vec3 &dir);
	float                   InnerProduct(const kexPluecker &pluecker) const;

	float                   p[6];
};

class Plane
{
public:
	Plane();
	Plane(const float a, const float b, const float c, const float d);
	Plane(const Vec3 &pt1, const Vec3 &pt2, const Vec3 &pt3);
	Plane(const Vec3 &normal, const Vec3 &point);
	Plane(const Plane &plane);

	enum planeAxis_t
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	const Vec3           &Normal() const;
	Vec3                 &Normal();
	Plane                &SetNormal(const Vec3 &normal);
	Plane                &SetNormal(const Vec3 &pt1, const Vec3 &pt2, const Vec3 &pt3);
	float                   Distance(const Vec3 &point);
	Plane                &SetDistance(const Vec3 &point);
	bool                    IsFacing(const float yaw);
	float                   ToYaw();
	float                   ToPitch();
	Quat                 ToQuat();
	const Vec4           &ToVec4() const;
	Vec4                 &ToVec4();
	const planeAxis_t       BestAxis() const;
	Vec3                 GetInclination();

	static Plane Inverse(const Plane &p) { return Plane(-p.a, -p.b, -p.c, -p.d); }

	float zAt(float x, float y) const { return (d - a * x - b * y) / c; }

	Plane                &operator|(const Quat &quat);
	Plane                &operator|=(const Quat &quat);
	Plane                &operator|(const Mat4 &mtx);
	Plane                &operator|=(const Mat4 &mtx);

	float                   a;
	float                   b;
	float                   c;
	float                   d;
};

class Angle
{
public:
	Angle();
	Angle(const float yaw, const float pitch, const float roll);
	Angle(const Vec3 &vector);
	Angle(const Angle &an);

	Angle                &Round();
	Angle                &Clamp180();
	Angle                &Clamp180Invert();
	Angle                &Clamp180InvertSum(const Angle &angle);
	Angle                Diff(Angle &angle);
	void                    ToAxis(Vec3 *forward, Vec3 *up, Vec3 *right);
	Vec3                 ToForwardAxis();
	Vec3                 ToUpAxis();
	Vec3                 ToRightAxis();
	const Vec3           &ToVec3() const;
	Vec3                 &ToVec3();
	Quat                 ToQuat();

	Angle                operator+(const Angle &angle);
	Angle                operator-(const Angle &angle);
	Angle                &operator+=(const Angle &angle);
	Angle                &operator-=(const Angle &angle);
	Angle                &operator=(const Angle &angle);
	Angle                &operator=(const Vec3 &vector);
	Angle                &operator=(const float *vecs);
	Angle                operator-();
	float                   operator[](int index) const;
	float                   &operator[](int index);

	float                   yaw;
	float                   pitch;
	float                   roll;
};

class BBox
{
public:
	BBox();
	BBox(const Vec3 &vMin, const Vec3 &vMax);

	void                    Clear();
	Vec3                 Center() const;
	Vec3                 Extents() const;
	float                   Radius() const;
	void                    AddPoint(const Vec3 &vec);
	bool                    PointInside(const Vec3 &vec) const;
	bool                    IntersectingBox(const BBox &box) const;
	bool                    IntersectingBox2D(const BBox &box) const;
	float                   DistanceToPlane(Plane &plane);
	bool                    LineIntersect(const Vec3 &start, const Vec3 &end);
	void                    ToPoints(float *points) const;
	void                    ToVectors(Vec3 *vectors) const;

	BBox                 operator+(const float radius) const;
	BBox                 &operator+=(const float radius);
	BBox                 operator+(const Vec3 &vec) const;
	BBox                 operator-(const float radius) const;
	BBox                 operator-(const Vec3 &vec) const;
	BBox                 &operator-=(const float radius);
	BBox                 operator*(const Mat4 &matrix) const;
	BBox                 &operator*=(const Mat4 &matrix);
	BBox                 operator*(const Vec3 &vec) const;
	BBox                 &operator*=(const Vec3 &vec);
	BBox                 &operator=(const BBox &bbox);
	Vec3                 operator[](int index) const;
	Vec3                 &operator[](int index);

	Vec3                 min;
	Vec3                 max;
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

inline void Math::Clamp(Vec3 &v, const float min, const float max)
{
	if (v.x < min) { v.x = min; }
	if (v.x > max) { v.x = max; }
	if (v.y < min) { v.y = min; }
	if (v.y > max) { v.y = max; }
	if (v.z < min) { v.z = min; }
	if (v.z > max) { v.z = max; }
}

/////////////////////////////////////////////////////////////////////////////

inline Vec2::Vec2()
{
	Clear();
}

inline Vec2::Vec2(const float x, const float y)
{
	Set(x, y);
}

inline void Vec2::Set(const float x, const float y)
{
	this->x = x;
	this->y = y;
}

inline void Vec2::Clear()
{
	x = y = 0.0f;
}

inline float Vec2::Dot(const Vec2 &vec) const
{
	return (x * vec.x + y * vec.y);
}

inline float Vec2::Dot(const Vec2 &vec1, const Vec2 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y);
}

inline float Vec2::Dot(const Vec3 &vec) const
{
	return (x * vec.x + y * vec.y);
}

inline float Vec2::Dot(const Vec3 &vec1, const Vec3 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y);
}

inline float Vec2::CrossScalar(const Vec2 &vec) const
{
	return vec.x * y - vec.y * x;
}

inline Vec2 Vec2::Cross(const Vec2 &vec) const
{
	return Vec2(
		vec.y - y,
		x - vec.x
	);
}

inline Vec2 &Vec2::Cross(const Vec2 &vec1, const Vec2 &vec2)
{
	x = vec2.y - vec1.y;
	y = vec1.x - vec2.x;

	return *this;
}

inline Vec2 Vec2::Cross(const Vec3 &vec) const
{
	return Vec2(
		vec.y - y,
		x - vec.x
	);
}

inline Vec2 &Vec2::Cross(const Vec3 &vec1, const Vec3 &vec2)
{
	x = vec2.y - vec1.y;
	y = vec1.x - vec2.x;

	return *this;
}

inline float Vec2::UnitSq() const
{
	return x * x + y * y;
}

inline float Vec2::Unit() const
{
	return Math::Sqrt(UnitSq());
}

inline float Vec2::DistanceSq(const Vec2 &vec) const
{
	return (
		(x - vec.x) * (x - vec.x) +
		(y - vec.y) * (y - vec.y)
		);
}

inline float Vec2::Distance(const Vec2 &vec) const
{
	return Math::Sqrt(DistanceSq(vec));
}

inline Vec2 &Vec2::Normalize()
{
	*this *= Math::InvSqrt(UnitSq());
	return *this;
}

inline Vec2 Vec2::Lerp(const Vec2 &next, float movement) const
{
	return (next - *this) * movement + *this;
}

inline Vec2 &Vec2::Lerp(const Vec2 &next, const float movement)
{
	*this = (next - *this) * movement + *this;
	return *this;
}

inline Vec2 &Vec2::Lerp(const Vec2 &start, const Vec2 &next, float movement)
{
	*this = (next - start) * movement + start;
	return *this;
}

inline float Vec2::ToYaw() const
{
	float d = x * x + y * y;

	if (d == 0.0f)
	{
		return 0.0f;
	}

	return Math::ATan2(x, y);
}

inline float *Vec2::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline Vec3 Vec2::ToVec3()
{
	return Vec3(x, y, 0);
}

inline Vec2 Vec2::operator+(const Vec2 &vec)
{
	return Vec2(x + vec.x, y + vec.y);
}

inline Vec2 Vec2::operator+(const Vec2 &vec) const
{
	return Vec2(x + vec.x, y + vec.y);
}

inline Vec2 Vec2::operator+(Vec2 &vec)
{
	return Vec2(x + vec.x, y + vec.y);
}

inline Vec2 &Vec2::operator+=(const Vec2 &vec)
{
	x += vec.x;
	y += vec.y;
	return *this;
}

inline Vec2 Vec2::operator-(const Vec2 &vec) const
{
	return Vec2(x - vec.x, y - vec.y);
}

inline Vec2 Vec2::operator-() const
{
	return Vec2(-x, -y);
}

inline Vec2 &Vec2::operator-=(const Vec2 &vec)
{
	x -= vec.x;
	y -= vec.y;
	return *this;
}

inline Vec2 Vec2::operator*(const Vec2 &vec)
{
	return Vec2(x * vec.x, y * vec.y);
}

inline Vec2 &Vec2::operator*=(const Vec2 &vec)
{
	x *= vec.x;
	y *= vec.y;
	return *this;
}

inline Vec2 Vec2::operator*(const float val)
{
	return Vec2(x * val, y * val);
}

inline Vec2 Vec2::operator*(const float val) const
{
	return Vec2(x * val, y * val);
}

inline Vec2 &Vec2::operator*=(const float val)
{
	x *= val;
	y *= val;
	return *this;
}

inline Vec2 Vec2::operator/(const Vec2 &vec)
{
	return Vec2(x / vec.x, y / vec.y);
}

inline Vec2 &Vec2::operator/=(const Vec2 &vec)
{
	x /= vec.x;
	y /= vec.y;
	return *this;
}

inline Vec2 Vec2::operator/(const float val)
{
	return Vec2(x / val, y / val);
}

inline Vec2 &Vec2::operator/=(const float val)
{
	x /= val;
	y /= val;
	return *this;
}

inline Vec2 Vec2::operator*(const Mat4 &mtx)
{
	return Vec2(mtx.vectors[1].x * y + mtx.vectors[0].x * x + mtx.vectors[3].x, mtx.vectors[1].y * y + mtx.vectors[0].y * x + mtx.vectors[3].y);
}

inline Vec2 Vec2::operator*(const Mat4 &mtx) const
{
	return Vec2(mtx.vectors[1].x * y + mtx.vectors[0].x * x + mtx.vectors[3].x, mtx.vectors[1].y * y + mtx.vectors[0].y * x + mtx.vectors[3].y);
}

inline Vec2 &Vec2::operator*=(const Mat4 &mtx)
{
	float _x = x;
	float _y = y;

	x = mtx.vectors[1].x * _y + mtx.vectors[0].x * _x + mtx.vectors[3].x;
	y = mtx.vectors[1].y * _y + mtx.vectors[0].y * _x + mtx.vectors[3].y;

	return *this;
}

inline Vec2 &Vec2::operator=(const Vec2 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline Vec2 &Vec2::operator=(const Vec3 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline Vec2 &Vec2::operator=(Vec3 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline Vec2 &Vec2::operator=(const float *vecs)
{
	x = vecs[0];
	y = vecs[2];
	return *this;
}

inline float Vec2::operator[](int index) const
{
	return (&x)[index];
}

inline float &Vec2::operator[](int index)
{
	return (&x)[index];
}

inline bool Vec2::operator==(const Vec2 &vec)
{
	return ((x == vec.x) && (y == vec.y));
}

/////////////////////////////////////////////////////////////////////////////

inline Vec3::Vec3()
{
	Clear();
}

inline Vec3::Vec3(const float x, const float y, const float z)
{
	Set(x, y, z);
}

inline void Vec3::Set(const float x, const float y, const float z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

inline void Vec3::Clear()
{
	x = y = z = 0.0f;
}

inline float Vec3::Dot(const Vec3 &vec) const
{
	return (x * vec.x + y * vec.y + z * vec.z);
}

inline float Vec3::Dot(const Vec3 &vec1, const Vec3 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z);
}

inline Vec3 Vec3::Cross(const Vec3 &vec) const
{
	return Vec3(
		vec.z * y - z * vec.y,
		vec.x * z - x * vec.z,
		x * vec.y - vec.x * y
	);
}

inline Vec3 Vec3::Cross(const Vec3 &vec1, const Vec3 &vec2)
{
	return vec1.Cross(vec2);
}

inline float Vec3::UnitSq() const
{
	return x * x + y * y + z * z;
}

inline float Vec3::Unit() const
{
	return Math::Sqrt(UnitSq());
}

inline float Vec3::DistanceSq(const Vec3 &vec) const
{
	return (
		(x - vec.x) * (x - vec.x) +
		(y - vec.y) * (y - vec.y) +
		(z - vec.z) * (z - vec.z)
		);
}

inline float Vec3::Distance(const Vec3 &vec) const
{
	return Math::Sqrt(DistanceSq(vec));
}

inline Vec3 &Vec3::Normalize()
{
	*this *= Math::InvSqrt(UnitSq());
	return *this;
}

inline Vec3 Vec3::Normalize(Vec3 a)
{
	a.Normalize();
	return a;
}

inline Angle Vec3::PointAt(Vec3 &location) const
{
	Vec3 dir = (*this - location).Normalize();

	return Angle(dir.ToYaw(), dir.ToPitch(), 0);
}

inline Vec3 Vec3::Lerp(const Vec3 &next, float movement) const
{
	return (next - *this) * movement + *this;
}

inline Vec3 &Vec3::Lerp(const Vec3 &start, const Vec3 &next, float movement)
{
	*this = (next - start) * movement + start;
	return *this;
}

inline Quat Vec3::ToQuat()
{
	Vec3 scv = *this * Math::InvSqrt(UnitSq());
	return Quat(Math::ACos(scv.z), vecForward.Cross(scv).Normalize());
}

inline float Vec3::ToYaw() const
{
	float d = x * x + z * z;

	if (d == 0.0f)
	{
		return 0.0f;
	}

	return Math::ATan2(x, z);
}

inline float Vec3::ToPitch() const
{
	float d = x * x + z * z;

	if (d == 0.0f)
	{
		if (y > 0.0f)
		{
			return DEG2RAD(90);
		}
		else
		{
			return DEG2RAD(-90);
		}
	}

	return Math::ATan2(y, d);
}

inline float *Vec3::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline Vec2 Vec3::ToVec2()
{
	return Vec2(x, y);
}

inline Vec2 Vec3::ToVec2() const
{
	return Vec2(x, y);
}

inline Vec3 Vec3::ScreenProject(Mat4 &proj, Mat4 &model, const int width, const int height, const int wx, const int wy)
{
	Vec4 projVec;
	Vec4 modelVec;

	modelVec.ToVec3() = *this;
	modelVec |= model;

	projVec = (modelVec | proj);
	projVec.x *= modelVec.w;
	projVec.y *= modelVec.w;
	projVec.z *= modelVec.w;

	if (projVec.w != 0)
	{
		projVec.w = 1.0f / projVec.w;
		projVec.x *= projVec.w;
		projVec.y *= projVec.w;
		projVec.z *= projVec.w;

		return Vec3(
			(projVec.x * 0.5f + 0.5f) * width + wx,
			(-projVec.y * 0.5f + 0.5f) * height + wy,
			(1.0f + projVec.z) * 0.5f);
	}

	return Vec3(*this);
}

inline Vec3 Vec3::operator+(const Vec3 &vec)
{
	return Vec3(x + vec.x, y + vec.y, z + vec.z);
}

inline Vec3 Vec3::operator+(const Vec3 &vec) const
{
	return Vec3(x + vec.x, y + vec.y, z + vec.z);
}

inline Vec3 Vec3::operator+(const float val) const
{
	return Vec3(x + val, y + val, z + val);
}

inline Vec3 Vec3::operator+(Vec3 &vec)
{
	return Vec3(x + vec.x, y + vec.y, z + vec.z);
}

inline Vec3 &Vec3::operator+=(const Vec3 &vec)
{
	x += vec.x;
	y += vec.y;
	z += vec.z;
	return *this;
}

inline Vec3 &Vec3::operator+=(const float val)
{
	x += val;
	y += val;
	z += val;
	return *this;
}

inline Vec3 Vec3::operator-(const Vec3 &vec) const
{
	return Vec3(x - vec.x, y - vec.y, z - vec.z);
}

inline Vec3 Vec3::operator-(const float val) const
{
	return Vec3(x - val, y - val, z - val);
}

inline Vec3 Vec3::operator-() const
{
	return Vec3(-x, -y, -z);
}

inline Vec3 &Vec3::operator-=(const Vec3 &vec)
{
	x -= vec.x;
	y -= vec.y;
	z -= vec.z;
	return *this;
}

inline Vec3 &Vec3::operator-=(const float val)
{
	x -= val;
	y -= val;
	z -= val;
	return *this;
}

inline Vec3 Vec3::operator*(const Vec3 &vec)
{
	return Vec3(x * vec.x, y * vec.y, z * vec.z);
}

inline Vec3 &Vec3::operator*=(const Vec3 &vec)
{
	x *= vec.x;
	y *= vec.y;
	z *= vec.z;
	return *this;
}

inline Vec3 Vec3::operator*(const float val)
{
	return Vec3(x * val, y * val, z * val);
}

inline Vec3 Vec3::operator*(const float val) const
{
	return Vec3(x * val, y * val, z * val);
}

inline Vec3 &Vec3::operator*=(const float val)
{
	x *= val;
	y *= val;
	z *= val;
	return *this;
}

inline Vec3 Vec3::operator/(const Vec3 &vec)
{
	return Vec3(x / vec.x, y / vec.y, z / vec.z);
}

inline Vec3 &Vec3::operator/=(const Vec3 &vec)
{
	x /= vec.x;
	y /= vec.y;
	z /= vec.z;
	return *this;
}

inline Vec3 Vec3::operator/(const float val)
{
	return Vec3(x / val, y / val, z / val);
}

inline Vec3 &Vec3::operator/=(const float val)
{
	x /= val;
	y /= val;
	z /= val;
	return *this;
}

inline Vec3 Vec3::operator*(const Quat &quat)
{
	float xx = quat.x * quat.x;
	float yx = quat.y * quat.x;
	float zx = quat.z * quat.x;
	float wx = quat.w * quat.x;
	float yy = quat.y * quat.y;
	float zy = quat.z * quat.y;
	float wy = quat.w * quat.y;
	float zz = quat.z * quat.z;
	float wz = quat.w * quat.z;
	float ww = quat.w * quat.w;

	return Vec3(
		((yx + yx) - (wz + wz)) * y +
		((wy + wy + zx + zx)) * z +
		(((ww + xx) - yy) - zz) * x,
		((yy + (ww - xx)) - zz) * y +
		((zy + zy) - (wx + wx)) * z +
		((wz + wz) + (yx + yx)) * x,
		((wx + wx) + (zy + zy)) * y +
		(((ww - xx) - yy) + zz) * z +
		((zx + zx) - (wy + wy)) * x
	);
}

inline Vec3 Vec3::operator*(const Mat4 &mtx)
{
	return Vec3(mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z);
}

inline Vec3 Vec3::operator*(const Mat4 &mtx) const
{
	return Vec3(mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z);
}

inline Vec3 &Vec3::operator*=(const Mat4 &mtx)
{
	float _x = x;
	float _y = y;
	float _z = z;

	x = mtx.vectors[1].x * _y + mtx.vectors[2].x * _z + mtx.vectors[0].x * _x + mtx.vectors[3].x;
	y = mtx.vectors[1].y * _y + mtx.vectors[2].y * _z + mtx.vectors[0].y * _x + mtx.vectors[3].y;
	z = mtx.vectors[1].z * _y + mtx.vectors[2].z * _z + mtx.vectors[0].z * _x + mtx.vectors[3].z;

	return *this;
}

inline Vec3 &Vec3::operator*=(const Quat &quat)
{
	float xx = quat.x * quat.x;
	float yx = quat.y * quat.x;
	float zx = quat.z * quat.x;
	float wx = quat.w * quat.x;
	float yy = quat.y * quat.y;
	float zy = quat.z * quat.y;
	float wy = quat.w * quat.y;
	float zz = quat.z * quat.z;
	float wz = quat.w * quat.z;
	float ww = quat.w * quat.w;
	float vx = x;
	float vy = y;
	float vz = z;

	x = ((yx + yx) - (wz + wz)) * vy +
		((wy + wy + zx + zx)) * vz +
		(((ww + xx) - yy) - zz) * vx;
	y = ((yy + (ww - xx)) - zz) * vy +
		((zy + zy) - (wx + wx)) * vz +
		((wz + wz) + (yx + yx)) * vx;
	z = ((wx + wx) + (zy + zy)) * vy +
		(((ww - xx) - yy) + zz) * vz +
		((zx + zx) - (wy + wy)) * vx;

	return *this;
}

inline Vec3 &Vec3::operator=(const Vec3 &vec)
{
	x = vec.x;
	y = vec.y;
	z = vec.z;
	return *this;
}

inline Vec3 &Vec3::operator=(const float *vecs)
{
	x = vecs[0];
	y = vecs[1];
	z = vecs[2];
	return *this;
}

inline float Vec3::operator[](int index) const
{
	return (&x)[index];
}

inline float &Vec3::operator[](int index)
{
	return (&x)[index];
}

/////////////////////////////////////////////////////////////////////////////

inline Vec4::Vec4()
{
	Clear();
}

inline Vec4::Vec4(const float x, const float y, const float z, const float w)
{
	Set(x, y, z, w);
}

inline Vec4::Vec4(const Vec3 &v, const float w)
{
	Set(v.x, v.y, v.z, w);
}

inline void Vec4::Set(const float x, const float y, const float z, const float w)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

inline void Vec4::Clear()
{
	x = y = z = w = 0.0f;
}

inline Vec3 const &Vec4::ToVec3() const
{
	return *reinterpret_cast<const Vec3*>(this);
}

inline Vec3 &Vec4::ToVec3()
{
	return *reinterpret_cast<Vec3*>(this);
}

inline float *Vec4::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline Vec4 Vec4::operator+(const Vec4 &vec) const
{
	return Vec4(x + vec.x, y + vec.y, z + vec.z, w + vec.w);
}

inline Vec4 Vec4::operator+(const float val) const
{
	return Vec4(x + val, y + val, z + val, w + val);
}

inline Vec4 Vec4::operator-(const Vec4 &vec) const
{
	return Vec4(x - vec.x, y - vec.y, z - vec.z, w - vec.w);
}

inline Vec4 Vec4::operator-(const float val) const
{
	return Vec4(x - val, y - val, z - val, w - val);
}

inline Vec4 Vec4::operator*(const Vec4 &vec) const
{
	return Vec4(x * vec.x, y * vec.y, z * vec.z, w * vec.w);
}

inline Vec4 Vec4::operator*(const float val) const
{
	return Vec4(x * val, y * val, z * val, w * val);
}

inline Vec4 Vec4::operator/(const Vec4 &vec) const
{
	return Vec4(x / vec.x, y / vec.y, z / vec.z, w / vec.w);
}

inline Vec4 Vec4::operator/(const float val) const
{
	return Vec4(x / val, y / val, z / val, w / val);
}

inline Vec4 &Vec4::operator+=(const Vec4 &vec)
{
	x += vec.x; y += vec.y; z += vec.z; w += vec.w;
	return *this;
}

inline Vec4 &Vec4::operator+=(const float val)
{
	x += val; y += val; z += val; w += val;
	return *this;
}

inline Vec4 &Vec4::operator-=(const Vec4 &vec)
{
	x -= vec.x; y -= vec.y; z -= vec.z; w -= vec.w;
	return *this;
}

inline Vec4 &Vec4::operator-=(const float val)
{
	x -= val; y -= val; z -= val; w -= val;
	return *this;
}

inline Vec4 &Vec4::operator*=(const Vec4 &vec)
{
	x *= vec.x; y *= vec.y; z *= vec.z; w *= vec.w;
	return *this;
}

inline Vec4 &Vec4::operator*=(const float val)
{
	x *= val; y *= val; z *= val; w *= val;
	return *this;
}

inline Vec4 &Vec4::operator/=(const Vec4 &vec)
{
	x /= vec.x; y /= vec.y; z /= vec.z; w /= vec.w;
	return *this;
}

inline Vec4 &Vec4::operator/=(const float val)
{
	x /= val; y /= val; z /= val; w /= val;
	return *this;
}

inline Vec4 Vec4::operator|(const Mat4 &mtx)
{
	return Vec4(
		mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z,
		mtx.vectors[1].w * y + mtx.vectors[2].w * z + mtx.vectors[0].w * x + mtx.vectors[3].w);
}

inline Vec4 &Vec4::operator|=(const Mat4 &mtx)
{
	float _x = x;
	float _y = y;
	float _z = z;

	x = mtx.vectors[1].x * _y + mtx.vectors[2].x * _z + mtx.vectors[0].x * _x + mtx.vectors[3].x;
	y = mtx.vectors[1].y * _y + mtx.vectors[2].y * _z + mtx.vectors[0].y * _x + mtx.vectors[3].y;
	z = mtx.vectors[1].z * _y + mtx.vectors[2].z * _z + mtx.vectors[0].z * _x + mtx.vectors[3].z;
	w = mtx.vectors[1].w * _y + mtx.vectors[2].w * _z + mtx.vectors[0].w * _x + mtx.vectors[3].w;

	return *this;
}

inline float Vec4::operator[](int index) const
{
	return (&x)[index];
}

inline float &Vec4::operator[](int index)
{
	return (&x)[index];
}

/////////////////////////////////////////////////////////////////////////////

inline Vec3 BBox::Center() const
{
	return Vec3(
		(max.x + min.x) * 0.5f,
		(max.y + min.y) * 0.5f,
		(max.z + min.z) * 0.5f);
}

inline Vec3 BBox::Extents() const
{
	return Vec3(
		(max.x - min.x) * 0.5f,
		(max.y - min.y) * 0.5f,
		(max.z - min.z) * 0.5f);
}
