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
//-----------------------------------------------------------------------------
//
// DESCRIPTION: Lightmap and lightgrid building module
//
//-----------------------------------------------------------------------------

#include "math/mathlib.h"
#include "surfaces.h"
#include "trace.h"
#include "mapdata.h"
#include "lightmap.h"
#include "worker.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

extern int Multisample;
extern thread_local kexVec3 *colorSamples;

const kexVec3 kexLightmapBuilder::gridSize(64, 64, 128);

kexLightmapBuilder::kexLightmapBuilder()
{
	textureWidth = 128;
	textureHeight = 128;
	numTextures = 0;
	samples = 16;
	extraSamples = 2;
	ambience = 0.0f;
	tracedTexels = 0;
}

kexLightmapBuilder::~kexLightmapBuilder()
{
}

void kexLightmapBuilder::NewTexture()
{
	numTextures++;

	allocBlocks.push_back(new int[textureWidth]);
	memset(allocBlocks.back(), 0, sizeof(int) * textureWidth);

	uint16_t *texture = new uint16_t[textureWidth * textureHeight * 3];
	textures.push_back(texture);
}

// Determines where to map a new block on to the lightmap texture
bool kexLightmapBuilder::MakeRoomForBlock(const int width, const int height, int *x, int *y, int *num)
{
	int i;
	int j;
	int k;
	int bestRow1;
	int bestRow2;

	*num = -1;

	if (allocBlocks.empty())
	{
		return false;
	}

	for (k = 0; k < numTextures; ++k)
	{
		bestRow1 = textureHeight;

		for (i = 0; i <= textureWidth - width; i++)
		{
			bestRow2 = 0;

			for (j = 0; j < width; j++)
			{
				if (allocBlocks[k][i + j] >= bestRow1)
				{
					break;
				}

				if (allocBlocks[k][i + j] > bestRow2)
				{
					bestRow2 = allocBlocks[k][i + j];
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
			continue;
		}

		for (i = 0; i < width; i++)
		{
			// store row offset
			allocBlocks[k][*x + i] = bestRow1 + height;
		}

		*num = k;
		return true;
	}

	return false;
}

kexBBox kexLightmapBuilder::GetBoundsFromSurface(const surface_t *surface)
{
	kexVec3 low(M_INFINITY, M_INFINITY, M_INFINITY);
	kexVec3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

	kexBBox bounds;
	bounds.Clear();

	for (int i = 0; i < surface->numVerts; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (surface->verts[i][j] < low[j])
			{
				low[j] = surface->verts[i][j];
			}
			if (surface->verts[i][j] > hi[j])
			{
				hi[j] = surface->verts[i][j];
			}
		}
	}

	bounds.min = low;
	bounds.max = hi;

	return bounds;
}

// Traces to the ceiling surface. Will emit light if the surface that was traced is a sky
bool kexLightmapBuilder::EmitFromCeiling(kexTrace &trace, const surface_t *surface, const kexVec3 &origin, const kexVec3 &normal, kexVec3 &color)
{
	float attenuation = normal.Dot(map->GetSunDirection());

	if (attenuation <= 0)
	{
		// plane is not even facing the sunlight
		return false;
	}

	trace.Trace(origin, origin + (map->GetSunDirection() * 32768.0f));

	if (trace.fraction == 1.0f)
	{
		// nothing was hit
		//color.x += 1.0f;
		return false;
	}

	if (trace.hitSurface->bSky == false)
	{
		if (trace.hitSurface->type == ST_CEILING)
			return false;

		// not a ceiling/sky surface
		return false;
	}

	color += map->GetSunColor() * attenuation;

	return true;
}

template<class T>
T smoothstep(const T edge0, const T edge1, const T x)
{
	auto t = clamp<T>((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

static float radians(float degrees)
{
	return degrees * 3.14159265359f / 180.0f;
}

// Traces a line from the texel's origin to the sunlight direction and against all nearby thing lights
kexVec3 kexLightmapBuilder::LightTexelSample(kexTrace &trace, const kexVec3 &origin, surface_t *surface)
{
	kexPlane plane = surface->plane;
	kexVec3 color(0.0f, 0.0f, 0.0f);

	// check all thing lights
	for (unsigned int i = 0; i < map->thingLights.Size(); i++)
	{
		thingLight_t *tl = map->thingLights[i];

		float originZ;
		if (!tl->bCeiling)
			originZ = tl->sector->floorplane.zAt(tl->origin.x, tl->origin.y) + tl->height;
		else
			originZ = tl->sector->ceilingplane.zAt(tl->origin.x, tl->origin.y) - tl->height;

		kexVec3 lightOrigin(tl->origin.x, tl->origin.y, originZ);

		if (plane.Distance(lightOrigin) - plane.d < 0)
		{
			// completely behind the plane
			continue;
		}

		float radius = tl->radius * 2.0f; // 2.0 because gzdoom's dynlights do this and we want them to match
		float intensity = tl->intensity;

		if (origin.DistanceSq(lightOrigin) > (radius*radius))
		{
			// not within range
			continue;
		}

		kexVec3 dir = (lightOrigin - origin);
		float dist = dir.Unit();
		dir.Normalize();

		float spotAttenuation = 1.0f;
		if (tl->outerAngleCos > -1.0f)
		{
			float negPitch = -radians(tl->mapThing->pitch);
			float xyLen = std::cosf(negPitch);
			kexVec3 spotDir;
			spotDir.x = std::sinf(radians(tl->mapThing->angle)) * xyLen;
			spotDir.y = std::cosf(radians(tl->mapThing->angle)) * xyLen;
			spotDir.z = -std::sinf(negPitch);
			float cosDir = kexVec3::Dot(dir, spotDir);
			spotAttenuation = smoothstep(tl->outerAngleCos, tl->innerAngleCos, cosDir);
			if (spotAttenuation <= 0.0f)
			{
				// outside spot light
				continue;
			}
		}

		trace.Trace(lightOrigin, origin);

		if (trace.fraction != 1)
		{
			// this light is occluded by something
			continue;
		}

		float attenuation = 1.0f - (dist / radius);
		attenuation *= spotAttenuation;
		attenuation *= plane.Normal().Dot(dir);
		attenuation *= intensity;

		// accumulate results
		color += tl->rgb * attenuation;

		tracedTexels++;
	}

	if (surface->type != ST_CEILING)
	{
		// see if it's exposed to sunlight
		if (EmitFromCeiling(trace, surface, origin, plane.Normal(), color))
			tracedTexels++;
	}

	// trace against surface lights
	for (unsigned int i = 0; i < map->lightSurfaces.Size(); ++i)
	{
		kexLightSurface *surfaceLight = map->lightSurfaces[i];

		float attenuation = surfaceLight->TraceSurface(map, trace, surface, origin);
		if (attenuation > 0.0f)
		{
			color += surfaceLight->GetRGB() * surfaceLight->Intensity() * attenuation;
			tracedTexels++;
		}
	}

	return color;
}

// Determines a lightmap block in which to map to the lightmap texture.
// Width and height of the block is calcuated and steps are computed to determine where each texel will be positioned on the surface
void kexLightmapBuilder::BuildSurfaceParams(surface_t *surface)
{
	kexPlane *plane;
	kexBBox bounds;
	kexVec3 roundedSize;
	int i;
	kexPlane::planeAxis_t axis;
	kexVec3 tCoords[2];
	kexVec3 tOrigin;
	int width;
	int height;
	float d;

	plane = &surface->plane;
	bounds = GetBoundsFromSurface(surface);

	// round off dimentions
	for (i = 0; i < 3; i++)
	{
		bounds.min[i] = samples * kexMath::Floor(bounds.min[i] / samples);
		bounds.max[i] = samples * kexMath::Ceil(bounds.max[i] / samples);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / samples + 1;
	}

	tCoords[0].Clear();
	tCoords[1].Clear();

	axis = plane->BestAxis();

	switch (axis)
	{
	case kexPlane::AXIS_YZ:
		width = (int)roundedSize.y;
		height = (int)roundedSize.z;
		tCoords[0].y = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case kexPlane::AXIS_XZ:
		width = (int)roundedSize.x;
		height = (int)roundedSize.z;
		tCoords[0].x = 1.0f / samples;
		tCoords[1].z = 1.0f / samples;
		break;

	case kexPlane::AXIS_XY:
		width = (int)roundedSize.x;
		height = (int)roundedSize.y;
		tCoords[0].x = 1.0f / samples;
		tCoords[1].y = 1.0f / samples;
		break;
	}

	// clamp width
	if (width > textureWidth)
	{
		tCoords[0] *= ((float)textureWidth / (float)width);
		width = textureWidth;
	}

	// clamp height
	if (height > textureHeight)
	{
		tCoords[1] *= ((float)textureHeight / (float)height);
		height = textureHeight;
	}

	surface->lightmapCoords = new float[surface->numVerts * 2];

	surface->textureCoords[0] = tCoords[0];
	surface->textureCoords[1] = tCoords[1];

	tOrigin = bounds.min;

	// project tOrigin and tCoords so they lie on the plane
	d = (plane->Distance(bounds.min) - plane->d) / plane->Normal()[axis];
	tOrigin[axis] -= d;

	for (i = 0; i < 2; i++)
	{
		tCoords[i].Normalize();
		d = plane->Distance(tCoords[i]) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface->bounds = bounds;
	surface->lightmapDims[0] = width;
	surface->lightmapDims[1] = height;
	surface->lightmapOrigin = tOrigin;
	surface->lightmapSteps[0] = tCoords[0] * (float)samples;
	surface->lightmapSteps[1] = tCoords[1] * (float)samples;
}

// Steps through each texel and traces a line to the world.
// For each non-occluded trace, color is accumulated and saved off into the lightmap texture based on what block is mapped to
void kexLightmapBuilder::TraceSurface(surface_t *surface)
{
	int sampleWidth;
	int sampleHeight;
	kexVec3 normal;
	kexVec3 pos;
	kexVec3 tDelta;
	int i;
	int j;
	kexTrace trace;
	uint16_t *currentTexture;
	bool bShouldLookupTexture = false;

	trace.Init(*map);

	sampleWidth = surface->lightmapDims[0];
	sampleHeight = surface->lightmapDims[1];

	normal = surface->plane.Normal();

	int multisampleCount = Multisample;

	// start walking through each texel
	for (i = 0; i < sampleHeight; i++)
	{
		for (j = 0; j < sampleWidth; j++)
		{
			kexVec3 c(0.0f, 0.0f, 0.0f);

			for (int k = 0; k < multisampleCount; k++)
			{
				kexVec2 multisamplePos((float)j, (float)i);
				if (k > 0)
				{
					multisamplePos.x += rand() / (float)RAND_MAX - 0.5f;
					multisamplePos.y += rand() / (float)RAND_MAX - 0.5f;
					multisamplePos.x = std::max(multisamplePos.x, 0.0f);
					multisamplePos.y = std::max(multisamplePos.y, 0.0f);
					multisamplePos.x = std::min(multisamplePos.x, (float)sampleWidth);
					multisamplePos.y = std::min(multisamplePos.y, (float)sampleHeight);
				}

				// convert the texel into world-space coordinates.
				// this will be the origin in which a line will be traced from
				pos = surface->lightmapOrigin + normal +
					(surface->lightmapSteps[0] * multisamplePos.x) +
					(surface->lightmapSteps[1] * multisamplePos.y);

				c += LightTexelSample(trace, pos, surface);
			}

			c /= multisampleCount;

			// if nothing at all was traced and color is completely black
			// then this surface will not go through the extra rendering
			// step in rendering the lightmap
			if (c.x > 0.0f || c.y > 0.0f || c.z > 0.0f)
			{
				bShouldLookupTexture = true;
			}

			kexMath::Clamp(c, 0, 1);
			colorSamples[i * 1024 + j] = c;
		}
	}

	// SVE redraws the scene for lightmaps, so for optimizations,
	// tell the engine to ignore this surface if completely black
	if (bShouldLookupTexture == false)
	{
		surface->lightmapNum = -1;
		return;
	}
	else
	{
		int x = 0, y = 0;
		int width = surface->lightmapDims[0];
		int height = surface->lightmapDims[1];

		std::unique_lock<std::mutex> lock(mutex);

		// now that we know the width and height of this block, see if we got
		// room for it in the light map texture. if not, then we must allocate
		// a new texture
		if (!MakeRoomForBlock(width, height, &x, &y, &surface->lightmapNum))
		{
			// allocate a new texture for this block
			NewTexture();

			if (!MakeRoomForBlock(width, height, &x, &y, &surface->lightmapNum))
			{
				throw std::runtime_error("Lightmap allocation failed");
			}
		}

		lock.unlock();

		// calculate texture coordinates
		for (i = 0; i < surface->numVerts; i++)
		{
			tDelta = surface->verts[i] - surface->bounds.min;
			surface->lightmapCoords[i * 2 + 0] =
				(tDelta.Dot(surface->textureCoords[0]) + x + 0.5f) / (float)textureWidth;
			surface->lightmapCoords[i * 2 + 1] =
				(tDelta.Dot(surface->textureCoords[1]) + y + 0.5f) / (float)textureHeight;
		}

		surface->lightmapOffs[0] = x;
		surface->lightmapOffs[1] = y;
	}

	std::unique_lock<std::mutex> lock(mutex);
	currentTexture = textures[surface->lightmapNum];
	lock.unlock();

	// store results to lightmap texture
	for (i = 0; i < sampleHeight; i++)
	{
		for (j = 0; j < sampleWidth; j++)
		{
			// get texture offset
			int offs = (((textureWidth * (i + surface->lightmapOffs[1])) + surface->lightmapOffs[0]) * 3);

			// convert RGB to bytes
			currentTexture[offs + j * 3 + 0] = floatToHalf(colorSamples[i * 1024 + j].x);
			currentTexture[offs + j * 3 + 1] = floatToHalf(colorSamples[i * 1024 + j].y);
			currentTexture[offs + j * 3 + 2] = floatToHalf(colorSamples[i * 1024 + j].z);
		}
	}
}

void kexLightmapBuilder::LightSurface(const int surfid)
{
	float remaining;
	int numsurfs = surfaces.size();

	// TODO: this should NOT happen, but apparently, it can randomly occur
	if (surfaces.size() == 0)
	{
		return;
	}

	BuildSurfaceParams(surfaces[surfid]);
	TraceSurface(surfaces[surfid]);

	std::unique_lock<std::mutex> lock(mutex);

	int lastproc = processed * 100 / numsurfs;
	processed++;
	int curproc = processed * 100 / numsurfs;
	if (lastproc != curproc || processed == 1)
	{
		remaining = (float)processed / (float)numsurfs;
		printf("%i%c surfaces done\r", (int)(remaining * 100.0f), '%');
	}
}

void kexLightmapBuilder::CreateLightmaps(FLevel &doomMap)
{
	map = &doomMap;

	printf("------------- Tracing surfaces -------------\n");

	processed = 0;
	kexWorker::RunJob(surfaces.size(), [=](int id) {
		LightSurface(id);
	});

	printf("Texels traced: %i \n\n", tracedTexels);
}

void kexLightmapBuilder::AddLightmapLump(FWadWriter &wadFile)
{
	// Calculate size of lump
	int numTexCoords = 0;
	int numSurfaces = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum != -1)
		{
			numTexCoords += surfaces[i]->numVerts;
			numSurfaces++;
		}
	}
	int version = 0;
	int headerSize = 3 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
	int surfacesSize = surfaces.size() * 5 * sizeof(uint32_t);
	int texCoordsSize = numTexCoords * 2 * sizeof(float);
	int texDataSize = textures.size() * textureWidth * textureHeight * 3 * 2;
	int lumpSize = headerSize + surfacesSize + texCoordsSize + texDataSize;

	// Setup buffer
	std::vector<uint8_t> buffer(lumpSize);
	kexBinFile lumpFile;
	lumpFile.SetBuffer(buffer.data());

	// Write header
	lumpFile.Write32(version);
	lumpFile.Write16(textureWidth);
	lumpFile.Write16(textures.size());
	lumpFile.Write32(numSurfaces);
	lumpFile.Write32(numTexCoords);

	// Write surfaces
	int coordOffsets = 0;
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum == -1)
			continue;

		lumpFile.Write32(surfaces[i]->type);
		lumpFile.Write32(surfaces[i]->typeIndex);
		lumpFile.Write32(0xffffffff/*surfaces[i]->controlSector*/);
		lumpFile.Write32(surfaces[i]->lightmapNum);
		lumpFile.Write32(coordOffsets);
		coordOffsets += surfaces[i]->numVerts;
	}

	// Write texture coordinates
	for (size_t i = 0; i < surfaces.size(); i++)
	{
		if (surfaces[i]->lightmapNum == -1)
			continue;

		int count = surfaces[i]->numVerts;
		if (surfaces[i]->type == ST_FLOOR)
		{
			for (int j = count - 1; j >= 0; j--)
			{
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2]);
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2 + 1]);
			}
		}
		else if (surfaces[i]->type == ST_CEILING)
		{
			for (int j = 0; j < count; j++)
			{
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2]);
				lumpFile.WriteFloat(surfaces[i]->lightmapCoords[j * 2 + 1]);
			}
		}
		else
		{
			// zdray uses triangle strip internally, lump/gzd uses triangle fan

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[0]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[1]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[4]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[5]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[6]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[7]);

			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[2]);
			lumpFile.WriteFloat(surfaces[i]->lightmapCoords[3]);
		}
	}

	// Write lightmap textures
	for (size_t i = 0; i < textures.size(); i++)
	{
		unsigned int count = (textureWidth * textureHeight) * 3;
		for (unsigned int j = 0; j < count; j++)
		{
			lumpFile.Write16(textures[i][j]);
		}
	}

