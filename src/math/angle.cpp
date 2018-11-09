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

#define FULLCIRCLE  (M_PI * 2)

Angle::Angle()
{
	this->yaw = 0;
	this->pitch = 0;
	this->roll = 0;
}

Angle::Angle(const float yaw, const float pitch, const float roll)
{
	this->yaw = yaw;
	this->pitch = pitch;
	this->roll = roll;
}

Angle::Angle(const Vec3 &vector)
{
	this->yaw = vector.x;
	this->pitch = vector.y;
	this->roll = vector.z;

	Clamp180();
}

Angle::Angle(const Angle &an)
{
	this->yaw = an.yaw;
	this->pitch = an.pitch;
	this->roll = an.roll;
}

Angle &Angle::Clamp180()
{
#define CLAMP180(x)                                             \
    if(x < -M_PI) for(; x < -M_PI; x = x + FULLCIRCLE);         \
    if(x >  M_PI) for(; x >  M_PI; x = x - FULLCIRCLE)
	CLAMP180(yaw);
	CLAMP180(pitch);
	CLAMP180(roll);
#undef CLAMP180

	return *this;
}

Angle &Angle::Clamp180Invert()
{
#define CLAMP180(x)                                             \
    for(; x < -M_PI; x = x + FULLCIRCLE);                       \
    for(; x >  M_PI; x = x - FULLCIRCLE)
	CLAMP180(yaw);
	CLAMP180(pitch);
	CLAMP180(roll);
#undef CLAMP180

	yaw = -yaw;
	pitch = -pitch;
	roll = -roll;

	return *this;
}

Angle &Angle::Clamp180InvertSum(const Angle &angle)
{
	Angle an = angle;

	an.Clamp180Invert();

	an.yaw += this->yaw;
	an.pitch += this->pitch;
	an.roll += this->roll;

	an.Clamp180Invert();

	this->yaw = an.yaw;
	this->pitch = an.pitch;
	this->roll = an.roll;

	return *this;
}

Angle &Angle::Round()
{
#define ROUND(x)                                        \
    x = DEG2RAD((360.0f / 65536.0f) *                   \
    ((int)(RAD2DEG(x) * (65536.0f / 360.0f)) & 65535))
	yaw = ROUND(yaw);
	pitch = ROUND(pitch);
	roll = ROUND(roll);
#undef ROUND

	return Clamp180();
}

Angle Angle::Diff(Angle &angle)
{
	float an;
	Angle out;

	Clamp180();
	angle.Clamp180();

#define DIFF(x)                     \
    if(x <= angle.x) {              \
        an = angle.x + FULLCIRCLE;  \
        if(x - angle.x > an - x) {  \
            out.x = x - an;         \
        }                           \
        else {                      \
            out.x = x - angle.x;    \
        }                           \
    }                               \
    else {                          \
        an = angle.x - FULLCIRCLE;  \
        if(angle.x - x <= x - an) { \
            out.x = x - angle.x;    \
        }                           \
        else {                      \
            out.x = x - an;         \
        }                           \
    }
	DIFF(yaw);
	DIFF(pitch);
	DIFF(roll);
#undef DIFF

	return out;
}

void Angle::ToAxis(Vec3 *forward, Vec3 *up, Vec3 *right)
{
	float sy = Math::Sin(yaw);
	float cy = Math::Cos(yaw);
	float sp = Math::Sin(pitch);
	float cp = Math::Cos(pitch);
	float sr = Math::Sin(roll);
	float cr = Math::Cos(roll);

	if (forward)
	{
		forward->x = sy * cp;
		forward->y = sp;
		forward->z = cy * cp;
	}
	if (right)
	{
		right->x = sr * sp * sy + cr * cy;
		right->y = sr * cp;
		right->z = sr * sp * cy + cr * -sy;
	}
	if (up)
	{
		up->x = cr * sp * sy + -sr * cy;
		up->y = cr * cp;
		up->z = cr * sp * cy + -sr * -sy;
	}
}

Vec3 Angle::ToForwardAxis()
{
	Vec3 vec;

	ToAxis(&vec, nullptr, nullptr);
	return vec;
}

Vec3 Angle::ToUpAxis()
{
	Vec3 vec;

	ToAxis(nullptr, &vec, nullptr);
	return vec;
}

Vec3 Angle::ToRightAxis()
{
	Vec3 vec;

	ToAxis(nullptr, nullptr, &vec);
	return vec;
}

const Vec3 &Angle::ToVec3() const
{
	return *reinterpret_cast<const Vec3*>(&yaw);
}

Vec3 &Angle::ToVec3()
{
	return *reinterpret_cast<Vec3*>(&yaw);
}

Quat Angle::ToQuat()
{
	return
		(Quat(pitch, Vec3::vecRight) *
		(Quat(yaw, Vec3::vecUp) *
			Quat(roll, Vec3::vecForward)));
}

Angle Angle::operator+(const Angle &angle)
{
	return Angle(yaw + angle.yaw, pitch + angle.pitch, roll + angle.roll);
}

Angle Angle::operator-(const Angle &angle)
{
	return Angle(yaw - angle.yaw, pitch - angle.pitch, roll - angle.roll);
}

Angle Angle::operator-()
{
	return Angle(-yaw, -pitch, -roll);
}

Angle &Angle::operator+=(const Angle &angle)
{
	yaw += angle.yaw;
	pitch += angle.pitch;
	roll += angle.roll;
	return *this;
}

Angle &Angle::operator-=(const Angle &angle)
{
	yaw -= angle.yaw;
	pitch -= angle.pitch;
	roll -= angle.roll;
	return *this;
}

Angle &Angle::operator=(const Angle &angle)
{
	yaw = angle.yaw;
	pitch = angle.pitch;
	roll = angle.roll;
	return *this;
}

Angle &Angle::operator=(const Vec3 &vector)
{
	yaw = vector.x;
	pitch = vector.y;
	roll = vector.z;
	return *this;
}

Angle &Angle::operator=(const float *vecs)
{
	yaw = vecs[0];
	pitch = vecs[1];
	roll = vecs[2];
	return *this;
}

float Angle::operator[](int index) const
{
	assert(index >= 0 && index < 3);
	return (&yaw)[index];
}

float &Angle::operator[](int index)
{
	assert(index >= 0 && index < 3);
	return (&yaw)[index];
}
