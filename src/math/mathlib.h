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

class kexVec2;
class kexVec3;
class kexVec4;
class kexMatrix;
class kexAngle;
class kexStr;

class kexMath
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
	static void             Clamp(kexVec3 &f, const float min, const float max);

	static void             CubicCurve(const kexVec3 &start, const kexVec3 &end, const float time,
		const kexVec3 &point, kexVec3 *vec);
	static void             QuadraticCurve(const kexVec3 &start, const kexVec3 &end, const float time,
		const kexVec3 &pt1, const kexVec3 &pt2, kexVec3 *vec);
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

class kexQuat
{
public:
	kexQuat();

	explicit kexQuat(const float angle, const float x, const float y, const float z);
	explicit kexQuat(const float angle, kexVec3 &vector);
	explicit kexQuat(const float angle, const kexVec3 &vector);

	void                    Set(const float x, const float y, const float z, const float w);
	void                    Clear();
	float                   Dot(const kexQuat &quat) const;
	float                   UnitSq() const;
	float                   Unit() const;
	kexQuat                 &Normalize();
	kexQuat                 Slerp(const kexQuat &quat, float movement) const;
	kexQuat                 RotateFrom(const kexVec3 &location, const kexVec3 &target, float maxAngle);
	kexQuat                 Inverse() const;

	kexQuat                 operator+(const kexQuat &quat);
	kexQuat                 &operator+=(const kexQuat &quat);
	kexQuat                 operator-(const kexQuat &quat);
	kexQuat                 &operator-=(const kexQuat &quat);
	kexQuat                 operator*(const kexQuat &quat);
	kexQuat                 operator*(const float val) const;
	kexQuat                 &operator*=(const kexQuat &quat);
	kexQuat                 &operator*=(const float val);
	kexQuat                 &operator=(const kexQuat &quat);
	kexQuat                 &operator=(const kexVec4 &vec);
	kexQuat                 &operator=(const float *vecs);
	kexVec3                 operator|(const kexVec3 &vector);

	const kexVec3           &ToVec3() const;
	kexVec3                 &ToVec3();

	float                   x;
	float                   y;
	float                   z;
	float                   w;
};

class kexVec2
{
public:
	kexVec2();
	explicit kexVec2(const float x, const float y);

	void                    Set(const float x, const float y);
	void                    Clear();
	float                   Dot(const kexVec2 &vec) const;
	static float            Dot(const kexVec2 &vec1, const kexVec2 &vec2);
	float                   CrossScalar(const kexVec2 &vec) const;
	kexVec2                 Cross(const kexVec2 &vec) const;
	kexVec2                 &Cross(const kexVec2 &vec1, const kexVec2 &vec2);
	float                   Dot(const kexVec3 &vec) const;
	static float            Dot(const kexVec3 &vec1, const kexVec3 &vec2);
	kexVec2                 Cross(const kexVec3 &vec) const;
	kexVec2                 &Cross(const kexVec3 &vec1, const kexVec3 &vec2);
	float                   UnitSq() const;
	float                   Unit() const;
	float                   DistanceSq(const kexVec2 &vec) const;
	float                   Distance(const kexVec2 &vec) const;
	kexVec2                 &Normalize();
	kexVec2                 Lerp(const kexVec2 &next, float movement) const;
	kexVec2                 &Lerp(const kexVec2 &next, const float movement);
	kexVec2                 &Lerp(const kexVec2 &start, const kexVec2 &next, float movement);
	//kexStr                  ToString() const;
	float                   ToYaw() const;
	float                   *ToFloatPtr();
	kexVec3                 ToVec3();

	kexVec2                 operator+(const kexVec2 &vec);
	kexVec2                 operator+(const kexVec2 &vec) const;
	kexVec2                 operator+(kexVec2 &vec);
	kexVec2                 operator-() const;
	kexVec2                 operator-(const kexVec2 &vec) const;
	kexVec2                 operator*(const kexVec2 &vec);
	kexVec2                 operator*(const float val);
	kexVec2                 operator*(const float val) const;
	kexVec2                 operator/(const kexVec2 &vec);
	kexVec2                 operator/(const float val);
	kexVec2                 &operator=(kexVec3 &vec);
	kexVec2                 &operator=(const kexVec2 &vec);
	kexVec2                 &operator=(const kexVec3 &vec);
	kexVec2                 &operator=(const float *vecs);
	kexVec2                 &operator+=(const kexVec2 &vec);
	kexVec2                 &operator-=(const kexVec2 &vec);
	kexVec2                 &operator*=(const kexVec2 &vec);
	kexVec2                 &operator*=(const float val);
	kexVec2                 &operator/=(const kexVec2 &vec);
	kexVec2                 &operator/=(const float val);
	kexVec2                 operator*(const kexMatrix &mtx);
	kexVec2                 operator*(const kexMatrix &mtx) const;
	kexVec2                 &operator*=(const kexMatrix &mtx);
	float                   operator[](int index) const;
	float                   &operator[](int index);
	bool                    operator==(const kexVec2 &vec);

