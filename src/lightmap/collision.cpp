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

#include "collision.h"
#include <algorithm>
#include <functional>
#ifndef NO_SSE
#include <immintrin.h>
#endif

TriangleMeshShape::TriangleMeshShape(const kexVec3 *vertices, int num_vertices, const unsigned int *elements, int num_elements, const int *surfaces)
	: vertices(vertices), num_vertices(num_vertices), elements(elements), num_elements(num_elements), surfaces(surfaces)
{
	int num_triangles = num_elements / 3;
	if (num_triangles <= 0)
		return;

	std::vector<int> triangles;
	std::vector<kexVec3> centroids;
	triangles.reserve(num_triangles);
	centroids.reserve(num_triangles);
	for (int i = 0; i < num_triangles; i++)
	{
		triangles.push_back(i);

		int element_index = i * 3;
		kexVec3 centroid = (vertices[elements[element_index + 0]] + vertices[elements[element_index + 1]] + vertices[elements[element_index + 2]]) * (1.0f / 3.0f);
		centroids.push_back(centroid);
	}

	std::vector<int> work_buffer(num_triangles * 2);

	root = subdivide(&triangles[0], (int)triangles.size(), &centroids[0], &work_buffer[0]);
}

float TriangleMeshShape::sweep(TriangleMeshShape *shape1, SphereShape *shape2, const kexVec3 &target)
{
	return sweep(shape1, shape2, shape1->root, target);
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2)
{
	return find_any_hit(shape1, shape2, shape1->root, shape2->root);
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2)
{
	return find_any_hit(shape1, shape2, shape1->root);
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape, const kexVec3 &ray_start, const kexVec3 &ray_end)
{
	return find_any_hit(shape, RayBBox(ray_start, ray_end), shape->root);
}

TraceHit TriangleMeshShape::find_first_hit(TriangleMeshShape *shape, const kexVec3 &ray_start, const kexVec3 &ray_end)
{
	TraceHit hit;

	// Perform segmented tracing to keep the ray AABB box smaller

	kexVec3 ray_dir = ray_end - ray_start;
	float tracedist = ray_dir.Length();
	float segmentlen = std::max(100.0f, tracedist / 20.0f);
	for (float t = 0.0f; t < tracedist; t += segmentlen)
	{
		float segstart = t / tracedist;
		float segend = std::min(t + segmentlen, tracedist) / tracedist;

		find_first_hit(shape, RayBBox(ray_start + ray_dir * segstart, ray_start + ray_dir * segend), shape->root, &hit);
		if (hit.fraction < 1.0f)
		{
			hit.fraction = segstart * (1.0f - hit.fraction) + segend * hit.fraction;
			break;
		}
	}

	if (hit.surface != -1)
		hit.surface = shape->surfaces[hit.surface / 3];
	return hit;
}

