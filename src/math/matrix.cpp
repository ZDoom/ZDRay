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
// DESCRIPTION: Matrix (left handed) operations
//
// Reference:
// _________________
// | 0   4   8   12 |
// | 1   5   9   13 |
// | 2   6  10   14 |
// | 3   7  11   15 |
// _________________
//
// translation
// _________________
// | 0   4   8   x  |
// | 1   5   9   y  |
// | 2   6  10   z  |
// | 3   7  11   15 |
// _________________
//
// rotation x
// _________________
// |(1)  4   8   x  |
// | 1   xc -xs  y  |
// | 2   xs  xs  z  |
// | 3   7  11  (1) |
// _________________
//
// rotation y
// _________________
// | yc  4  ys   12 |
// | 1  (1)  9   13 |
// |-ys  6  yc   14 |
// | 3   7  11  (1) |
// _________________
//
// rotation z
// _________________
// | zc -zs  8   12 |
// | zs  zc  9   13 |
// | 2   6  (1)  14 |
// | 3   7  11  (1) |
// _________________
//
//-----------------------------------------------------------------------------

#include <math.h>
#include "mathlib.h"

Mat4::Mat4()
{
	Identity();
}

Mat4::Mat4(const Mat4 &mtx)
{
	for (int i = 0; i < 4; i++)
	{
		vectors[i].x = mtx.vectors[i].x;
		vectors[i].y = mtx.vectors[i].y;
		vectors[i].z = mtx.vectors[i].z;
		vectors[i].w = mtx.vectors[i].w;
	}
}

Mat4::Mat4(const float x, const float y, const float z)
{
	Identity(x, y, z);
}

Mat4::Mat4(const Quat &quat)
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

	vectors[0].Set(
		((ww + xx) - yy) - zz,
		(wz + wz) + (yx + yx),
		(zx + zx) - (wy + wy),
		0);
	vectors[1].Set(
		(yx + yx) - (wz + wz),
		(yy + (ww - xx)) - zz,
		(wx + wx) + (zy + zy),
		0);
	vectors[2].Set(
		(wy + wy + zx + zx),
		(zy + zy) - (wx + wx),
		((ww - xx) - yy) + zz,
		0);
	vectors[3].Set(0, 0, 0, 1);
}

Mat4::Mat4(const float angle, const int axis)
{
	float s;
	float c;

	s = Math::Sin(angle);
	c = Math::Cos(angle);

	Identity();

	switch (axis)
	{
	case 0:
		this->vectors[0].x = c;
		this->vectors[0].z = -s;
		this->vectors[3].x = s;
		this->vectors[3].z = c;
		break;
	case 1:
		this->vectors[1].y = c;
		this->vectors[1].z = s;
		this->vectors[2].y = -s;
		this->vectors[2].z = c;
		break;
	case 2:
		this->vectors[0].x = c;
		this->vectors[0].y = s;
		this->vectors[1].x = -s;
		this->vectors[1].y = c;
		break;
	}
}

Mat4 &Mat4::Identity()
{
	vectors[0].Set(1, 0, 0, 0);
	vectors[1].Set(0, 1, 0, 0);
	vectors[2].Set(0, 0, 1, 0);
	vectors[3].Set(0, 0, 0, 1);

	return *this;
}

Mat4 &Mat4::Identity(const float x, const float y, const float z)
{
	vectors[0].Set(x, 0, 0, 0);
	vectors[1].Set(0, y, 0, 0);
	vectors[2].Set(0, 0, z, 0);
	vectors[3].Set(0, 0, 0, 1);

	return *this;
}

Mat4 &Mat4::SetTranslation(const float x, const float y, const float z)
{
	vectors[3].ToVec3().Set(x, y, z);
	return *this;
}

Mat4 &Mat4::SetTranslation(const Vec3 &vector)
{
	vectors[3].ToVec3() = vector;
	return *this;
}

Mat4 &Mat4::AddTranslation(const float x, const float y, const float z)
{
	vectors[3].x += x;
	vectors[3].y += y;
	vectors[3].z += z;
	return *this;
}

Mat4 &Mat4::AddTranslation(const Vec3 &vector)
{
	vectors[3].ToVec3() += vector;
	return *this;
}

Mat4 &Mat4::Scale(const float x, const float y, const float z)
{
	vectors[0].ToVec3() *= x;
	vectors[1].ToVec3() *= y;
	vectors[2].ToVec3() *= z;

	return *this;
}