	operator float *() { return reinterpret_cast<float*>(&x); }

	static kexVec2          vecZero;
	static const kexVec2    vecRight;
	static const kexVec2    vecUp;

	float                   x;
	float                   y;
};

class kexVec3
{
public:
	kexVec3();
	explicit kexVec3(const float x, const float y, const float z);

	void                    Set(const float x, const float y, const float z);
	void                    Clear();
	float                   Dot(const kexVec3 &vec) const;
	static float            Dot(const kexVec3 &vec1, const kexVec3 &vec2);
	kexVec3                 Cross(const kexVec3 &vec) const;
	static kexVec3          Cross(const kexVec3 &vec1, const kexVec3 &vec2);
	float                   UnitSq() const;
	float                   Unit() const;
	float                   LengthSq() const { return UnitSq(); }
	float                   Length() const { return Unit(); }
	float                   DistanceSq(const kexVec3 &vec) const;
	float                   Distance(const kexVec3 &vec) const;
	kexVec3                 &Normalize();
	static kexVec3          Normalize(kexVec3 a);
	kexAngle                PointAt(kexVec3 &location) const;
	kexVec3                 Lerp(const kexVec3 &next, float movement) const;
	kexVec3                 &Lerp(const kexVec3 &start, const kexVec3 &next, float movement);
	kexQuat                 ToQuat();
	float                   ToYaw() const;
	float                   ToPitch() const;
	//kexStr                  ToString() const;
	float                   *ToFloatPtr();
	kexVec2                 ToVec2();
	kexVec2                 ToVec2() const;
	kexVec3                 ScreenProject(kexMatrix &proj, kexMatrix &model,
		const int width, const int height,
		const int wx, const int wy);

	kexVec3                 operator+(const kexVec3 &vec);
	kexVec3                 operator+(const kexVec3 &vec) const;
	kexVec3                 operator+(kexVec3 &vec);
	kexVec3                 operator+(const float val) const;
	kexVec3                 operator-() const;
	kexVec3                 operator-(const float val) const;
	kexVec3                 operator-(const kexVec3 &vec) const;
	kexVec3                 operator*(const kexVec3 &vec);
	kexVec3                 operator*(const float val);
	kexVec3                 operator*(const float val) const;
	kexVec3                 operator/(const kexVec3 &vec);
	kexVec3                 operator/(const float val);
	kexVec3                 operator*(const kexQuat &quat);
	kexVec3                 operator*(const kexMatrix &mtx);
	kexVec3                 operator*(const kexMatrix &mtx) const;
	kexVec3                 &operator=(const kexVec3 &vec);
	kexVec3                 &operator=(const float *vecs);
	kexVec3                 &operator+=(const kexVec3 &vec);
	kexVec3                 &operator+=(const float val);
	kexVec3                 &operator-=(const kexVec3 &vec);
	kexVec3                 &operator-=(const float val);
	kexVec3                 &operator*=(const kexVec3 &vec);
	kexVec3                 &operator*=(const float val);
	kexVec3                 &operator/=(const kexVec3 &vec);
	kexVec3                 &operator/=(const float val);
	kexVec3                 &operator*=(const kexQuat &quat);
	kexVec3                 &operator*=(const kexMatrix &mtx);
	float                   operator[](int index) const;
	float                   &operator[](int index);

	operator float *() { return reinterpret_cast<float*>(&x); }

	static const kexVec3    vecForward;
	static const kexVec3    vecUp;
	static const kexVec3    vecRight;

	float                   x;
	float                   y;
	float                   z;
};

class kexVec4
{
public:
	kexVec4();
	explicit kexVec4(const float x, const float y, const float z, const float w);
	explicit kexVec4(const kexVec3 &v, const float w);

	void                    Set(const float x, const float y, const float z, const float w);
	void                    Clear();
	float                   *ToFloatPtr();

	static float Dot(const kexVec4 &a, const kexVec4 &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	const kexVec3           &ToVec3() const;
	kexVec3                 &ToVec3();
	kexVec4                 operator|(const kexMatrix &mtx);
	kexVec4                 &operator|=(const kexMatrix &mtx);
	kexVec4                 operator+(const kexVec4 &vec) const;
	kexVec4                 operator+(const float val) const;
	kexVec4                 operator-(const kexVec4 &vec) const;
	kexVec4                 operator-(const float val) const;
	kexVec4                 operator*(const kexVec4 &vec) const;
	kexVec4                 operator*(const float val) const;
	kexVec4                 operator/(const kexVec4 &vec) const;
	kexVec4                 operator/(const float val) const;
	kexVec4                 &operator+=(const kexVec4 &vec);
	kexVec4                 &operator+=(const float val);
	kexVec4                 &operator-=(const kexVec4 &vec);
	kexVec4                 &operator-=(const float val);
	kexVec4                 &operator*=(const kexVec4 &vec);
	kexVec4                 &operator*=(const float val);
	kexVec4                 &operator/=(const kexVec4 &vec);
	kexVec4                 &operator/=(const float val);
	float                   operator[](int index) const;
	float                   &operator[](int index);

	float                   x;
	float                   y;
	float                   z;
	float                   w;
};

class kexMatrix
{
public:
	kexMatrix();
	kexMatrix(const kexMatrix &mtx);
	kexMatrix(const float x, const float y, const float z);
	kexMatrix(const kexQuat &quat);
	kexMatrix(const float angle, const int axis);

