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

#include "surfaces.h"

struct FLevel;
struct surfaceLightDef;
class kexTrace;

class kexLightSurface
{
public:
    kexLightSurface();
    ~kexLightSurface();

    void                    Init(const surfaceLightDef &lightSurfaceDef, surface_t *surface, const bool bWall, const bool bNoCenterPoint);
    void                    Subdivide(const float divide);
    void                    CreateCenterOrigin();
    bool                    TraceSurface(FLevel *doomMap, kexTrace &trace, const surface_t *surface, const kexVec3 &origin, float *dist);

    const float             OuterCone() const { return outerCone; }
    const float             InnerCone() const { return innerCone; }
    const float             FallOff() const { return falloff; }
    const float             Distance() const { return distance; }
    const float             Intensity() const { return intensity; }
    const kexVec3           GetRGB() const { return rgb; }
    const bool              IsAWall() const { return bWall; }
    const bool              NoCenterPoint() const { return bNoCenterPoint; }
    const surface_t         *Surface() const { return surface; }
    const vertexBatch_t     Origins() const { return origins; }

private:
    bool                    SubdivideRecursion(vertexBatch_t &surfPoints, float divide, kexArray<vertexBatch_t*> &points);
    void                    Clip(vertexBatch_t &points, const kexVec3 &normal, float dist, vertexBatch_t *frontPoints, vertexBatch_t *backPoints);

    float                   outerCone;
    float                   innerCone;
    float                   falloff;
    float                   distance;
    float                   intensity;
    kexVec3                 rgb;
    bool                    bWall;
    bool                    bNoCenterPoint;
    vertexBatch_t           origins;
    surface_t               *surface;
};