float TriangleMeshShape::sweep(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target)
{
	if (sweep_overlap_bv_sphere(shape1, shape2, a, target))
	{
		if (shape1->is_leaf(a))
		{
			return sweep_intersect_triangle_sphere(shape1, shape2, a, target);
		}
		else
		{
			return std::min(sweep(shape1, shape2, shape1->nodes[a].left, target), sweep(shape1, shape2, shape1->nodes[a].right, target));
		}
	}
	return 1.0f;
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2, int a)
{
	if (overlap_bv_sphere(shape1, shape2, a))
	{
		if (shape1->is_leaf(a))
		{
			return overlap_triangle_sphere(shape1, shape2, a);
		}
		else
		{
			if (find_any_hit(shape1, shape2, shape1->nodes[a].left))
				return true;
			else
				return find_any_hit(shape1, shape2, shape1->nodes[a].right);
		}
	}
	return false;
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	bool leaf_a = shape1->is_leaf(a);
	bool leaf_b = shape2->is_leaf(b);
	if (leaf_a && leaf_b)
	{
		return overlap_triangle_triangle(shape1, shape2, a, b);
	}
	else if (!leaf_a && !leaf_b)
	{
		if (overlap_bv(shape1, shape2, a, b))
		{
			if (shape1->volume(a) > shape2->volume(b))
			{
				if (find_any_hit(shape1, shape2, shape1->nodes[a].left, b))
					return true;
				else
					return find_any_hit(shape1, shape2, shape1->nodes[a].right, b);
			}
			else
			{
				if (find_any_hit(shape1, shape2, a, shape2->nodes[b].left))
					return true;
				else
					return find_any_hit(shape1, shape2, a, shape2->nodes[b].right);
			}
		}
		return false;
	}
	else if (leaf_a)
	{
		if (overlap_bv_triangle(shape2, shape1, b, a))
		{
			if (find_any_hit(shape1, shape2, a, shape2->nodes[b].left))
				return true;
			else
				return find_any_hit(shape1, shape2, a, shape2->nodes[b].right);
		}
		return false;
	}
	else
	{
		if (overlap_bv_triangle(shape1, shape2, a, b))
		{
			if (find_any_hit(shape1, shape2, shape1->nodes[a].left, b))
				return true;
			else
				return find_any_hit(shape1, shape2, shape1->nodes[a].right, b);
		}
		return false;
	}
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape, const RayBBox &ray, int a)
{
	if (overlap_bv_ray(shape, ray, a))
	{
		if (shape->is_leaf(a))
		{
			return intersect_triangle_ray(shape, ray, a) < 1.0f;
		}
		else
		{
			if (find_any_hit(shape, ray, shape->nodes[a].left))
				return true;
			else
				return find_any_hit(shape, ray, shape->nodes[a].right);
		}
	}
	return false;
}

void TriangleMeshShape::find_first_hit(TriangleMeshShape *shape, const RayBBox &ray, int a, TraceHit *hit)
{
	if (overlap_bv_ray(shape, ray, a))
	{
		if (shape->is_leaf(a))
		{
			float t = intersect_triangle_ray(shape, ray, a);
			if (t < hit->fraction)
			{
				hit->fraction = t;
				hit->surface = shape->nodes[a].element_index;
			}
		}
		else
		{
			find_first_hit(shape, ray, shape->nodes[a].left, hit);
			find_first_hit(shape, ray, shape->nodes[a].right, hit);
		}
	}
}

bool TriangleMeshShape::overlap_bv_ray(TriangleMeshShape *shape, const RayBBox &ray, int a)
{
	return IntersectionTest::ray_aabb(ray, shape->nodes[a].aabb) == IntersectionTest::overlap;
}