Mat4 &Mat4::Scale(const Vec3 &vector)
{
	vectors[0].ToVec3() *= vector.x;
	vectors[1].ToVec3() *= vector.y;
	vectors[2].ToVec3() *= vector.z;

	return *this;
}

Mat4 Mat4::Scale(const Mat4 &mtx, const float x, const float y, const float z)
{
	Mat4 out;

	out.vectors[0].ToVec3() = mtx.vectors[0].ToVec3() * x;
	out.vectors[1].ToVec3() = mtx.vectors[1].ToVec3() * y;
	out.vectors[2].ToVec3() = mtx.vectors[2].ToVec3() * z;

	return out;
}

Mat4 &Mat4::Transpose()
{
	Vec3 v1 = vectors[1].ToVec3();
	Vec3 v2 = vectors[2].ToVec3();

	vectors[1].ToVec3() = v2;
	vectors[2].ToVec3() = v1;
	return *this;
}

Mat4 Mat4::Transpose(const Mat4 &mtx)
{
	Mat4 out;

	out.vectors[0].ToVec3() = mtx.vectors[0].ToVec3();
	out.vectors[1].ToVec3() = mtx.vectors[2].ToVec3();
	out.vectors[2].ToVec3() = mtx.vectors[1].ToVec3();
	out.vectors[3].ToVec3() = mtx.vectors[3].ToVec3();

	return out;
}

Mat4 Mat4::Invert(Mat4 &mtx)
{
	float d;
	float *m;

	m = mtx.ToFloatPtr();

	d = m[0] * m[10] * m[5] -
		m[0] * m[9] * m[6] -
		m[1] * m[4] * m[10] +
		m[2] * m[4] * m[9] +
		m[1] * m[6] * m[8] -
		m[2] * m[5] * m[8];

	if (d != 0.0f)
	{
		Mat4 inv;

		d = (1.0f / d);

		inv.vectors[0].x = (m[10] * m[5] - m[9] * m[6]) * d;
		inv.vectors[0].y = -((m[1] * m[10] - m[2] * m[9]) * d);
		inv.vectors[0].z = (m[1] * m[6] - m[2] * m[5]) * d;
		inv.vectors[0].w = 0;
		inv.vectors[1].x = (m[6] * m[8] - m[4] * m[10]) * d;
		inv.vectors[1].y = (m[0] * m[10] - m[2] * m[8]) * d;
		inv.vectors[1].z = -((m[0] * m[6] - m[2] * m[4]) * d);
		inv.vectors[1].w = 0;
		inv.vectors[2].x = -((m[5] * m[8] - m[4] * m[9]) * d);
		inv.vectors[2].y = (m[1] * m[8] - m[0] * m[9]) * d;
		inv.vectors[2].z = -((m[1] * m[4] - m[0] * m[5]) * d);
		inv.vectors[2].w = 0;
		inv.vectors[3].x = (
			(m[13] * m[10] - m[14] * m[9]) * m[4]
			+ m[14] * m[5] * m[8]
			- m[13] * m[6] * m[8]
			- m[12] * m[10] * m[5]
			+ m[12] * m[9] * m[6]) * d;
		inv.vectors[3].y = (
			m[0] * m[14] * m[9]
			- m[0] * m[13] * m[10]
			- m[14] * m[1] * m[8]
			+ m[13] * m[2] * m[8]
			+ m[12] * m[1] * m[10]
			- m[12] * m[2] * m[9]) * d;
		inv.vectors[3].z = -(
			(m[0] * m[14] * m[5]
				- m[0] * m[13] * m[6]
				- m[14] * m[1] * m[4]
				+ m[13] * m[2] * m[4]
				+ m[12] * m[1] * m[6]
				- m[12] * m[2] * m[5]) * d);
		inv.vectors[3].w = 1.0f;

		return inv;
	}

	return mtx;
}

void Mat4::SetViewProjection(float aspect, float fov, float zNear, float zFar)
{
	float top = zNear * Math::Tan(fov * M_PI / 360.0f);
	float bottom = -top;
	float left = bottom * aspect;
	float right = top * aspect;

	vectors[0].x = (2 * zNear) / (right - left);
	vectors[1].y = (2 * zNear) / (top - bottom);
	vectors[3].z = -(2 * zFar * zNear) / (zFar - zNear);

	vectors[2].x = (right + left) / (right - left);
	vectors[2].y = (top + bottom) / (top - bottom);
	vectors[2].z = -(zFar + zNear) / (zFar - zNear);

	vectors[0].y = 0;
	vectors[0].z = 0;
	vectors[0].w = 0;
	vectors[1].x = 0;
	vectors[1].w = 0;
	vectors[1].z = 0;
	vectors[2].w = -1;
	vectors[3].x = 0;
	vectors[3].y = 0;
	vectors[3].w = 0;
}