	kexMatrix               &Identity();
	kexMatrix               &Identity(const float x, const float y, const float z);
	kexMatrix               &SetTranslation(const float x, const float y, const float z);
	kexMatrix               &SetTranslation(const kexVec3 &vector);
	kexMatrix               &AddTranslation(const float x, const float y, const float z);
	kexMatrix               &AddTranslation(const kexVec3 &vector);
	kexMatrix               &Scale(const float x, const float y, const float z);
	kexMatrix               &Scale(const kexVec3 &vector);
	static kexMatrix        Scale(const kexMatrix &mtx, const float x, const float y, const float z);
	kexMatrix               &Transpose();
	static kexMatrix        Transpose(const kexMatrix &mtx);
	static kexMatrix        Invert(kexMatrix &mtx);
	kexQuat                 ToQuat();
	float                   *ToFloatPtr();
	void                    SetViewProjection(float aspect, float fov, float zNear, float zFar);
	void                    SetOrtho(float left, float right,
		float bottom, float top,
		float zNear, float zFar);

	kexMatrix               operator*(const kexVec3 &vector);
	kexMatrix               &operator*=(const kexVec3 &vector);
	kexMatrix               operator*(const kexMatrix &matrix);
	kexMatrix               &operator*=(const kexMatrix &matrix);
	friend kexMatrix        operator*(const kexMatrix &m1, const kexMatrix &m2);
	kexMatrix               &operator=(const kexMatrix &matrix);
	kexMatrix               &operator=(const float *m);
	kexMatrix               operator|(const kexMatrix &matrix);

	float operator[](size_t i) const { return vectors[i >> 2][i & 3]; }
	float &operator[](size_t i) { return vectors[i >> 2][i & 3]; }

	kexVec4                 vectors[4];
};

class kexPluecker
{
public:
	kexPluecker();
	kexPluecker(const kexVec3 &start, const kexVec3 &end, bool bRay = false);

	void                    Clear();
	void                    SetLine(const kexVec3 &start, const kexVec3 &end);
	void                    SetRay(const kexVec3 &start, const kexVec3 &dir);
	float                   InnerProduct(const kexPluecker &pluecker) const;

	float                   p[6];
};

class kexPlane
{
public:
	kexPlane();
	kexPlane(const float a, const float b, const float c, const float d);
	kexPlane(const kexVec3 &pt1, const kexVec3 &pt2, const kexVec3 &pt3);
	kexPlane(const kexVec3 &normal, const kexVec3 &point);
	kexPlane(const kexPlane &plane);

	enum planeAxis_t
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	const kexVec3           &Normal() const;
	kexVec3                 &Normal();
	kexPlane                &SetNormal(const kexVec3 &normal);
	kexPlane                &SetNormal(const kexVec3 &pt1, const kexVec3 &pt2, const kexVec3 &pt3);
	float                   Distance(const kexVec3 &point);
	kexPlane                &SetDistance(const kexVec3 &point);
	bool                    IsFacing(const float yaw);
	float                   ToYaw();
	float                   ToPitch();
	kexQuat                 ToQuat();
	const kexVec4           &ToVec4() const;
	kexVec4                 &ToVec4();
	const planeAxis_t       BestAxis() const;
	kexVec3                 GetInclination();

	static kexPlane Inverse(const kexPlane &p) { return kexPlane(-p.a, -p.b, -p.c, -p.d); }

	float zAt(float x, float y) const { return (d - a * x - b * y) / c; }

	kexPlane                &operator|(const kexQuat &quat);
	kexPlane                &operator|=(const kexQuat &quat);
	kexPlane                &operator|(const kexMatrix &mtx);
	kexPlane                &operator|=(const kexMatrix &mtx);

	float                   a;
	float                   b;
	float                   c;
	float                   d;
};

class kexAngle
{
public:
	kexAngle();
	kexAngle(const float yaw, const float pitch, const float roll);
	kexAngle(const kexVec3 &vector);
	kexAngle(const kexAngle &an);