float TriangleMeshShape::intersect_triangle_ray(TriangleMeshShape *shape, const RayBBox &ray, int a)
{
	const int start_element = shape->nodes[a].element_index;

	kexVec3 p[3] =
	{
		shape->vertices[shape->elements[start_element]],
		shape->vertices[shape->elements[start_element + 1]],
		shape->vertices[shape->elements[start_element + 2]]
	};

#if 1

	kexVec3 e1 = p[1] - p[0];
	kexVec3 e2 = p[2] - p[0];

	kexVec3 triangleNormal = kexVec3::Cross(e1, e2);
	float dist = kexVec3::Dot(p[0], triangleNormal);
	float distA = kexVec3::Dot(ray.start, triangleNormal) - dist;
	float distB = kexVec3::Dot(ray.end, triangleNormal) - dist;

	if (distA * distB >= 0.0f)
		return 1.0f;

	// Backface check
	//if (distA < 0.0f)
	//	return 1.0f;

	float projLength = distA - distB;
	float distance = (distA) / (projLength);
	if (distance > 1.0f)
		return 1.0f;

	float edgeTolerance = triangleNormal.LengthSq() * -0.0001f;
	kexVec3 point = ray.start * (1.0f - distance) + ray.end * distance;

	kexVec3 v0p = p[0] - point;
	kexVec3 v1p = p[1] - point;
	kexVec3 cp0 = kexVec3::Cross(v0p, v1p);
	if (kexVec3::Dot(cp0, triangleNormal) < edgeTolerance)
		return 1.0f;

	kexVec3 v2p = p[2] - point;
	kexVec3 cp1 = kexVec3::Cross(v1p, v2p);
	if (kexVec3::Dot(cp1, triangleNormal) < edgeTolerance)
		return 1.0f;

	kexVec3 cp2 = kexVec3::Cross(v2p, v0p);
	if (kexVec3::Dot(cp2, triangleNormal) < edgeTolerance)
		return 1.0f;

	return distance;

#else
	// Moeller–Trumbore ray-triangle intersection algorithm:

	kexVec3 D = ray.end - ray.start;

	// Find vectors for two edges sharing p[0]
	kexVec3 e1 = p[1] - p[0];
	kexVec3 e2 = p[2] - p[0];

	// Begin calculating determinant - also used to calculate u parameter
	kexVec3 P = kexVec3::Cross(D, e2);
	float det = kexVec3::Dot(e1, P);

	// Backface check
	//if (det < 0.0f)
	//	return 1.0f;

	// If determinant is near zero, ray lies in plane of triangle
	if (det > -FLT_EPSILON && det < FLT_EPSILON)
		return 1.0f;

	float inv_det = 1.0f / det;

	// Calculate distance from p[0] to ray origin
	kexVec3 T = ray.start - p[0];

	// Calculate u parameter and test bound
	float u = kexVec3::Dot(T, P) * inv_det;

	// Check if the intersection lies outside of the triangle
	if (u < 0.f || u > 1.f)
		return 1.0f;

	// Prepare to test v parameter
	kexVec3 Q = kexVec3::Cross(T, e1);

	// Calculate V parameter and test bound
	float v = kexVec3::Dot(D, Q) * inv_det;

	// The intersection lies outside of the triangle
	if (v < 0.f || u + v  > 1.f)
		return 1.0f;

	float t = kexVec3::Dot(e2, Q) * inv_det;
	if (t > FLT_EPSILON)
		return t;

	// No hit, no win
	return 1.0f;
#endif
}

bool TriangleMeshShape::sweep_overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target)
{
	// Convert to ray test by expanding the AABB:

	CollisionBBox aabb = shape1->nodes[a].aabb;
	aabb.Extents += shape2->radius;

	return IntersectionTest::ray_aabb(RayBBox(shape2->center, target), aabb) == IntersectionTest::overlap;
}

