#include "surfaceclip.h"

typedef DelauneyTriangulator::Vertex DTVertex;

inline bool PointOnSide(const vec2& p, const DTVertex& v1, const DTVertex& v2, float tolerance)
{
	vec2 p2 = p - normalize(vec2(-(v2.y - v1.y), v2.x - v1.x)) * tolerance;
	return (p2.y - v1.y) * (v2.x - v1.x) + (v1.x - p2.x) * (v2.y - v1.y) <= 0;
}

SurfaceClip::SurfaceClip(Surface* surface)
{
	sampleWidth = float(surface->lightmapDims[0]);
	sampleHeight = float(surface->lightmapDims[1]);

	// Transformation matrix
	mat3 base;
	base[0] = surface->lightmapSteps[0].x;
	base[1] = surface->lightmapSteps[0].y;
	base[2] = surface->lightmapSteps[0].z;
	base[3] = surface->lightmapSteps[1].x;
	base[4] = surface->lightmapSteps[1].y;
	base[5] = surface->lightmapSteps[1].z;
	base[6] = surface->plane.a;
	base[7] = surface->plane.b;
	base[8] = surface->plane.c;

	mat3 inverseProjection = mat3::inverse(base);

	// Transform vertices to XY and triangulate
	triangulator.vertices.reserve(surface->verts.size());

	for (const auto& vertex : surface->verts)
	{
		auto flattenedVertex = inverseProjection * vertex;

		triangulator.vertices.emplace_back(flattenedVertex.x, flattenedVertex.y, nullptr);

		if (triangulator.vertices.empty())
		{
			bounds = BBox(flattenedVertex, flattenedVertex);
		}
		else
		{
			bounds.AddPoint(flattenedVertex);
		}
	}

	triangulator.triangulate();

	// Init misc. variables
	boundsWidth = bounds.max.x - bounds.min.x;
	boundsHeight = bounds.max.y - bounds.min.y;

	offsetW = boundsWidth / sampleWidth;
	offsetH = boundsHeight / sampleHeight;

	tolerance = (offsetH > offsetW ? offsetH : offsetW) * 2.0f;
}

bool SurfaceClip::SampleIsInBounds(float x, float y) const
{
	const vec2 p = vec2((x / float(sampleWidth)) * boundsWidth + bounds.min.x + offsetW, (y / float(sampleHeight)) * boundsHeight + bounds.min.y + offsetH);

	for (const auto& triangle : triangulator.triangles)
	{
		if (PointOnSide(p, *triangle.A, *triangle.B, tolerance)
			&& PointOnSide(p, *triangle.B, *triangle.C, tolerance)
			&& PointOnSide(p, *triangle.C, *triangle.A, tolerance))
		{
			return true;
		}
	}
	return false;
}