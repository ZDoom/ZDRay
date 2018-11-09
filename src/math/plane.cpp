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
// DESCRIPTION: Plane operations
//
//-----------------------------------------------------------------------------

#include <math.h>
#include "mathlib.h"

//
// Plane::Plane
//

Plane::Plane()
{
	this->a = 0;
	this->b = 0;
	this->c = 0;
	this->d = 0;
}

//
// Plane::Plane
//

Plane::Plane(const float a, const float b, const float c, const float d)
{
	this->a = a;
	this->b = b;
	this->c = c;
	this->d = d;
}

//
// Plane::Plane
//

Plane::Plane(const Vec3 &pt1, const Vec3 &pt2, const Vec3 &pt3)
{
	SetNormal(pt1, pt2, pt3);
	this->d = Vec3::Dot(pt1, Normal());
}

//
// Plane::Plane
//

Plane::Plane(const Vec3 &normal, const Vec3 &point)
{
	this->a = normal.x;
	this->b = normal.y;
	this->c = normal.z;
	this->d = point.Dot(normal);
}

//
// Plane::Plane
//

Plane::Plane(const Plane &plane)
{
	this->a = plane.a;
	this->b = plane.b;
	this->c = plane.c;
	this->d = plane.d;
}

//
// Plane::SetNormal
//

Plane &Plane::SetNormal(const Vec3 &normal)
{
	Normal() = normal;
	return *this;
}

//
// Plane::SetNormal
//

Plane &Plane::SetNormal(const Vec3 &pt1, const Vec3 &pt2, const Vec3 &pt3)
{
	Normal() = (pt2 - pt1).Cross(pt3 - pt2).Normalize();
	return *this;
}

//
// Plane::Normal
//

Vec3 const &Plane::Normal() const
{
	return *reinterpret_cast<const Vec3*>(&a);
}

//
// Plane::Normal
//

Vec3 &Plane::Normal()
{
	return *reinterpret_cast<Vec3*>(&a);
}

//
// Plane::Distance
//

float Plane::Distance(const Vec3 &point)
{
	return point.Dot(Normal());
}

//
// Plane::SetDistance
//

Plane &Plane::SetDistance(const Vec3 &point)
{
	this->d = point.Dot(Normal());
	return *this;
}

//
// Plane::IsFacing
//

bool Plane::IsFacing(const float yaw)
{
	return -Math::Sin(yaw) * a + -Math::Cos(yaw) * b < 0;
}

//
// Plane::ToYaw
//

float Plane::ToYaw()
{
	float d = Normal().Unit();

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

//
// Plane::ToPitch
//

float Plane::ToPitch()
{
	return Math::ACos(Vec3::vecUp.Dot(Normal()));
}

//
// Plane::ToQuat
//

Quat Plane::ToQuat()
{
	Vec3 cross = Vec3::vecUp.Cross(Normal()).Normalize();
	return Quat(Math::ACos(Vec3::vecUp.Dot(Normal())), cross);
}

//
// Plane::ToVec4
//

Vec4 const &Plane::ToVec4() const
{
	return *reinterpret_cast<const Vec4*>(&a);
}

//
// Plane::ToVec4
//

Vec4 &Plane::ToVec4()
{
	return *reinterpret_cast<Vec4*>(&a);
}

//
// Plane::BestAxis
//

const Plane::planeAxis_t Plane::BestAxis() const
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

//
// Plane::GetInclination
//

Vec3 Plane::GetInclination()
{
	Vec3 dir = Normal() * Vec3::vecUp.Dot(Normal());
	return (Vec3::vecUp - dir).Normalize();
}