float TriangleMeshShape::sweep_intersect_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const kexVec3 &target)
{
	const int start_element = shape1->nodes[a].element_index;

	kexVec3 p[3] =
	{
		shape1->vertices[shape1->elements[start_element]],
		shape1->vertices[shape1->elements[start_element + 1]],
		shape1->vertices[shape1->elements[start_element + 2]]
	};

	kexVec3 c = shape2->center;
	kexVec3 e = target;
	float r = shape2->radius;

	// Dynamic intersection test between a ray and the minkowski sum of the sphere and polygon:

	kexVec3 n = kexVec3::Normalize(kexVec3::Cross(p[1] - p[0], p[2] - p[0]));
	kexVec4 plane(n, -kexVec3::Dot(n, p[0]));

	// Step 1: Plane intersect test

	float sc = kexVec4::Dot(plane, kexVec4(c, 1.0f));
	float se = kexVec4::Dot(plane, kexVec4(e, 1.0f));
	bool same_side = sc * se > 0.0f;

	if (same_side && std::abs(sc) > r && std::abs(se) > r)
		return 1.0f;

	// Step 1a: Check if point is in polygon (using crossing ray test in 2d)
	{
		float t = (sc - r) / (sc - se);

		kexVec3 vt = c + (e - c) * t;

		kexVec3 u0 = p[1] - p[0];
		kexVec3 u1 = p[2] - p[0];

		kexVec2 v_2d[3] =
		{
			kexVec2(0.0f, 0.0f),
			kexVec2(kexVec3::Dot(u0, u0), 0.0f),
			kexVec2(0.0f, kexVec3::Dot(u1, u1))
		};

		kexVec2 point(kexVec3::Dot(u0, vt), kexVec3::Dot(u1, vt));

		bool inside = false;
		kexVec2 e0 = v_2d[2];
		bool y0 = e0.y >= point.y;
		for (int i = 0; i < 3; i++)
		{
			kexVec2 e1 = v_2d[i];
			bool y1 = e1.y >= point.y;

			if (y0 != y1 && ((e1.y - point.y) * (e0.x - e1.x) >= (e1.x - point.x) * (e0.y - e1.y)) == y1)
				inside = !inside;

			y0 = y1;
			e0 = e1;
		}

		if (inside)
			return t;
	}

	// Step 2: Edge intersect test

	kexVec3 ke[3] =
	{
		p[1] - p[0],
		p[2] - p[1],
		p[0] - p[2],
	};

	kexVec3 kg[3] =
	{
		p[0] - c,
		p[1] - c,
		p[2] - c,
	};

	kexVec3 ks = e - c;

	float kgg[3];
	float kgs[3];
	float kss[3];

	for (int i = 0; i < 3; i++)
	{
		float kee = kexVec3::Dot(ke[i], ke[i]);
		float keg = kexVec3::Dot(ke[i], kg[i]);
		float kes = kexVec3::Dot(ke[i], ks);
		kgg[i] = kexVec3::Dot(kg[i], kg[i]);
		kgs[i] = kexVec3::Dot(kg[i], ks);
		kss[i] = kexVec3::Dot(ks, ks);

		float aa = kee * kss[i] - kes * kes;
		float bb = 2 * (keg * kes - kee * kgs[i]);
		float cc = kee * (kgg[i] - r * r) - keg * keg;

		float sign = (bb >= 0.0f) ? 1.0f : -1.0f;
		float q = -0.5f * (bb + sign * std::sqrt(bb * bb - 4 * aa * cc));
		float t0 = q / aa;
		float t1 = cc / q;

		float t;
		if (t0 < 0.0f || t0 > 1.0f)
			t = t1;
		else if (t1 < 0.0f || t1 > 1.0f)
			t = t0;
		else
			t = std::min(t0, t1);

		if (t >= 0.0f && t <= 1.0f)
		{
			kexVec3 ct = c + ks * t;
			float d = kexVec3::Dot(ct - p[i], ke[i]);
			if (d >= 0.0f && d <= kee)
				return t;
		}
	}

	// Step 3: Point intersect test

	for (int i = 0; i < 3; i++)
	{
		float aa = kss[i];
		float bb = -2.0f * kgs[i];
		float cc = kgg[i] - r * r;

		float sign = (bb >= 0.0f) ? 1.0f : -1.0f;
		float q = -0.5f * (bb + sign * std::sqrt(bb * bb - 4 * aa * cc));
		float t0 = q / aa;
		float t1 = cc / q;

		float t;
		if (t0 < 0.0f || t0 > 1.0f)
			t = t1;
		else if (t1 < 0.0f || t1 > 1.0f)
			t = t0;
		else
			t = std::min(t0, t1);

		if (t >= 0.0f && t <= 1.0f)
			return t;
	}

	return 1.0f;
}

bool TriangleMeshShape::overlap_bv(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return IntersectionTest::aabb(shape1->nodes[a].aabb, shape2->nodes[b].aabb) == IntersectionTest::overlap;
}

bool TriangleMeshShape::overlap_bv_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return false;
}

bool TriangleMeshShape::overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a)
{
	return IntersectionTest::sphere_aabb(shape2->center, shape2->radius, shape1->nodes[a].aabb) == IntersectionTest::overlap;
}

bool TriangleMeshShape::overlap_triangle_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return false;
}