#if 0
	// Apply compression predictor
	uint8_t *texBytes = lumpFile.BufferAt() - texDataSize;
	for (int i = texDataSize - 1; i > 0; i--)
	{
		texBytes[i] -= texBytes[i - 1];
	}
#endif

	// Compress and store in lump
	ZLibOut zout(wadFile);
	wadFile.StartWritingLump("LIGHTMAP");
	zout.Write(buffer.data(), lumpFile.BufferAt() - lumpFile.Buffer());
}

/*
void kexLightmapBuilder::WriteTexturesToTGA()
{
	kexBinFile file;

	for (unsigned int i = 0; i < textures.size(); i++)
	{
		file.Create(Va("lightmap_%02d.tga", i));
		file.Write16(0);
		file.Write16(2);
		file.Write16(0);
		file.Write16(0);
		file.Write16(0);
		file.Write16(0);
		file.Write16(textureWidth);
		file.Write16(textureHeight);
		file.Write16(24);

		for (int j = 0; j < (textureWidth * textureHeight) * 3; j += 3)
		{
			file.Write8(textures[i][j + 2]);
			file.Write8(textures[i][j + 1]);
			file.Write8(textures[i][j + 0]);
		}
		file.Close();
	}
}
*/

void kexLightmapBuilder::WriteMeshToOBJ()
{
	FILE *f = fopen("mesh.obj", "w");

	std::map<int, std::vector<surface_t*>> sortedSurfs;

	for (unsigned int i = 0; i < surfaces.size(); i++)
		sortedSurfs[surfaces[i]->lightmapNum].push_back(surfaces[i]);

	for (const auto &it : sortedSurfs)
	{
		for (const auto &s : it.second)
		{
			for (int j = 0; j < s->numVerts; j++)
			{
				fprintf(f, "v %f %f %f\n", s->verts[j].x, s->verts[j].z + 100.0f, s->verts[j].y);
			}
		}
	}

	for (const auto &it : sortedSurfs)
	{
		for (const auto &s : it.second)
		{
			for (int j = 0; j < s->numVerts; j++)
			{
				fprintf(f, "vt %f %f\n", s->lightmapCoords[j * 2], s->lightmapCoords[j * 2 + 1]);
			}
		}
	}

	int voffset = 1;
	for (const auto &it : sortedSurfs)
	{
		int lightmapNum = it.first;

		if (lightmapNum != -1)
			fprintf(f, "usemtl lightmap_%02d\n", lightmapNum);
		else
			fprintf(f, "usemtl black\n");

		for (const auto &s : it.second)
		{
			switch (s->type)
			{
			case ST_FLOOR:
				for (int j = 2; j < s->numVerts; j++)
				{
					fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j, voffset + j, voffset + j - 1, voffset + j - 1, voffset, voffset);
				}
				break;
			case ST_CEILING:
				for (int j = 2; j < s->numVerts; j++)
				{
					fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset, voffset, voffset + j - 1, voffset + j - 1, voffset + j, voffset + j);
				}
				break;
			default:
				for (int j = 2; j < s->numVerts; j++)
				{
					if (j % 2 == 0)
						fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j - 2, voffset + j - 2, voffset + j - 1, voffset + j - 1, voffset + j, voffset + j);
					else
						fprintf(f, "f %d/%d %d/%d %d/%d\n", voffset + j, voffset + j, voffset + j - 1, voffset + j - 1, voffset + j - 2, voffset + j - 2);
				}
				break;
			}

			voffset += s->numVerts;
		}
	}

	fclose(f);
}
