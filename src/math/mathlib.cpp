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

#include "mathlib.h"

int Math::RoundPowerOfTwo(int x)
{
	int mask = 1;

	while (mask < 0x40000000)
	{
		if (x == mask || (x & (mask - 1)) == x)
		{
			return mask;
		}

		mask <<= 1;
	}

	return x;
}

void Math::CubicCurve(const vec3 &start, const vec3 &end, const float time, const vec3 &point, vec3 *vec)
{
	int i;
	float xyz[3];

	for (i = 0; i < 3; i++)
	{
		xyz[i] = Math::Pow(1 - time, 2) * start[i] +
			(2 * (1 - time)) * time * point[i] + Math::Pow(time, 2) * end[i];
	}

	vec->x = xyz[0];
	vec->y = xyz[1];
	vec->z = xyz[2];
}

void Math::QuadraticCurve(const vec3 &start, const vec3 &end, const float time, const vec3 &pt1, const vec3 &pt2, vec3 *vec)
{
	int i;
	float xyz[3];

	for (i = 0; i < 3; i++)
	{
		xyz[i] = Math::Pow(1 - time, 3) * start[i] + (3 * Math::Pow(1 - time, 2)) *
			time * pt1[i] + (3 * (1 - time)) * Math::Pow(time, 2) * pt2[i] +
			Math::Pow(time, 3) * end[i];
	}

	vec->x = xyz[0];
	vec->y = xyz[1];
	vec->z = xyz[2];
}