bool TriangleMeshShape::overlap_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int shape1_node_index)
{
	// http://realtimecollisiondetection.net/blog/?p=103

	int element_index = shape1->nodes[shape1_node_index].element_index;

	kexVec3 P = shape2->center;
	kexVec3 A = shape1->vertices[shape1->elements[element_index]] - P;
	kexVec3 B = shape1->vertices[shape1->elements[element_index + 1]] - P;
	kexVec3 C = shape1->vertices[shape1->elements[element_index + 2]] - P;
	float r = shape2->radius;
	float rr = r * r;

	// Testing if sphere lies outside the triangle plane
	kexVec3 V = kexVec3::Cross(B - A, C - A);
	float d = kexVec3::Dot(A, V);
	float e = kexVec3::Dot(V, V);
	bool sep1 = d * d > rr * e;

	// Testing if sphere lies outside a triangle vertex
	float aa = kexVec3::Dot(A, A);
	float ab = kexVec3::Dot(A, B);
	float ac = kexVec3::Dot(A, C);
	float bb = kexVec3::Dot(B, B);
	float bc = kexVec3::Dot(B, C);
	float cc = kexVec3::Dot(C, C);
	bool sep2 = (aa > rr) && (ab > aa) && (ac > aa);
	bool sep3 = (bb > rr) && (ab > bb) && (bc > bb);
	bool sep4 = (cc > rr) && (ac > cc) && (bc > cc);

	// Testing if sphere lies outside a triangle edge
	kexVec3 AB = B - A;
	kexVec3 BC = C - B;
	kexVec3 CA = A - C;
	float d1 = ab - aa;
	float d2 = bc - bb;
	float d3 = ac - cc;
	float e1 = kexVec3::Dot(AB, AB);
	float e2 = kexVec3::Dot(BC, BC);
	float e3 = kexVec3::Dot(CA, CA);
	kexVec3 Q1 = A * e1 - AB * d1;
	kexVec3 Q2 = B * e2 - BC * d2;
	kexVec3 Q3 = C * e3 - CA * d3;
	kexVec3 QC = C * e1 - Q1;
	kexVec3 QA = A * e2 - Q2;
	kexVec3 QB = B * e3 - Q3;
	bool sep5 = (kexVec3::Dot(Q1, Q1) > rr * e1 * e1) && (kexVec3::Dot(Q1, QC) > 0.0f);
	bool sep6 = (kexVec3::Dot(Q2, Q2) > rr * e2 * e2) && (kexVec3::Dot(Q2, QA) > 0.0f);
	bool sep7 = (kexVec3::Dot(Q3, Q3) > rr * e3 * e3) && (kexVec3::Dot(Q3, QB) > 0.0f);

	bool separated = sep1 || sep2 || sep3 || sep4 || sep5 || sep6 || sep7;
	return (!separated);
}

bool TriangleMeshShape::is_leaf(int node_index)
{
	return nodes[node_index].element_index != -1;
}

float TriangleMeshShape::volume(int node_index)
{
	const kexVec3 &extents = nodes[node_index].aabb.Extents;
	return extents.x * extents.y * extents.z;
}

int TriangleMeshShape::get_min_depth() const
{
	std::function<int(int, int)> visit;
	visit = [&](int level, int node_index) -> int {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return std::min(visit(level + 1, node.left), visit(level + 1, node.right));
		else
			return level;
	};
	return visit(1, root);
}

int TriangleMeshShape::get_max_depth() const
{
	std::function<int(int, int)> visit;
	visit = [&](int level, int node_index) -> int {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return std::max(visit(level + 1, node.left), visit(level + 1, node.right));
		else
			return level;
	};
	return visit(1, root);
}

float TriangleMeshShape::get_average_depth() const
{
	std::function<float(int, int)> visit;
	visit = [&](int level, int node_index) -> float {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return visit(level + 1, node.left) + visit(level + 1, node.right);
		else
			return (float)level;
	};
	float depth_sum = visit(1, root);
	int leaf_count = (num_elements / 3);
	return depth_sum / leaf_count;
}

float TriangleMeshShape::get_balanced_depth() const
{
	return std::log2((float)(num_elements / 3));
}