void Mat4::SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar)
{
	vectors[0].x = 2 / (right - left);
	vectors[1].y = 2 / (top - bottom);
	vectors[2].z = -2 / (zFar - zNear);

	vectors[3].x = -(right + left) / (right - left);
	vectors[3].y = -(top + bottom) / (top - bottom);
	vectors[3].z = -(zFar + zNear) / (zFar - zNear);
	vectors[3].w = 1;

	vectors[0].y = 0;
	vectors[0].z = 0;
	vectors[0].w = 0;
	vectors[1].x = 0;
	vectors[1].z = 0;
	vectors[1].w = 0;
	vectors[2].x = 0;
	vectors[2].y = 0;
	vectors[2].w = 0;
}

Quat Mat4::ToQuat()
{
	float t;
	float d;
	float mx;
	float my;
	float mz;
	float m21;
	float m20;
	float m10;
	Quat q;

	mx = vectors[0][0];
	my = vectors[1][1];
	mz = vectors[2][2];

	m21 = (vectors[2][1] - vectors[1][2]);
	m20 = (vectors[2][0] - vectors[0][2]);
	m10 = (vectors[1][0] - vectors[0][1]);

	t = 1.0f + mx + my + mz;

	if (t > 0)
	{
		d = 0.5f / Math::Sqrt(t);
		q.x = m21 * d;
		q.y = m20 * d;
		q.z = m10 * d;
		q.w = 0.25f / d;
	}
	else if (mx > my && mx > mz)
	{
		d = Math::Sqrt(1.0f + mx - my - mz) * 2;
		q.x = 0.5f / d;
		q.y = m10 / d;
		q.z = m20 / d;
		q.w = m21 / d;
	}
	else if (my > mz)
	{
		d = Math::Sqrt(1.0f + my - mx - mz) * 2;
		q.x = m10 / d;
		q.y = 0.5f / d;
		q.z = m21 / d;
		q.w = m20 / d;
	}
	else
	{
		d = Math::Sqrt(1.0f + mz - mx - my) * 2;
		q.x = m20 / d;
		q.y = m21 / d;
		q.z = 0.5f / d;
		q.w = m10 / d;
	}

	q.Normalize();
	return q;
}

Mat4 Mat4::operator*(const Vec3 &vector)
{
	Mat4 out(*this);

	out.vectors[3].ToVec3() +=
		vectors[0].ToVec3() * vector.x +
		vectors[1].ToVec3() * vector.y +
		vectors[2].ToVec3() * vector.z;
	return out;
}

Mat4 &Mat4::operator*=(const Vec3 &vector)
{
	vectors[3].ToVec3() +=
		vectors[0].ToVec3() * vector.x +
		vectors[1].ToVec3() * vector.y +
		vectors[2].ToVec3() * vector.z;
	return *this;
}

float *Mat4::ToFloatPtr()
{
	return reinterpret_cast<float*>(vectors);
}

Mat4 Mat4::operator*(const Mat4 &matrix)
{
	Mat4 out;

	for (int i = 0; i < 4; i++)
	{
		out.vectors[i].x =
			vectors[i].x * matrix.vectors[0].x +
			vectors[i].y * matrix.vectors[1].x +
			vectors[i].z * matrix.vectors[2].x +
			vectors[i].w * matrix.vectors[3].x;
		out.vectors[i].y =
			vectors[i].x * matrix.vectors[0].y +
			vectors[i].y * matrix.vectors[1].y +
			vectors[i].z * matrix.vectors[2].y +
			vectors[i].w * matrix.vectors[3].y;
		out.vectors[i].z =
			vectors[i].x * matrix.vectors[0].z +
			vectors[i].y * matrix.vectors[1].z +
			vectors[i].z * matrix.vectors[2].z +
			vectors[i].w * matrix.vectors[3].z;
		out.vectors[i].w =
			vectors[i].x * matrix.vectors[0].w +
			vectors[i].y * matrix.vectors[1].w +
			vectors[i].z * matrix.vectors[2].w +
			vectors[i].w * matrix.vectors[3].w;
	}

	return out;
}

