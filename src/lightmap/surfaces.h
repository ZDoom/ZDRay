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

#include <vector>
#include <memory>
#include <string>
#include <cstring>

#include "framework/tarray.h"
#include "lightmap/collision.h"

struct MapSubsectorEx;
struct IntSector;
struct IntSideDef;
struct FLevel;
class FWadWriter;

enum SurfaceType
{
	ST_UNKNOWN,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

struct Surface
{
	Plane plane;
	int lightmapNum;
	int lightmapOffs[2];
	int lightmapDims[2];
	Vec3 lightmapOrigin;
	Vec3 lightmapSteps[2];
	Vec3 textureCoords[2];
	BBox bounds;
	int numVerts;
	std::vector<Vec3> verts;
	std::vector<float> lightmapCoords;
	std::vector<Vec3> samples;
	std::vector<Vec3> indirect;
	SurfaceType type;
	int typeIndex;
	IntSector *controlSector;
	bool bSky;
	std::vector<Vec2> uvs;
	std::string material;
};

struct LevelTraceHit
{
	Vec3 start;
	Vec3 end;
	float fraction;

	Surface *hitSurface;
	int indices[3];
	float b, c;
};

class LightmapTexture
{
public:
	LightmapTexture(int width, int height) : textureWidth(width), textureHeight(height)
	{
		mPixels.resize(width * height * 3);
		allocBlocks.resize(width);
	}

	bool MakeRoomForBlock(const int width, const int height, int* x, int* y)
	{
		int bestRow1 = textureHeight;

		for (int i = 0; i <= textureWidth - width; i++)
		{
			int bestRow2 = 0;

			int j;
			for (j = 0; j < width; j++)
			{
				if (allocBlocks[i + j] >= bestRow1)
				{
					break;
				}

				if (allocBlocks[i + j] > bestRow2)
				{
					bestRow2 = allocBlocks[i + j];
				}
			}

			// found a free block
			if (j == width)
			{
				*x = i;
				*y = bestRow1 = bestRow2;
			}
		}

		if (bestRow1 + height > textureHeight)
		{
			// no room
			return false;
		}

		// store row offset
		for (int i = 0; i < width; i++)
		{
			allocBlocks[*x + i] = bestRow1 + height;
		}

		return true;
	}

	int Width() const { return textureWidth; }
	int Height() const { return textureHeight; }
	uint16_t* Pixels() { return mPixels.data(); }

private:
	int textureWidth;
	int textureHeight;
	std::vector<uint16_t> mPixels;
	std::vector<int> allocBlocks;
};

class LightProbeSample
{
public:
	Vec3 Position = Vec3(0.0f, 0.0f, 0.0f);
	Vec3 Color = Vec3(0.0f, 0.0f, 0.0f);
};

class LevelMesh
{
public:
	LevelMesh(FLevel &doomMap, int sampleDistance, int textureSize);

	void CreateTextures();
	void AddLightmapLump(FWadWriter& wadFile);
	void Export(std::string filename);

	LevelTraceHit Trace(const Vec3 &startVec, const Vec3 &endVec);
	bool TraceAnyHit(const Vec3 &startVec, const Vec3 &endVec);

	FLevel* map = nullptr;

	std::vector<std::unique_ptr<Surface>> surfaces;
	std::vector<LightProbeSample> lightProbes;

	std::vector<std::unique_ptr<LightmapTexture>> textures;

	int samples = 16;
	int textureWidth = 128;
	int textureHeight = 128;

	TArray<Vec3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;
	std::unique_ptr<TriangleMeshShape> CollisionMesh;

private:
	void CreateSubsectorSurfaces(FLevel &doomMap);
	void CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side);
	void CreateLightProbes(FLevel& doomMap);

	void BuildSurfaceParams(Surface* surface);
	BBox GetBoundsFromSurface(const Surface* surface);
	void FinishSurface(Surface* surface);
	uint16_t* AllocTextureRoom(Surface* surface, int* x, int* y);

	static bool IsDegenerate(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2);
};

/////////////////////////////////////////////////////////////////////////////

struct ZModelVec2f
{
	float X, Y;
};

struct ZModelVec3f
{
	float X, Y, Z;
};

struct ZModelVec4ub
{
	uint8_t X, Y, Z, W;
};

struct ZModelQuaternionf
{
	float X, Y, Z, W;
};

struct ZModelVertex
{
	ZModelVec3f Pos;
	ZModelVec4ub BoneWeights;
	ZModelVec4ub BoneIndices;
	ZModelVec3f Normal;
	ZModelVec2f TexCoords;
	ZModelVec3f TexCoords2;
};