int TriangleMeshShape::subdivide(int *triangles, int num_triangles, const kexVec3 *centroids, int *work_buffer)
{
	if (num_triangles == 0)
		return -1;

	// Find bounding box and median of the triangle centroids
	kexVec3 median;
	kexVec3 min, max;
	min = vertices[elements[triangles[0] * 3]];
	max = min;
	for (int i = 0; i < num_triangles; i++)
	{
		int element_index = triangles[i] * 3;
		for (int j = 0; j < 3; j++)
		{
			const kexVec3 &vertex = vertices[elements[element_index + j]];

			min.x = std::min(min.x, vertex.x);
			min.y = std::min(min.y, vertex.y);
			min.z = std::min(min.z, vertex.z);

			max.x = std::max(max.x, vertex.x);
			max.y = std::max(max.y, vertex.y);
			max.z = std::max(max.z, vertex.z);
		}

		median += centroids[triangles[i]];
	}
	median /= (float)num_triangles;

	if (num_triangles == 1) // Leaf node
	{
		nodes.push_back(Node(min, max, triangles[0] * 3));
		return (int)nodes.size() - 1;
	}

	// Find the longest axis
	float axis_lengths[3] =
	{
		max.x - min.x,
		max.y - min.y,
		max.z - min.z
	};

	int axis_order[3] = { 0, 1, 2 };
	std::sort(axis_order, axis_order + 3, [&](int a, int b) { return axis_lengths[a] > axis_lengths[b]; });

	// Try split at longest axis, then if that fails the next longest, and then the remaining one
	int left_count, right_count;
	kexVec3 axis;
	for (int attempt = 0; attempt < 3; attempt++)
	{
		// Find the split plane for axis
		switch (axis_order[attempt])
		{
		default:
		case 0: axis = kexVec3(1.0f, 0.0f, 0.0f); break;
		case 1: axis = kexVec3(0.0f, 1.0f, 0.0f); break;
		case 2: axis = kexVec3(0.0f, 0.0f, 1.0f); break;
		}
		kexVec4 plane(axis, -kexVec3::Dot(median, axis));

		// Split triangles into two
		left_count = 0;
		right_count = 0;
		for (int i = 0; i < num_triangles; i++)
		{
			int triangle = triangles[i];
			int element_index = triangle * 3;

			float side = kexVec4::Dot(kexVec4(centroids[triangles[i]], 1.0f), plane);
			if (side >= 0.0f)
			{
				work_buffer[left_count] = triangle;
				left_count++;
			}
			else
			{
				work_buffer[num_triangles + right_count] = triangle;
				right_count++;
			}
		}

		if (left_count != 0 && right_count != 0)
			break;
	}

	// Check if something went wrong when splitting and do a random split instead
	if (left_count == 0 || right_count == 0)
	{
		left_count = num_triangles / 2;
		right_count = num_triangles - left_count;
	}
	else
	{
		// Move result back into triangles list:
		for (int i = 0; i < left_count; i++)
			triangles[i] = work_buffer[i];
		for (int i = 0; i < right_count; i++)
			triangles[i + left_count] = work_buffer[num_triangles + i];
	}

	// Create child nodes:
	int left_index = -1;
	int right_index = -1;
	if (left_count > 0)
		left_index = subdivide(triangles, left_count, centroids, work_buffer);
	if (right_count > 0)
		right_index = subdivide(triangles + left_count, right_count, centroids, work_buffer);

	nodes.push_back(Node(min, max, left_index, right_index));
	return (int)nodes.size() - 1;
}

/////////////////////////////////////////////////////////////////////////////

IntersectionTest::Result IntersectionTest::plane_aabb(const kexVec4 &plane, const kexBBox &aabb)
{
	kexVec3 center = aabb.Center();
	kexVec3 extents = aabb.Extents();
	float e = extents.x * std::abs(plane.x) + extents.y * std::abs(plane.y) + extents.z * std::abs(plane.z);
	float s = center.x * plane.x + center.y * plane.y + center.z * plane.z + plane.w;
	if (s - e > 0)
		return inside;
	else if (s + e < 0)
		return outside;
	else
		return intersecting;
}

IntersectionTest::Result IntersectionTest::plane_obb(const kexVec4 &plane, const kexOrientedBBox &obb)
{
	kexVec3 n = plane.ToVec3();
	float d = plane.w;
	float e = obb.Extents.x * std::abs(kexVec3::Dot(obb.axis_x, n)) + obb.Extents.y * std::abs(kexVec3::Dot(obb.axis_y, n)) + obb.Extents.z * std::abs(kexVec3::Dot(obb.axis_z, n));
	float s = kexVec3::Dot(obb.Center, n) + d;
	if (s - e > 0)
		return inside;
	else if (s + e < 0)
		return outside;
	else
		return intersecting;
}

