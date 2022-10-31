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

	// Checks only transformation
	inline bool IsInverseTransformationPortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(vec3(0)));
		return abs(diff.x) < 0.001 && abs(diff.y) < 0.001 && abs(diff.z) < 0.001;
	}

	// Checks only transformation
	inline bool IsEqualTransformationPortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(vec3(0)) - TransformPosition(vec3(0));
		return (abs(diff.x) < 0.001 && abs(diff.y) < 0.001 && abs(diff.z) < 0.001);
	}


	// Checks transformation, source and destiantion sector groups
	inline bool IsEqualPortal(const Portal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup && IsEqualTransformationPortal(portal);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsInversePortal(const Portal& portal) const
	{
		return sourceSectorGroup == portal.targetSectorGroup && targetSectorGroup == portal.sourceSectorGroup && IsInverseTransformationPortal(portal);
	}

	inline void DumpInfo()
	{
		auto v = TransformPosition(vec3(0));
		printf("Portal offset: %.3f %.3f %.3f\n\tsource group:\t%d\n\ttarget group:\t%d", v.x, v.y, v.z, sourceSectorGroup, targetSectorGroup);
	}
};

// for use with std::set to recursively go through portals and skip returning portals
struct RecursivePortalComparator
{
	bool operator()(const Portal& a, const Portal& b) const
	{
		return !a.IsInversePortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(mat4)) < 0;
	}
};

// for use with std::map to reject portals which have the same effect for light rays
struct IdenticalPortalComparator
{
	bool operator()(const Portal& a, const Portal& b) const
	{
		return !a.IsEqualPortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(mat4)) < 0;
	}
};