struct ZModelMaterial
{
	std::string Name;
	uint32_t Flags = 0; // Two-sided, depth test/write, what else?
	uint32_t Renderstyle;
	uint32_t StartElement = 0;
	uint32_t VertexCount = 0;
};

template<typename Value>
struct ZModelTrack
{
	std::vector<float> Timestamps;
	std::vector<Value> Values;
};

struct ZModelBoneAnim
{
	ZModelTrack<ZModelVec3f> Translation;
	ZModelTrack<ZModelQuaternionf> Rotation;
	ZModelTrack<ZModelVec3f> Scale;
};

struct ZModelMaterialAnim
{
	ZModelTrack<ZModelVec3f> Translation;
	ZModelTrack<ZModelQuaternionf> Rotation; // Rotation center is texture center (0.5, 0.5)
	ZModelTrack<ZModelVec3f> Scale;
};

struct ZModelAnimation
{
	std::string Name; // Name of animation
	float Duration; // Length of this animation sequence in seconds

	ZModelVec3f AabbMin; // Animation bounds (for culling purposes)
	ZModelVec3f AabbMax;

	std::vector<ZModelBoneAnim> Bones; // Animation tracks for each bone
	std::vector<ZModelMaterialAnim> Materials; // Animation tracks for each material
};

enum class ZModelBoneType : uint32_t
{
	Normal,
	BillboardSpherical,
	BillboardCylindricalX,
	BillboardCylindricalY,
	BillboardCylindricalZ
};

struct ZModelBone
{
	std::string Name;
	ZModelBoneType Type = ZModelBoneType::Normal;
	int32_t ParentBone = -1;
	ZModelVec3f Pivot;
};

struct ZModelAttachment
{
	std::string Name;
	int32_t Bone = -1;
	ZModelVec3f Position;
};

struct ZModel
{
	// ZMDL chunk
	uint32_t Version = 1;
	std::vector<ZModelMaterial> Materials;
	std::vector<ZModelBone> Bones;
	std::vector<ZModelAnimation> Animations;
	std::vector<ZModelAttachment> Attachments;

	// ZDAT chunk
	std::vector<ZModelVertex> Vertices;
	std::vector<uint32_t> Elements;
};

struct ZChunkStream
{
	void Uint32(uint32_t v) { Write<uint32_t>(v); }
	void Float(float v) { Write<float>(v); }
	void Vec2f(const ZModelVec2f &v) { Write<ZModelVec2f>(v); }
	void Vec3f(const ZModelVec3f &v) { Write<ZModelVec3f>(v); }
	void Vec4ub(const ZModelVec4ub &v) { Write<ZModelVec4ub>(v); }
	void Quaternionf(const ZModelQuaternionf &v) { Write<ZModelQuaternionf>(v); }

	void Uint32Array(const std::vector<uint32_t> &v) { WriteArray<uint32_t>(v); }
	void FloatArray(const std::vector<float> &v) { WriteArray<float>(v); }
	void Vec2fArray(const std::vector<ZModelVec2f> &v) { WriteArray<ZModelVec2f>(v); }
	void Vec3fArray(const std::vector<ZModelVec3f> &v) { WriteArray<ZModelVec3f>(v); }
	void Vec4ubArray(const std::vector<ZModelVec4ub> &v) { WriteArray<ZModelVec4ub>(v); }
	void QuaternionfArray(const std::vector<ZModelQuaternionf> &v) { WriteArray<ZModelQuaternionf>(v); }
	void VertexArray(const std::vector<ZModelVertex> &v) { WriteArray<ZModelVertex>(v); }

	void String(const std::string &v)
	{
		Write(v.c_str(), v.length() + 1);
	}

	void StringArray(const std::vector<std::string> &v)
	{
		Uint32((uint32_t)v.size());
		for (const std::string &s : v)
			String(s);
	}

	const void *ChunkData() const { return buffer.data(); }
	uint32_t ChunkLength() const { return (uint32_t)pos; }

private:
	template<typename Type>
	void Write(const Type &v)
	{
		Write(&v, sizeof(Type));
	}

	template<typename Type>
	void WriteArray(const std::vector<Type> &v)
	{
		Uint32((uint32_t)v.size());
		Write(v.data(), v.size() * sizeof(Type));
	}

	void Write(const void *data, size_t size)
	{
		if (pos + size > buffer.size())
			buffer.resize(buffer.size() * 2);

		memcpy(buffer.data() + pos, data, size);
		pos += size;
	}

	std::vector<uint8_t> buffer = std::vector<uint8_t>(16 * 1024 * 1024);
	size_t pos = 0;
};