IntersectionTest::OverlapResult IntersectionTest::sphere(const kexVec3 &center1, float radius1, const kexVec3 &center2, float radius2)
{
	kexVec3 h = center1 - center2;
	float square_distance = kexVec3::Dot(h, h);
	float radius_sum = radius1 + radius2;
	if (square_distance > radius_sum * radius_sum)
		return disjoint;
	else
		return overlap;
}

IntersectionTest::OverlapResult IntersectionTest::sphere_aabb(const kexVec3 &center, float radius, const kexBBox &aabb)
{
	kexVec3 a = aabb.min - center;
	kexVec3 b = center - aabb.max;
	a.x = std::max(a.x, 0.0f);
	a.y = std::max(a.y, 0.0f);
	a.z = std::max(a.z, 0.0f);
	b.x = std::max(b.x, 0.0f);
	b.y = std::max(b.y, 0.0f);
	b.z = std::max(b.z, 0.0f);
	kexVec3 e = a + b;
	float d = kexVec3::Dot(e, e);
	if (d > radius * radius)
		return disjoint;
	else
		return overlap;
}

IntersectionTest::OverlapResult IntersectionTest::aabb(const kexBBox &a, const kexBBox &b)
{
	if (a.min.x > b.max.x || b.min.x > a.max.x ||
		a.min.y > b.max.y || b.min.y > a.max.y ||
		a.min.z > b.max.z || b.min.z > a.max.z)
	{
		return disjoint;
	}
	else
	{
		return overlap;
	}
}

IntersectionTest::Result IntersectionTest::frustum_aabb(const FrustumPlanes &frustum, const kexBBox &box)
{
	bool is_intersecting = false;
	for (int i = 0; i < 6; i++)
	{
		Result result = plane_aabb(frustum.planes[i], box);
		if (result == outside)
			return outside;
		else if (result == intersecting)
			is_intersecting = true;
		break;
	}
	if (is_intersecting)
		return intersecting;
	else
		return inside;
}

IntersectionTest::Result IntersectionTest::frustum_obb(const FrustumPlanes &frustum, const kexOrientedBBox &box)
{
	bool is_intersecting = false;
	for (int i = 0; i < 6; i++)
	{
		Result result = plane_obb(frustum.planes[i], box);
		if (result == outside)
			return outside;
		else if (result == intersecting)
			is_intersecting = true;
	}
	if (is_intersecting)
		return intersecting;
	else
		return inside;
}

static const uint32_t clearsignbitmask[] = { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff };

IntersectionTest::OverlapResult IntersectionTest::ray_aabb(const RayBBox &ray, const CollisionBBox &aabb)
{
#ifndef NO_SSE

	__m128 v = _mm_loadu_ps(&ray.v.x);
	__m128 w = _mm_loadu_ps(&ray.w.x);
	__m128 h = _mm_loadu_ps(&aabb.Extents.x);
	__m128 c = _mm_sub_ps(_mm_loadu_ps(&ray.c.x), _mm_loadu_ps(&aabb.Center.x));

	__m128 clearsignbit = _mm_loadu_ps(reinterpret_cast<const float*>(clearsignbitmask));

	__m128 abs_c = _mm_and_ps(c, clearsignbit);
	int mask = _mm_movemask_ps(_mm_cmpgt_ps(abs_c, _mm_add_ps(v, h)));
	if (mask & 7)
		return disjoint;

	__m128 c1 = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 0, 1)); // c.y, c.x, c.x
	__m128 c2 = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 1, 2, 2)); // c.z, c.z, c.y
	__m128 w1 = _mm_shuffle_ps(w, w, _MM_SHUFFLE(3, 1, 2, 2)); // w.z, w.z, w.y
	__m128 w2 = _mm_shuffle_ps(w, w, _MM_SHUFFLE(3, 0, 0, 1)); // w.y, w.x, w.x
	__m128 lhs = _mm_and_ps(_mm_sub_ps(_mm_mul_ps(c1, w1), _mm_mul_ps(c2, w2)), clearsignbit);

	__m128 h1 = _mm_shuffle_ps(h, h, _MM_SHUFFLE(3, 0, 0, 1)); // h.y, h.x, h.x
	__m128 h2 = _mm_shuffle_ps(h, h, _MM_SHUFFLE(3, 1, 2, 2)); // h.z, h.z, h.y
	__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 1, 2, 2)); // v.z, v.z, v.y
	__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 0, 0, 1)); // v.y, v.x, v.x
	__m128 rhs = _mm_add_ps(_mm_mul_ps(h1, v1), _mm_mul_ps(h2, v2));

	mask = _mm_movemask_ps(_mm_cmpgt_ps(lhs, rhs));
	return (mask & 7) ? disjoint : overlap;

