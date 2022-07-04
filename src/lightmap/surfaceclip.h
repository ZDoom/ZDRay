#pragma once

#include "lightmap/levelmesh.h"
#include "math/mathlib.h"
#include "delauneytriangulator.h"

class SurfaceClip
{
	DelauneyTriangulator triangulator;

	float sampleWidth;
	float sampleHeight;

	BBox bounds;
	float boundsWidth;
	float boundsHeight;
	float offsetW;
	float offsetH;
	float tolerance;

public:
	SurfaceClip(Surface* surface);

	// Tolerates points close enough to the surface to avoid missing used samples
	bool SampleIsInBounds(float x, float y) const;
};