	kexAngle                &Round();
	kexAngle                &Clamp180();
	kexAngle                &Clamp180Invert();
	kexAngle                &Clamp180InvertSum(const kexAngle &angle);
	kexAngle                Diff(kexAngle &angle);
	void                    ToAxis(kexVec3 *forward, kexVec3 *up, kexVec3 *right);
	kexVec3                 ToForwardAxis();
	kexVec3                 ToUpAxis();
	kexVec3                 ToRightAxis();
	const kexVec3           &ToVec3() const;
	kexVec3                 &ToVec3();
	kexQuat                 ToQuat();

	kexAngle                operator+(const kexAngle &angle);
	kexAngle                operator-(const kexAngle &angle);
	kexAngle                &operator+=(const kexAngle &angle);
	kexAngle                &operator-=(const kexAngle &angle);
	kexAngle                &operator=(const kexAngle &angle);
	kexAngle                &operator=(const kexVec3 &vector);
	kexAngle                &operator=(const float *vecs);
	kexAngle                operator-();
	float                   operator[](int index) const;
	float                   &operator[](int index);

	float                   yaw;
	float                   pitch;
	float                   roll;
};

class kexBBox
{
public:
	kexBBox();
	explicit kexBBox(const kexVec3 &vMin, const kexVec3 &vMax);

	void                    Clear();
	kexVec3                 Center() const;
	kexVec3                 Extents() const;
	float                   Radius() const;
	void                    AddPoint(const kexVec3 &vec);
	bool                    PointInside(const kexVec3 &vec) const;
	bool                    IntersectingBox(const kexBBox &box) const;
	bool                    IntersectingBox2D(const kexBBox &box) const;
	float                   DistanceToPlane(kexPlane &plane);
	bool                    LineIntersect(const kexVec3 &start, const kexVec3 &end);
	void                    ToPoints(float *points) const;
	void                    ToVectors(kexVec3 *vectors) const;

	kexBBox                 operator+(const float radius) const;
	kexBBox                 &operator+=(const float radius);
	kexBBox                 operator+(const kexVec3 &vec) const;
	kexBBox                 operator-(const float radius) const;
	kexBBox                 operator-(const kexVec3 &vec) const;
	kexBBox                 &operator-=(const float radius);
	kexBBox                 operator*(const kexMatrix &matrix) const;
	kexBBox                 &operator*=(const kexMatrix &matrix);
	kexBBox                 operator*(const kexVec3 &vec) const;
	kexBBox                 &operator*=(const kexVec3 &vec);
	kexBBox                 &operator=(const kexBBox &bbox);
	kexVec3                 operator[](int index) const;
	kexVec3                 &operator[](int index);

	kexVec3                 min;
	kexVec3                 max;
};

/////////////////////////////////////////////////////////////////////////////

inline float kexMath::Sin(float x)
{
	return sinf(x);
}

inline float kexMath::Cos(float x)
{
	return cosf(x);
}

inline float kexMath::Tan(float x)
{
	return tanf(x);
}

inline float kexMath::ATan2(float x, float y)
{
	return atan2f(x, y);
}

inline float kexMath::ACos(float x)
{
	return acosf(x);
}

inline float kexMath::Sqrt(float x)
{
	return x * InvSqrt(x);
}

inline float kexMath::Pow(float x, float y)
{
	return powf(x, y);
}

inline float kexMath::Log(float x)
{
	return logf(x);
}

inline float kexMath::Floor(float x)
{
	return floorf(x);
}

inline float kexMath::Ceil(float x)
{
	return ceilf(x);
}

inline float kexMath::Deg2Rad(float x)
{
	return DEG2RAD(x);
}

inline float kexMath::Rad2Deg(float x)
{
	return RAD2DEG(x);
}

inline int kexMath::Abs(int x)
{
	int y = x >> 31;
	return ((x ^ y) - y);
}

inline float kexMath::Fabs(float x)
{
	int tmp = *reinterpret_cast<int*>(&x);
	tmp &= 0x7FFFFFFF;
	return *reinterpret_cast<float*>(&tmp);
}

inline float kexMath::InvSqrt(float x)
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

inline void kexMath::Clamp(float &f, const float min, const float max)
{
	if (f < min) { f = min; }
	if (f > max) { f = max; }
}

inline void kexMath::Clamp(uint8_t &b, const uint8_t min, const uint8_t max)
{
	if (b < min) { b = min; }
	if (b > max) { b = max; }
}

inline void kexMath::Clamp(int &i, const int min, const int max)
{
	if (i < min) { i = min; }
	if (i > max) { i = max; }
}

inline void kexMath::Clamp(kexVec3 &v, const float min, const float max)
{
	if (v.x < min) { v.x = min; }
	if (v.x > max) { v.x = max; }
	if (v.y < min) { v.y = min; }
	if (v.y > max) { v.y = max; }
	if (v.z < min) { v.z = min; }
	if (v.z > max) { v.z = max; }
}

/////////////////////////////////////////////////////////////////////////////

inline kexVec2::kexVec2()
{
	Clear();
}

inline kexVec2::kexVec2(const float x, const float y)
{
	Set(x, y);
}

inline void kexVec2::Set(const float x, const float y)
{
	this->x = x;
	this->y = y;
}

inline void kexVec2::Clear()
{
	x = y = 0.0f;
}

inline float kexVec2::Dot(const kexVec2 &vec) const
{
	return (x * vec.x + y * vec.y);
}

inline float kexVec2::Dot(const kexVec2 &vec1, const kexVec2 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y);
}

