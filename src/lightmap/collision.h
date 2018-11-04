/*
**  ZDRay collision
**  Copyright (c) 2018 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "math/mathlib.h"
#include <vector>

class SphereShape
{
public:
	SphereShape() { }
	SphereShape(const kexVec3 &center, float radius) : center(center), radius(radius) { }

	kexVec3 center;
	float radius = 0.0f;
};

struct TraceHit
{
	float fraction = 1.0f;
	int surface = -1;
};

class CollisionBBox : public kexBBox
{
public:
	CollisionBBox() = default;

	CollisionBBox(const kexVec3 &aabb_min, const kexVec3 &aabb_max) : kexBBox(aabb_min, aabb_max)
	{
		auto halfmin = aabb_min * 0.5f;
		auto halfmax = aabb_max * 0.5f;
		Center = halfmax + halfmin;
		Extents = halfmax - halfmin;
	}

	kexVec3 Center;
	kexVec3 Extents;
};

class RayBBox
{
public:
	RayBBox(const kexVec3 &ray_start, const kexVec3 &ray_end) : start(ray_start), end(ray_end)
	{
		c = (ray_start + ray_end) * 0.5f;
		w = ray_end - c;
		v.x = std::abs(w.x);
		v.y = std::abs(w.y);
		v.z = std::abs(w.z);
	}

	kexVec3 start, end;
	kexVec3 c, w, v;
};

class TriangleMeshShape
{
public:
	TriangleMeshShape(const kexVec3 *vertices, int num_vertices, const unsigned int *elements, int num_elements, const int *surfaces);

	int get_min_depth();
	int get_max_depth();
	float get_average_depth();
	float get_balanced_depth();

	static float sweep(TriangleMeshShape *shape1, SphereShape *shape2, const kexVec3 &target);

	static bool find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2);
	static bool find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2);
	static bool find_any_hit(TriangleMeshShape *shape, const kexVec3 &ray_start, const kexVec3 &ray_end);

	static TraceHit find_first_hit(TriangleMeshShape *shape, const kexVec3 &ray_start, const kexVec3 &ray_end);

	struct Node
	{
		Node() = default;
		Node(const kexVec3 &aabb_min, const kexVec3 &aabb_max, int element_index) : aabb(aabb_min, aabb_max), element_index(element_index) { }
		Node(const kexVec3 &aabb_min, const kexVec3 &aabb_max, int left, int right) : aabb(aabb_min, aabb_max), left(left), right(right) { }

		CollisionBBox aabb;
		int left = -1;
		int right = -1;
		int element_index = -1;
	};

	const kexVec3 *vertices = nullptr;
	const int num_vertices = 0;
	const unsigned int *elements = nullptr;
	int num_elements = 0;
	const int *surfaces = nullptr;

	std::vector<Node> nodes;
	int root = -1;

private:
	static float sweep(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target);

	static bool find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	static bool find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2, int a);
	static bool find_any_hit(TriangleMeshShape *shape1, const RayBBox &ray, int a);

	static void find_first_hit(TriangleMeshShape *shape1, const RayBBox &ray, int a, TraceHit *hit);

	inline static bool overlap_bv_ray(TriangleMeshShape *shape, const RayBBox &ray, int a);
	inline static float intersect_triangle_ray(TriangleMeshShape *shape, const RayBBox &ray, int a);

	inline static bool sweep_overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target);
	inline static float sweep_intersect_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target);

	inline static bool overlap_bv(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_bv_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a);
	inline static bool overlap_triangle_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a);

	inline bool is_leaf(int node_index);
	inline float volume(int node_index);

	int subdivide(int *triangles, int num_triangles, const kexVec3 *centroids, int *work_buffer);
};

class kexOrientedBBox
{
public:
	kexVec3 Center;
	kexVec3 Extents;
	kexVec3 axis_x;
	kexVec3 axis_y;
	kexVec3 axis_z;
};

class FrustumPlanes
{
public:
	FrustumPlanes();
	explicit FrustumPlanes(const kexMatrix &world_to_projection);

	kexVec4 planes[6];

private:
	static kexVec4 left_frustum_plane(const kexMatrix &matrix);
	static kexVec4 right_frustum_plane(const kexMatrix &matrix);
	static kexVec4 top_frustum_plane(const kexMatrix &matrix);
	static kexVec4 bottom_frustum_plane(const kexMatrix &matrix);
	static kexVec4 near_frustum_plane(const kexMatrix &matrix);
	static kexVec4 far_frustum_plane(const kexMatrix &matrix);
};

class IntersectionTest
{
public:
	enum Result
	{
		outside,
		inside,
		intersecting,
	};

	enum OverlapResult
	{
		disjoint,
		overlap
	};

	static Result plane_aabb(const kexVec4 &plane, const kexBBox &aabb);
	static Result plane_obb(const kexVec4 &plane, const kexOrientedBBox &obb);
	static OverlapResult sphere(const kexVec3 &center1, float radius1, const kexVec3 &center2, float radius2);
	static OverlapResult sphere_aabb(const kexVec3 &center, float radius, const kexBBox &aabb);
	static OverlapResult aabb(const kexBBox &a, const kexBBox &b);
	static Result frustum_aabb(const FrustumPlanes &frustum, const kexBBox &box);
	static Result frustum_obb(const FrustumPlanes &frustum, const kexOrientedBBox &box);
	static OverlapResult ray_aabb(const RayBBox &ray, const CollisionBBox &box);
};
