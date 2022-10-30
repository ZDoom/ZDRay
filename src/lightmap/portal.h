#pragma once

#include "math/mathlib.h"

struct Portal
{
	mat4 transformation = mat4::identity();
	int sourceSectorGroup = 0;
	int targetSectorGroup = 0;

	inline vec3 TransformPosition(const vec3& pos) const
	{
		auto v = transformation * vec4(pos, 1.0);
		return vec3(v.x, v.y, v.z);
	}

	inline vec3 TransformRotation(const vec3& dir) const
	{
		auto v = transformation * vec4(dir, 0.0);
		return vec3(v.x, v.y, v.z);
	}

	// Check if the portal is inverse
	inline bool IsInversePortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(vec3(0)));
		return abs(diff.x) < 0.001 && abs(diff.y) < 0.001 && abs(diff.z) < 0.001;
	}

	// Check if the portal transformation is equal
	inline bool IsEqualPortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(vec3(0)) - TransformPosition(vec3(0));
		return (abs(diff.x) < 0.001 && abs(diff.y) < 0.001 && abs(diff.z) < 0.001);
	}

	// Do both portals travel from sector group A to sector group B?
	inline bool IsTravelingBetweenSameSectorGroups(const Portal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup;
	}
};

// for use with std::set to recursively go through portals
struct RejectRecursivePortals
{
	inline bool IsEqual(const Portal& a, const Portal& b) const
	{
		return a.IsInversePortal(b) || a.IsEqualPortal(b);
	}

	bool operator()(const Portal& a, const Portal& b) const
	{
		return !IsEqual(a, b) && std::memcmp(&a.transformation, &b.transformation, sizeof(mat4)) < 0;
	}
};

// for use with std::map to reject portals which have equal transformation between equal sector groups
struct IdenticalPortalComparator
{
	inline bool IsEqual(const Portal& a, const Portal& b) const
	{
		return a.IsEqualPortal(b) && a.IsTravelingBetweenSameSectorGroups(b);
	}

	bool operator()(const Portal& a, const Portal& b) const
	{
		return !IsEqual(a, b) && std::memcmp(&a.transformation, &b.transformation, sizeof(mat4)) < 0;
	}
};
