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
// DESCRIPTION: General class module for handling ray tracing of the
//              world geometry. Ideally, all of this needs to be revisited...
//
//-----------------------------------------------------------------------------

#include "common.h"
#include "mapdata.h"
#include "trace.h"

void kexTrace::Init(FLevel &doomMap)
{
	map = &doomMap;
}

void kexTrace::Trace(const kexVec3 &startVec, const kexVec3 &endVec)
{
	start = startVec;
	end = endVec;

	TraceHit hit = TriangleMeshShape::find_first_hit(map->CollisionMesh.get(), start, end);
	fraction = hit.fraction;
	if (fraction < 1.0f)
		hitSurface = surfaces[hit.surface];
	else
		hitSurface = nullptr;
}