#else
	const kexVec3 &v = ray.v;
	const kexVec3 &w = ray.w;
	const kexVec3 &h = aabb.Extents;
	auto c = ray.c - aabb.Center;

	if (std::abs(c.x) > v.x + h.x || std::abs(c.y) > v.y + h.y || std::abs(c.z) > v.z + h.z)
		return disjoint;

	if (std::abs(c.y * w.z - c.z * w.y) > h.y * v.z + h.z * v.y ||
		std::abs(c.x * w.z - c.z * w.x) > h.x * v.z + h.z * v.x ||
		std::abs(c.x * w.y - c.y * w.x) > h.x * v.y + h.y * v.x)
		return disjoint;

	return overlap;
#endif
}

/////////////////////////////////////////////////////////////////////////////

FrustumPlanes::FrustumPlanes()
{
}

FrustumPlanes::FrustumPlanes(const kexMatrix &world_to_projection)
{
	planes[0] = near_frustum_plane(world_to_projection);
	planes[1] = far_frustum_plane(world_to_projection);
	planes[2] = left_frustum_plane(world_to_projection);
	planes[3] = right_frustum_plane(world_to_projection);
	planes[4] = top_frustum_plane(world_to_projection);
	planes[5] = bottom_frustum_plane(world_to_projection);
}

kexVec4 FrustumPlanes::left_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] + matrix[0 + 0 * 4],
		matrix[3 + 1 * 4] + matrix[0 + 1 * 4],
		matrix[3 + 2 * 4] + matrix[0 + 2 * 4],
		matrix[3 + 3 * 4] + matrix[0 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}

kexVec4 FrustumPlanes::right_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] - matrix[0 + 0 * 4],
		matrix[3 + 1 * 4] - matrix[0 + 1 * 4],
		matrix[3 + 2 * 4] - matrix[0 + 2 * 4],
		matrix[3 + 3 * 4] - matrix[0 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}

kexVec4 FrustumPlanes::top_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] - matrix[1 + 0 * 4],
		matrix[3 + 1 * 4] - matrix[1 + 1 * 4],
		matrix[3 + 2 * 4] - matrix[1 + 2 * 4],
		matrix[3 + 3 * 4] - matrix[1 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}

kexVec4 FrustumPlanes::bottom_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] + matrix[1 + 0 * 4],
		matrix[3 + 1 * 4] + matrix[1 + 1 * 4],
		matrix[3 + 2 * 4] + matrix[1 + 2 * 4],
		matrix[3 + 3 * 4] + matrix[1 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}

kexVec4 FrustumPlanes::near_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] + matrix[2 + 0 * 4],
		matrix[3 + 1 * 4] + matrix[2 + 1 * 4],
		matrix[3 + 2 * 4] + matrix[2 + 2 * 4],
		matrix[3 + 3 * 4] + matrix[2 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}

kexVec4 FrustumPlanes::far_frustum_plane(const kexMatrix &matrix)
{
	kexVec4 plane(
		matrix[3 + 0 * 4] - matrix[2 + 0 * 4],
		matrix[3 + 1 * 4] - matrix[2 + 1 * 4],
		matrix[3 + 2 * 4] - matrix[2 + 2 * 4],
		matrix[3 + 3 * 4] - matrix[2 + 3 * 4]);
	plane /= plane.ToVec3().Length();
	return plane;
}
