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
#include <mutex>

#define LIGHTMAP_MAX_SIZE  1024

class kexTrace;
class FWadWriter;

class kexLightmapBuilder
{
public:
	kexLightmapBuilder();
	~kexLightmapBuilder();

	void BuildSurfaceParams(surface_t *surface);
	void TraceSurface(surface_t *surface);
	void CreateLightmaps(FLevel &doomMap);
	void LightSurface(const int surfid);
	//void WriteTexturesToTGA();
	void WriteMeshToOBJ();
	void AddLightmapLump(FWadWriter &wadFile);

	int samples;
	float ambience;
	int textureWidth;
	int textureHeight;

	static const kexVec3 gridSize;

private:
	void NewTexture();
	bool MakeRoomForBlock(const int width, const int height, int *x, int *y, int *num);
	kexBBox GetBoundsFromSurface(const surface_t *surface);
	kexVec3 LightTexelSample(kexTrace &trace, const kexVec3 &origin, surface_t *surface);
	bool EmitFromCeiling(kexTrace &trace, const surface_t *surface, const kexVec3 &origin, const kexVec3 &normal, kexVec3 &color);

	FLevel *map;
	std::vector<uint16_t*> textures;
	std::vector<int*> allocBlocks;
	int numTextures;
	int extraSamples;
	int tracedTexels;

	std::mutex mutex;
	int processed = 0;
};
