#include "surfaceclip.h"

inline bool PointOnSide(const vec2& p, const vec2& v1, const vec2& v2, float tolerance)
{
	vec2 p2 = p + normalize(vec2(-(v2.y - v1.y), v2.x - v1.x)) * tolerance;
	return (p2.y - v1.y) * (v2.x - v1.x) + (v1.x - p2.x) * (v2.y - v1.y) >= 0;
}


inline bool PointBeyondSide(const vec2& p, const vec2& v1, const vec2& v2)
{
	vec2 p2 = p - normalize(vec2(-(v2.y - v1.y), v2.x - v1.x)); // What a hack!
	return (p2.y - v1.y) * (v2.x - v1.x) + (v1.x - p2.x) * (v2.y - v1.y) < 0;
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
	vertices.reserve(surface->verts.size());

	for (const auto& vertex : surface->verts)
	{
		auto flattenedVertex = inverseProjection * vertex;

		vertices.emplace_back(flattenedVertex.x, flattenedVertex.y);

		if (vertices.empty())
		{
			bounds = BBox(flattenedVertex, flattenedVertex);
		}
		else
		{
			bounds.AddPoint(flattenedVertex);
		}
	}

	// Walls have "Z" like pattern for vertices
	if (surface->type != ST_CEILING && surface->type != ST_FLOOR)
	{
		if (vertices.size() == 4)
		{
			std::swap(vertices[vertices.size() - 2], vertices[vertices.size() - 1]);
		}
	}

	auto isConvex = [&]() {
		for (size_t i = 2; i < vertices.size(); ++i)
		{
			if (!PointBeyondSide(vertices[i - 1], vertices[i - 2], vertices[i]))
			{
				return false;
			}
		}
		return PointBeyondSide(vertices[vertices.size() - 1], vertices[vertices.size() - 2], vertices[0]) && PointBeyondSide(vertices[0], vertices[vertices.size() - 1], vertices[1]);
	};

	// Fix vertex order
	if (!isConvex())
	{
		for (size_t i = 0; i < vertices.size() / 2; ++i)
		{
			std::swap(vertices[i], vertices[vertices.size() - 1 - i]);
		}
	}

	// Init misc. variables
	boundsWidth = bounds.max.x - bounds.min.x;
	boundsHeight = bounds.max.y - bounds.min.y;

	offsetW = boundsWidth / sampleWidth;
	offsetH = boundsHeight / sampleHeight;

	tolerance = (offsetH > offsetW ? offsetH : offsetW) * 2.0f;
}

bool SurfaceClip::PointInBounds(const vec2& p, float tolerance) const
{
	for (size_t i = 1; i < vertices.size(); ++i)
	{
		if (!PointOnSide(p, vertices[i - 1], vertices[i], tolerance))
		{
			return false;
		}
	}
	return PointOnSide(p, vertices[vertices.size() - 1], vertices[0], tolerance);
}

bool SurfaceClip::SampleIsInBounds(float x, float y) const
{
	return PointInBounds(vec2((x / float(sampleWidth)) * boundsWidth + bounds.min.x + offsetW, (y / float(sampleHeight)) * boundsHeight + bounds.min.y + offsetH), tolerance);
}