inline float kexVec2::Dot(const kexVec3 &vec) const
{
	return (x * vec.x + y * vec.y);
}

inline float kexVec2::Dot(const kexVec3 &vec1, const kexVec3 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y);
}

inline float kexVec2::CrossScalar(const kexVec2 &vec) const
{
	return vec.x * y - vec.y * x;
}

inline kexVec2 kexVec2::Cross(const kexVec2 &vec) const
{
	return kexVec2(
		vec.y - y,
		x - vec.x
	);
}

inline kexVec2 &kexVec2::Cross(const kexVec2 &vec1, const kexVec2 &vec2)
{
	x = vec2.y - vec1.y;
	y = vec1.x - vec2.x;

	return *this;
}

inline kexVec2 kexVec2::Cross(const kexVec3 &vec) const
{
	return kexVec2(
		vec.y - y,
		x - vec.x
	);
}

inline kexVec2 &kexVec2::Cross(const kexVec3 &vec1, const kexVec3 &vec2)
{
	x = vec2.y - vec1.y;
	y = vec1.x - vec2.x;

	return *this;
}

inline float kexVec2::UnitSq() const
{
	return x * x + y * y;
}

inline float kexVec2::Unit() const
{
	return kexMath::Sqrt(UnitSq());
}

inline float kexVec2::DistanceSq(const kexVec2 &vec) const
{
	return (
		(x - vec.x) * (x - vec.x) +
		(y - vec.y) * (y - vec.y)
		);
}

inline float kexVec2::Distance(const kexVec2 &vec) const
{
	return kexMath::Sqrt(DistanceSq(vec));
}

inline kexVec2 &kexVec2::Normalize()
{
	*this *= kexMath::InvSqrt(UnitSq());
	return *this;
}

inline kexVec2 kexVec2::Lerp(const kexVec2 &next, float movement) const
{
	return (next - *this) * movement + *this;
}

inline kexVec2 &kexVec2::Lerp(const kexVec2 &next, const float movement)
{
	*this = (next - *this) * movement + *this;
	return *this;
}

inline kexVec2 &kexVec2::Lerp(const kexVec2 &start, const kexVec2 &next, float movement)
{
	*this = (next - start) * movement + start;
	return *this;
}

inline float kexVec2::ToYaw() const
{
	float d = x * x + y * y;

	if (d == 0.0f)
	{
		return 0.0f;
	}

	return kexMath::ATan2(x, y);
}

/*
inline kexStr kexVec2::ToString() const
{
	kexStr str;
	str = str + x + " " + y;
	return str;
}
*/

inline float *kexVec2::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline kexVec3 kexVec2::ToVec3()
{
	return kexVec3(x, y, 0);
}

inline kexVec2 kexVec2::operator+(const kexVec2 &vec)
{
	return kexVec2(x + vec.x, y + vec.y);
}

inline kexVec2 kexVec2::operator+(const kexVec2 &vec) const
{
	return kexVec2(x + vec.x, y + vec.y);
}

inline kexVec2 kexVec2::operator+(kexVec2 &vec)
{
	return kexVec2(x + vec.x, y + vec.y);
}

inline kexVec2 &kexVec2::operator+=(const kexVec2 &vec)
{
	x += vec.x;
	y += vec.y;
	return *this;
}

inline kexVec2 kexVec2::operator-(const kexVec2 &vec) const
{
	return kexVec2(x - vec.x, y - vec.y);
}

inline kexVec2 kexVec2::operator-() const
{
	return kexVec2(-x, -y);
}

inline kexVec2 &kexVec2::operator-=(const kexVec2 &vec)
{
	x -= vec.x;
	y -= vec.y;
	return *this;
}

inline kexVec2 kexVec2::operator*(const kexVec2 &vec)
{
	return kexVec2(x * vec.x, y * vec.y);
}

inline kexVec2 &kexVec2::operator*=(const kexVec2 &vec)
{
	x *= vec.x;
	y *= vec.y;
	return *this;
}

inline kexVec2 kexVec2::operator*(const float val)
{
	return kexVec2(x * val, y * val);
}

inline kexVec2 kexVec2::operator*(const float val) const
{
	return kexVec2(x * val, y * val);
}

inline kexVec2 &kexVec2::operator*=(const float val)
{
	x *= val;
	y *= val;
	return *this;
}

inline kexVec2 kexVec2::operator/(const kexVec2 &vec)
{
	return kexVec2(x / vec.x, y / vec.y);
}