Mat4 &Mat4::operator*=(const Mat4 &matrix)
{
	for (int i = 0; i < 4; i++)
	{
		vectors[i].x =
			vectors[i].x * matrix.vectors[0].x +
			vectors[i].y * matrix.vectors[1].x +
			vectors[i].z * matrix.vectors[2].x +
			vectors[i].w * matrix.vectors[3].x;
		vectors[i].y =
			vectors[i].x * matrix.vectors[0].y +
			vectors[i].y * matrix.vectors[1].y +
			vectors[i].z * matrix.vectors[2].y +
			vectors[i].w * matrix.vectors[3].y;
		vectors[i].z =
			vectors[i].x * matrix.vectors[0].z +
			vectors[i].y * matrix.vectors[1].z +
			vectors[i].z * matrix.vectors[2].z +
			vectors[i].w * matrix.vectors[3].z;
		vectors[i].w =
			vectors[i].x * matrix.vectors[0].w +
			vectors[i].y * matrix.vectors[1].w +
			vectors[i].z * matrix.vectors[2].w +
			vectors[i].w * matrix.vectors[3].w;
	}

	return *this;
}

Mat4 operator*(const Mat4 &m1, const Mat4 &m2)
{
	Mat4 out;

	for (int i = 0; i < 4; i++)
	{
		out.vectors[i].x =
			m1.vectors[i].x * m2.vectors[0].x +
			m1.vectors[i].y * m2.vectors[1].x +
			m1.vectors[i].z * m2.vectors[2].x +
			m1.vectors[i].w * m2.vectors[3].x;
		out.vectors[i].y =
			m1.vectors[i].x * m2.vectors[0].y +
			m1.vectors[i].y * m2.vectors[1].y +
			m1.vectors[i].z * m2.vectors[2].y +
			m1.vectors[i].w * m2.vectors[3].y;
		out.vectors[i].z =
			m1.vectors[i].x * m2.vectors[0].z +
			m1.vectors[i].y * m2.vectors[1].z +
			m1.vectors[i].z * m2.vectors[2].z +
			m1.vectors[i].w * m2.vectors[3].z;
		out.vectors[i].w =
			m1.vectors[i].x * m2.vectors[0].w +
			m1.vectors[i].y * m2.vectors[1].w +
			m1.vectors[i].z * m2.vectors[2].w +
			m1.vectors[i].w * m2.vectors[3].w;
	}

	return out;
}

Mat4 Mat4::operator|(const Mat4 &matrix)
{
	Mat4 out;

	for (int i = 0; i < 3; i++)
	{
		out.vectors[i].x =
			vectors[i].x * matrix.vectors[0].x +
			vectors[i].y * matrix.vectors[1].x +
			vectors[i].z * matrix.vectors[2].x;
		out.vectors[i].y =
			vectors[i].x * matrix.vectors[0].y +
			vectors[i].y * matrix.vectors[1].y +
			vectors[i].z * matrix.vectors[2].y;
		out.vectors[i].z =
			vectors[i].x * matrix.vectors[0].z +
			vectors[i].y * matrix.vectors[1].z +
			vectors[i].z * matrix.vectors[2].z;
	}

	out.vectors[3].x =
		vectors[3].x * matrix.vectors[0].x +
		vectors[3].y * matrix.vectors[1].x +
		vectors[3].z * matrix.vectors[2].x + matrix.vectors[3].x;
	out.vectors[3].y =
		vectors[3].x * matrix.vectors[0].y +
		vectors[3].y * matrix.vectors[1].y +
		vectors[3].z * matrix.vectors[2].y + matrix.vectors[3].y;
	out.vectors[3].z =
		vectors[3].x * matrix.vectors[0].z +
		vectors[3].y * matrix.vectors[1].z +
		vectors[3].z * matrix.vectors[2].z + matrix.vectors[3].z;

	return out;
}

Mat4 &Mat4::operator=(const Mat4 &matrix)
{
	vectors[0] = matrix.vectors[0];
	vectors[1] = matrix.vectors[1];
	vectors[2] = matrix.vectors[2];
	vectors[3] = matrix.vectors[3];

	return *this;
}

Mat4 &Mat4::operator=(const float *m)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			vectors[i][j] = m[i * 4 + j];
		}
	}

	return *this;
}
