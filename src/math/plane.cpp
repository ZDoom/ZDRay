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

Plane::Plane()
{
	this->a = 0;
	this->b = 0;
	this->c = 0;
	this->d = 0;
}

Plane::Plane(const float a, const float b, const float c, const float d)
{
	this->a = a;
	this->b = b;
	this->c = c;
	this->d = d;
}

Plane::Plane(const vec3 &pt1, const vec3 &pt2, const vec3 &pt3)
{
	SetNormal(pt1, pt2, pt3);
	this->d = dot(pt1, Normal());
}

Plane::Plane(const vec3 &normal, const vec3 &point)
{
	this->a = normal.x;
	this->b = normal.y;
	this->c = normal.z;
	this->d = dot(point, normal);
}

Plane::Plane(const Plane &plane)
{
	this->a = plane.a;
	this->b = plane.b;
	this->c = plane.c;
	this->d = plane.d;
}

void Plane::Set(float a, float b, float c, float d)
{
	this->a = a;
	this->b = b;
	this->c = c;
	this->d = d;
}

Plane &Plane::SetNormal(const vec3 &normal)
{
	Normal() = normal;
	return *this;
}

Plane &Plane::SetNormal(const vec3 &pt1, const vec3 &pt2, const vec3 &pt3)
{
	Normal() = normalize(cross(pt2 - pt1, pt3 - pt2));
	return *this;
}

Plane& Plane::SetNormal(const vec3& v1, const vec3& v2, const vec3& v3, const vec3& v4)
{
	// Handle cases where two vertices share the same position

	// TODO fix this hack
	// Certain vertex positions are close enough to cause NaN, but aren't equal
	auto iv1 = ivec3(v1);
	auto iv2 = ivec3(v2);
	auto iv3 = ivec3(v3);
	auto iv4 = ivec3(v4);

	if (iv1 == iv3)
	{
		return SetNormal(v1, v2, v4);
	}
	else if (iv2 == iv4)
	{
		return SetNormal(v1, v2, v3);
	}
	else if (iv1 == iv2)
	{
		return SetNormal(v2, v3, v4);
	}
	else if (iv2 == iv3)
	{
		return SetNormal(v1, v2, v4);
	}

	return SetNormal(v1, v2, v3);
}

vec3 const &Plane::Normal() const
{
	return *reinterpret_cast<const vec3*>(&a);
}

vec3 &Plane::Normal()
{
	return *reinterpret_cast<vec3*>(&a);
}

float Plane::Distance(const vec3 &point)
{
	return dot(point, Normal());
}

Plane &Plane::SetDistance(const vec3 &point)
{
	this->d = dot(point, Normal());
	return *this;
}

bool Plane::IsFacing(const float yaw)
{
	return -Math::Sin(yaw) * a + -Math::Cos(yaw) * b < 0;
}

float Plane::ToYaw()
{
	float d = length(Normal());

	if (d != 0)
	{
		float phi;
		phi = Math::ACos(b / d);
		if (a <= 0)
		{
			phi = -phi;
		}

		return phi;
	}

	return 0;
}

float Plane::ToPitch()
{
	return Math::ACos(dot(vec3(0.0f, 0.0f, 1.0f), Normal()));
}

vec4 const &Plane::ToVec4() const
{
	return *reinterpret_cast<const vec4*>(&a);
}

vec4 &Plane::ToVec4()
{
	return *reinterpret_cast<vec4*>(&a);
}

const Plane::PlaneAxis Plane::BestAxis() const
{
	float na = Math::Fabs(a);
	float nb = Math::Fabs(b);
	float nc = Math::Fabs(c);

	// figure out what axis the plane lies on
	if (na >= nb && na >= nc)
	{
		return AXIS_YZ;
	}
	else if (nb >= na && nb >= nc)
	{
		return AXIS_XZ;
	}

	return AXIS_XY;
}

vec3 Plane::GetInclination()
{
	vec3 dir = Normal() * dot(vec3(0.0f, 0.0f, 1.0f), Normal());
	return normalize(vec3(0.0f, 0.0f, 1.0f) - dir);
}