inline kexVec2 &kexVec2::operator/=(const kexVec2 &vec)
{
	x /= vec.x;
	y /= vec.y;
	return *this;
}

inline kexVec2 kexVec2::operator/(const float val)
{
	return kexVec2(x / val, y / val);
}

inline kexVec2 &kexVec2::operator/=(const float val)
{
	x /= val;
	y /= val;
	return *this;
}

inline kexVec2 kexVec2::operator*(const kexMatrix &mtx)
{
	return kexVec2(mtx.vectors[1].x * y + mtx.vectors[0].x * x + mtx.vectors[3].x, mtx.vectors[1].y * y + mtx.vectors[0].y * x + mtx.vectors[3].y);
}

inline kexVec2 kexVec2::operator*(const kexMatrix &mtx) const
{
	return kexVec2(mtx.vectors[1].x * y + mtx.vectors[0].x * x + mtx.vectors[3].x, mtx.vectors[1].y * y + mtx.vectors[0].y * x + mtx.vectors[3].y);
}

inline kexVec2 &kexVec2::operator*=(const kexMatrix &mtx)
{
	float _x = x;
	float _y = y;

	x = mtx.vectors[1].x * _y + mtx.vectors[0].x * _x + mtx.vectors[3].x;
	y = mtx.vectors[1].y * _y + mtx.vectors[0].y * _x + mtx.vectors[3].y;

	return *this;
}

inline kexVec2 &kexVec2::operator=(const kexVec2 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline kexVec2 &kexVec2::operator=(const kexVec3 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline kexVec2 &kexVec2::operator=(kexVec3 &vec)
{
	x = vec.x;
	y = vec.y;
	return *this;
}

inline kexVec2 &kexVec2::operator=(const float *vecs)
{
	x = vecs[0];
	y = vecs[2];
	return *this;
}

inline float kexVec2::operator[](int index) const
{
	return (&x)[index];
}

inline float &kexVec2::operator[](int index)
{
	return (&x)[index];
}

inline bool kexVec2::operator==(const kexVec2 &vec)
{
	return ((x == vec.x) && (y == vec.y));
}

/////////////////////////////////////////////////////////////////////////////

inline kexVec3::kexVec3()
{
	Clear();
}

inline kexVec3::kexVec3(const float x, const float y, const float z)
{
	Set(x, y, z);
}

inline void kexVec3::Set(const float x, const float y, const float z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

inline void kexVec3::Clear()
{
	x = y = z = 0.0f;
}

inline float kexVec3::Dot(const kexVec3 &vec) const
{
	return (x * vec.x + y * vec.y + z * vec.z);
}

inline float kexVec3::Dot(const kexVec3 &vec1, const kexVec3 &vec2)
{
	return (vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z);
}

inline kexVec3 kexVec3::Cross(const kexVec3 &vec) const
{
	return kexVec3(
		vec.z * y - z * vec.y,
		vec.x * z - x * vec.z,
		x * vec.y - vec.x * y
	);
}

inline kexVec3 kexVec3::Cross(const kexVec3 &vec1, const kexVec3 &vec2)
{
	return vec1.Cross(vec2);
}

inline float kexVec3::UnitSq() const
{
	return x * x + y * y + z * z;
}

inline float kexVec3::Unit() const
{
	return kexMath::Sqrt(UnitSq());
}

inline float kexVec3::DistanceSq(const kexVec3 &vec) const
{
	return (
		(x - vec.x) * (x - vec.x) +
		(y - vec.y) * (y - vec.y) +
		(z - vec.z) * (z - vec.z)
		);
}

inline float kexVec3::Distance(const kexVec3 &vec) const
{
	return kexMath::Sqrt(DistanceSq(vec));
}

inline kexVec3 &kexVec3::Normalize()
{
	*this *= kexMath::InvSqrt(UnitSq());
	return *this;
}

inline kexVec3 kexVec3::Normalize(kexVec3 a)
{
	a.Normalize();
	return a;
}

inline kexAngle kexVec3::PointAt(kexVec3 &location) const
{
	kexVec3 dir = (*this - location).Normalize();

	return kexAngle(dir.ToYaw(), dir.ToPitch(), 0);
}

inline kexVec3 kexVec3::Lerp(const kexVec3 &next, float movement) const
{
	return (next - *this) * movement + *this;
}

inline kexVec3 &kexVec3::Lerp(const kexVec3 &start, const kexVec3 &next, float movement)
{
	*this = (next - start) * movement + start;
	return *this;
}

inline kexQuat kexVec3::ToQuat()
{
	kexVec3 scv = *this * kexMath::InvSqrt(UnitSq());
	return kexQuat(kexMath::ACos(scv.z), vecForward.Cross(scv).Normalize());
}

inline float kexVec3::ToYaw() const
{
	float d = x * x + z * z;

	if (d == 0.0f)
	{
		return 0.0f;
	}

	return kexMath::ATan2(x, z);
}

inline float kexVec3::ToPitch() const
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

	return kexMath::ATan2(y, d);
}

/*
inline kexStr kexVec3::ToString() const
{
	kexStr str;
	str = str + x + " " + y + " " + z;
	return str;
}
*/

inline float *kexVec3::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline kexVec2 kexVec3::ToVec2()
{
	return kexVec2(x, y);
}

inline kexVec2 kexVec3::ToVec2() const
{
	return kexVec2(x, y);
}

inline kexVec3 kexVec3::ScreenProject(kexMatrix &proj, kexMatrix &model, const int width, const int height, const int wx, const int wy)
{
	kexVec4 projVec;
	kexVec4 modelVec;

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

		return kexVec3(
			(projVec.x * 0.5f + 0.5f) * width + wx,
			(-projVec.y * 0.5f + 0.5f) * height + wy,
			(1.0f + projVec.z) * 0.5f);
	}

	return kexVec3(*this);
}

inline kexVec3 kexVec3::operator+(const kexVec3 &vec)
{
	return kexVec3(x + vec.x, y + vec.y, z + vec.z);
}

inline kexVec3 kexVec3::operator+(const kexVec3 &vec) const
{
	return kexVec3(x + vec.x, y + vec.y, z + vec.z);
}

inline kexVec3 kexVec3::operator+(const float val) const
{
	return kexVec3(x + val, y + val, z + val);
}

inline kexVec3 kexVec3::operator+(kexVec3 &vec)
{
	return kexVec3(x + vec.x, y + vec.y, z + vec.z);
}

inline kexVec3 &kexVec3::operator+=(const kexVec3 &vec)
{
	x += vec.x;
	y += vec.y;
	z += vec.z;
	return *this;
}

inline kexVec3 &kexVec3::operator+=(const float val)
{
	x += val;
	y += val;
	z += val;
	return *this;
}

inline kexVec3 kexVec3::operator-(const kexVec3 &vec) const
{
	return kexVec3(x - vec.x, y - vec.y, z - vec.z);
}

inline kexVec3 kexVec3::operator-(const float val) const
{
	return kexVec3(x - val, y - val, z - val);
}

inline kexVec3 kexVec3::operator-() const
{
	return kexVec3(-x, -y, -z);
}

inline kexVec3 &kexVec3::operator-=(const kexVec3 &vec)
{
	x -= vec.x;
	y -= vec.y;
	z -= vec.z;
	return *this;
}

inline kexVec3 &kexVec3::operator-=(const float val)
{
	x -= val;
	y -= val;
	z -= val;
	return *this;
}

inline kexVec3 kexVec3::operator*(const kexVec3 &vec)
{
	return kexVec3(x * vec.x, y * vec.y, z * vec.z);
}

inline kexVec3 &kexVec3::operator*=(const kexVec3 &vec)
{
	x *= vec.x;
	y *= vec.y;
	z *= vec.z;
	return *this;
}

inline kexVec3 kexVec3::operator*(const float val)
{
	return kexVec3(x * val, y * val, z * val);
}

inline kexVec3 kexVec3::operator*(const float val) const
{
	return kexVec3(x * val, y * val, z * val);
}

inline kexVec3 &kexVec3::operator*=(const float val)
{
	x *= val;
	y *= val;
	z *= val;
	return *this;
}

inline kexVec3 kexVec3::operator/(const kexVec3 &vec)
{
	return kexVec3(x / vec.x, y / vec.y, z / vec.z);
}

inline kexVec3 &kexVec3::operator/=(const kexVec3 &vec)
{
	x /= vec.x;
	y /= vec.y;
	z /= vec.z;
	return *this;
}

inline kexVec3 kexVec3::operator/(const float val)
{
	return kexVec3(x / val, y / val, z / val);
}

inline kexVec3 &kexVec3::operator/=(const float val)
{
	x /= val;
	y /= val;
	z /= val;
	return *this;
}

inline kexVec3 kexVec3::operator*(const kexQuat &quat)
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

	return kexVec3(
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

inline kexVec3 kexVec3::operator*(const kexMatrix &mtx)
{
	return kexVec3(mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z);
}

inline kexVec3 kexVec3::operator*(const kexMatrix &mtx) const
{
	return kexVec3(mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z);
}

inline kexVec3 &kexVec3::operator*=(const kexMatrix &mtx)
{
	float _x = x;
	float _y = y;
	float _z = z;

	x = mtx.vectors[1].x * _y + mtx.vectors[2].x * _z + mtx.vectors[0].x * _x + mtx.vectors[3].x;
	y = mtx.vectors[1].y * _y + mtx.vectors[2].y * _z + mtx.vectors[0].y * _x + mtx.vectors[3].y;
	z = mtx.vectors[1].z * _y + mtx.vectors[2].z * _z + mtx.vectors[0].z * _x + mtx.vectors[3].z;

	return *this;
}

inline kexVec3 &kexVec3::operator*=(const kexQuat &quat)
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

inline kexVec3 &kexVec3::operator=(const kexVec3 &vec)
{
	x = vec.x;
	y = vec.y;
	z = vec.z;
	return *this;
}

inline kexVec3 &kexVec3::operator=(const float *vecs)
{
	x = vecs[0];
	y = vecs[1];
	z = vecs[2];
	return *this;
}

inline float kexVec3::operator[](int index) const
{
	return (&x)[index];
}

inline float &kexVec3::operator[](int index)
{
	return (&x)[index];
}

/////////////////////////////////////////////////////////////////////////////

inline kexVec4::kexVec4()
{
	Clear();
}

inline kexVec4::kexVec4(const float x, const float y, const float z, const float w)
{
	Set(x, y, z, w);
}

inline kexVec4::kexVec4(const kexVec3 &v, const float w)
{
	Set(v.x, v.y, v.z, w);
}

inline void kexVec4::Set(const float x, const float y, const float z, const float w)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

inline void kexVec4::Clear()
{
	x = y = z = w = 0.0f;
}

inline kexVec3 const &kexVec4::ToVec3() const
{
	return *reinterpret_cast<const kexVec3*>(this);
}

inline kexVec3 &kexVec4::ToVec3()
{
	return *reinterpret_cast<kexVec3*>(this);
}

inline float *kexVec4::ToFloatPtr()
{
	return reinterpret_cast<float*>(&x);
}

inline kexVec4 kexVec4::operator+(const kexVec4 &vec) const
{
	return kexVec4(x + vec.x, y + vec.y, z + vec.z, w + vec.w);
}

inline kexVec4 kexVec4::operator+(const float val) const
{
	return kexVec4(x + val, y + val, z + val, w + val);
}

inline kexVec4 kexVec4::operator-(const kexVec4 &vec) const
{
	return kexVec4(x - vec.x, y - vec.y, z - vec.z, w - vec.w);
}

inline kexVec4 kexVec4::operator-(const float val) const
{
	return kexVec4(x - val, y - val, z - val, w - val);
}

inline kexVec4 kexVec4::operator*(const kexVec4 &vec) const
{
	return kexVec4(x * vec.x, y * vec.y, z * vec.z, w * vec.w);
}

inline kexVec4 kexVec4::operator*(const float val) const
{
	return kexVec4(x * val, y * val, z * val, w * val);
}

inline kexVec4 kexVec4::operator/(const kexVec4 &vec) const
{
	return kexVec4(x / vec.x, y / vec.y, z / vec.z, w / vec.w);
}

inline kexVec4 kexVec4::operator/(const float val) const
{
	return kexVec4(x / val, y / val, z / val, w / val);
}

inline kexVec4 &kexVec4::operator+=(const kexVec4 &vec)
{
	x += vec.x; y += vec.y; z += vec.z; w += vec.w;
	return *this;
}

inline kexVec4 &kexVec4::operator+=(const float val)
{
	x += val; y += val; z += val; w += val;
	return *this;
}

inline kexVec4 &kexVec4::operator-=(const kexVec4 &vec)
{
	x -= vec.x; y -= vec.y; z -= vec.z; w -= vec.w;
	return *this;
}

inline kexVec4 &kexVec4::operator-=(const float val)
{
	x -= val; y -= val; z -= val; w -= val;
	return *this;
}

inline kexVec4 &kexVec4::operator*=(const kexVec4 &vec)
{
	x *= vec.x; y *= vec.y; z *= vec.z; w *= vec.w;
	return *this;
}

inline kexVec4 &kexVec4::operator*=(const float val)
{
	x *= val; y *= val; z *= val; w *= val;
	return *this;
}

inline kexVec4 &kexVec4::operator/=(const kexVec4 &vec)
{
	x /= vec.x; y /= vec.y; z /= vec.z; w /= vec.w;
	return *this;
}

inline kexVec4 &kexVec4::operator/=(const float val)
{
	x /= val; y /= val; z /= val; w /= val;
	return *this;
}

inline kexVec4 kexVec4::operator|(const kexMatrix &mtx)
{
	return kexVec4(
		mtx.vectors[1].x * y + mtx.vectors[2].x * z + mtx.vectors[0].x * x + mtx.vectors[3].x,
		mtx.vectors[1].y * y + mtx.vectors[2].y * z + mtx.vectors[0].y * x + mtx.vectors[3].y,
		mtx.vectors[1].z * y + mtx.vectors[2].z * z + mtx.vectors[0].z * x + mtx.vectors[3].z,
		mtx.vectors[1].w * y + mtx.vectors[2].w * z + mtx.vectors[0].w * x + mtx.vectors[3].w);
}

inline kexVec4 &kexVec4::operator|=(const kexMatrix &mtx)
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

inline float kexVec4::operator[](int index) const
{
	return (&x)[index];
}

inline float &kexVec4::operator[](int index)
{
	return (&x)[index];
}
