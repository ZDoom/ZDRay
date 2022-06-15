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

#include "math/mathlib.h"
#include "level/level.h"
#include <algorithm>
#include <memory>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

// convert from fixed point(FRACUNIT) to floating point
#define F(x)  (((float)(x))/65536.0f)

void FLevel::SetupLights()
{
	CheckSkySectors();

	for (unsigned int i = 0; i < Sectors.Size(); i++)
		Sectors[i].controlsector = false;

	for (unsigned int i = 0; i < Sides.Size(); i++)
		Sides[i].line = nullptr;

	for (unsigned int i = 0; i < Lines.Size(); i++)
	{
		IntLineDef *line = &Lines[i];

		// Link sides to lines
		if (line->sidenum[0] < Sides.Size())
			Sides[line->sidenum[0]].line = line;
		if (line->sidenum[1] < Sides.Size())
			Sides[line->sidenum[1]].line = line;

		if (line->special == Sector_Set3DFloor)
		{
			int sectorTag = line->args[0];
			int type = line->args[1];
			int opacity = line->args[3];

			if (opacity > 0)
			{
				IntSector *controlsector = &Sectors[Sides[Lines[i].sidenum[0]].sector];
				controlsector->controlsector = true;

				for (unsigned int j = 0; j < Sectors.Size(); j++)
				{
					for (unsigned t = 0; t < Sectors[j].tags.Size(); t++)
					{
						if (Sectors[j].tags[t] == sectorTag)
						{
							Sectors[j].x3dfloors.Push(controlsector);
							break;
						}
					}
				}
			}
		}
	}

	// GG to whoever memset'ed FLevel
	defaultSunColor = vec3(1, 1, 1);
	defaultSunDirection = vec3(0.45f, 0.3f, 0.9f);
	DefaultSamples = 8;
	LightBounce = 0;
	GridSize = 32.0f;

	for (int i = 0; i < (int)Things.Size(); ++i)
	{
		IntThing* thing = &Things[i];
		if (thing->type == THING_LIGHTPROBE)
		{
			ThingLightProbes.Push(i);
		}
		else if (thing->type == THING_ZDRAYINFO)
		{
			uint32_t lightcolor = 0xffffff;
			vec3 sundir(0.0f, 0.0f, 0.0f);
			vec3 suncolor(1.0f, 1.0f, 1.0f);

			// to do: is the math here correct?
			float sdx = (float)std::cos(radians(thing->angle)) * (float)std::cos(radians(thing->pitch));
			float sdy = (float)std::sin(radians(thing->angle)) * (float)std::cos(radians(thing->pitch));
			float sdz = (float)-std::sin(radians(thing->pitch));
			sundir.x = -sdx;
			sundir.y = -sdy;
			sundir.z = -sdz;

			printf("Sun vector: %f, %f, %f\n", sundir.x, sundir.y, sundir.z);

			for (unsigned int propIndex = 0; propIndex < thing->props.Size(); propIndex++)
			{
				const UDMFKey &key = thing->props[propIndex];

				if (!stricmp(key.key, "lm_suncolor"))
				{
					FString hex = FString("0x") + FString(key.value);
					hex.StripChars("\"");
					printf("Sun color: %s\n", hex);
					int rgb = hex.ToULong();
					lightcolor = (uint32_t)rgb;
				}
				else if (!stricmp(key.key, "lm_sampledistance"))
				{
					DefaultSamples = atoi(key.value);
					if (DefaultSamples < 8) DefaultSamples = 8;
					if (DefaultSamples > 128) DefaultSamples = 128;
					DefaultSamples = Math::RoundPowerOfTwo(DefaultSamples);
				}
				/*
				// light bounces temporarily disabled
				else if (!stricmp(key.key, "lm_bounces"))
				{
					LightBounce = atoi(key.value);
					if (LightBounce < 0) LightBounce = 0;
					if (LightBounce > 8) LightBounce = 8;
				}
				*/
				else if (!stricmp(key.key, "lm_gridsize"))
				{
					GridSize = atof(key.value) ? atof(key.value) : 64.f;
					if (GridSize < 1.f) GridSize = 1.f;
					if (GridSize > 1024.f) GridSize = 1024.f;
				}
			}

			if (dot(sundir, sundir) > 0.01f)
			{
				sundir = normalize(sundir);
				suncolor.x = ((lightcolor >> 16) & 0xff) / 255.0f;
				suncolor.y = ((lightcolor >> 8) & 0xff) / 255.0f;
				suncolor.z = (lightcolor & 0xff) / 255.0f;

				defaultSunColor = suncolor;
				defaultSunDirection = sundir;
			}
		}
	}

	CreateLights();
}

void FLevel::CheckSkySectors()
{
	char name[65];

	for (int i = 0; i < (int)Sectors.Size(); ++i)
	{
		//if (mapDef && mapDef->sunIgnoreTag != 0 && Sectors[i].data.tag == mapDef->sunIgnoreTag)
		//	continue;

		strncpy(name, Sectors[i].data.ceilingpic, 64);
		name[64] = 0;

		if (!strncmp(name, "F_SKY001", 64) || !strncmp(name, "F_SKY1", 64) || !strncmp(name, "F_SKY", 64))
		{
			Sectors[i].skySector = true;
		}
		else
		{
			Sectors[i].skySector = false;
		}
	}
}

const vec3 &FLevel::GetSunColor() const
{
	return defaultSunColor;
}

const vec3 &FLevel::GetSunDirection() const
{
	return defaultSunDirection;
}

IntSector *FLevel::GetFrontSector(const IntSideDef *side)
{
	return &Sectors[side->sector];
}

IntSector *FLevel::GetBackSector(const IntSideDef *side)
{
	IntLineDef *line = side->line;
	if (!(line->flags & ML_TWOSIDED))
		return nullptr;

	int sidenum = (ptrdiff_t)(side - &Sides[0]);
	if (line->sidenum[0] == sidenum)
		sidenum = line->sidenum[1];
	else
		sidenum = line->sidenum[0];

	if (sidenum == NO_SIDE_INDEX)
		return nullptr;

	return GetFrontSector(&Sides[sidenum]);
}

IntSector *FLevel::GetSectorFromSubSector(const MapSubsectorEx *sub)
{
	for (int i = 0; i < (int)sub->numlines; i++)
	{
		MapSegGLEx *seg = &GLSegs[sub->firstline + i];
		if (seg->side != NO_SIDE_INDEX)
		{
			IntLineDef *line = &Lines[seg->linedef];
			return GetFrontSector(&Sides[line->sidenum[seg->side]]);
		}
	}
	return nullptr;
}

MapSubsectorEx *FLevel::PointInSubSector(const int x, const int y)
{
	MapNodeEx   *node;
	int         side;
	int         nodenum;
	vec3     dp1;
	vec3     dp2;
	float       d;

	// single subsector is a special case
	if (!NumGLNodes)
	{
		return &GLSubsectors[0];
	}

	nodenum = NumGLNodes - 1;

	while (!(nodenum & NFX_SUBSECTOR))
	{
		node = &GLNodes[nodenum];

		vec3 pt1(F(node->x), F(node->y), 0);
		vec3 pt2(F(node->dx), F(node->dy), 0);
		//vec3 pt1(F(node->x << 16), F(node->y << 16), 0);
		//vec3 pt2(F(node->dx << 16), F(node->dy << 16), 0);
		vec3 pos(F(x << 16), F(y << 16), 0);

		dp1 = pt1 - pos;
		dp2 = (pt2 + pt1) - pos;
		d = cross(dp1, dp2).z;

		side = FLOATSIGNBIT(d);

		nodenum = node->children[side ^ 1];
	}

	return &GLSubsectors[nodenum & ~NFX_SUBSECTOR];
}

FloatVertex FLevel::GetSegVertex(int index)
{
	if (index & 0x8000)
	{
		index = (index & 0x7FFF) + NumGLVertices;
	}

	FloatVertex v;
	v.x = F(GLVertices[index].x);
	v.y = F(GLVertices[index].y);
	return v;
}

void FLevel::CreateLights()
{
	// add lights from thing sources
	for (int i = 0; i < (int)Things.Size(); ++i)
	{
		IntThing *thing = &Things[i];

		// skip things that aren't actually static point lights or static spotlights
		if (thing->type != THING_POINTLIGHT_STATIC && thing->type != THING_SPOTLIGHT_STATIC)
			continue;

		uint32_t lightcolor = 0xffffff;
		float lightintensity = 1.0f;
		float lightdistance = 0.0f;
		float innerAngleCos = -1.0f;
		float outerAngleCos = -1.0f;

		// need to process point lights and spot lights differently due to their
		// inconsistent arg usage...
		if (thing->type == THING_POINTLIGHT_STATIC)
		{
			int r = thing->args[0];
			int g = thing->args[1];
			int b = thing->args[2];
			int rgb = r;
			rgb = rgb << 8;
			rgb |= g;
			rgb = rgb << 8;
			rgb |= b;
			lightcolor = (uint32_t)rgb;
		}
		else if (thing->type == THING_SPOTLIGHT_STATIC)
		{
			lightcolor = (uint32_t)thing->args[0];

			// UDB's color picker will assign the color as a hex string, stored
			// in the arg0str field. detect this, so that it can be converted into an int
			if (thing->arg0str.Len() > 0)
			{
				FString hex = "0x" + thing->arg0str;
				int rgb = hex.ToULong();
				lightcolor = (uint32_t)rgb;
			}

			innerAngleCos = std::cos((float)thing->args[1] * 3.14159265359f / 180.0f);
			outerAngleCos = std::cos((float)thing->args[2] * 3.14159265359f / 180.0f);
		}

		// this is known as "intensity" on dynamic lights (and in UDB)
		lightdistance = thing->args[3];

		// static light intensity (not to be confused with dynamic lights' intensity, which is actually static light distance
		lightintensity = thing->alpha;

		if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
		{
			int x = thing->x >> FRACBITS;
			int y = thing->y >> FRACBITS;

			ThingLight thingLight;
			thingLight.mapThing = thing;
			thingLight.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
			thingLight.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
			thingLight.rgb.z = (lightcolor & 0xff) / 255.0f;
			thingLight.intensity = lightintensity;
			thingLight.innerAngleCos = std::max(innerAngleCos, outerAngleCos);
			thingLight.outerAngleCos = outerAngleCos;
			thingLight.radius = lightdistance;
			thingLight.height = thing->height;
			thingLight.bCeiling = false;
			thingLight.ssect = PointInSubSector(x, y);
			thingLight.sector = GetSectorFromSubSector(thingLight.ssect);
			thingLight.origin.x = x;
			thingLight.origin.y = y;

			ThingLights.Push(thingLight);
		}
	}

	printf("Thing lights: %i\n", (int)ThingLights.Size());

	// add surface lights (temporarily disabled)
	for (unsigned int i = 0; i < Sides.Size(); i++)
	{
		IntSideDef *side = &Sides[i];
		side->lightdef = -1;

		IntLineDef *line = side->line;
		if (line)
		{
			uint32_t lightcolor = 0xffffff;
			float lightintensity = 1.0f;
			float lightdistance = 0.0f;

			/*
			for (unsigned int propIndex = 0; propIndex < line->props.Size(); propIndex++)
			{
				const UDMFKey &key = line->props[propIndex];
				if (!stricmp(key.key, "lightcolorline"))
				{
					lightcolor = atoi(key.value);
				}
				else if (!stricmp(key.key, "lightintensityline"))
				{
					lightintensity = atof(key.value);
				}
				else if (!stricmp(key.key, "lightdistanceline"))
				{
					lightdistance = atof(key.value);
				}
			}

			if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
			{
				SurfaceLightDef desc;
				desc.intensity = lightintensity;
				desc.distance = lightdistance;
				desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
				desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
				desc.rgb.z = (lightcolor & 0xff) / 255.0f;
				side->lightdef = SurfaceLights.Push(desc);
			}
			*/
		}
	}

	for (unsigned int i = 0; i < Sectors.Size(); i++)
	{
		IntSector *sector = &Sectors[i];

		sector->floorlightdef = -1;
		sector->ceilinglightdef = -1;

		uint32_t lightcolor = 0xffffff;
		float lightintensity = 1.0f;
		float lightdistance = 0.0f;

		/*
		for (unsigned int propIndex = 0; propIndex < sector->props.Size(); propIndex++)
		{
			const UDMFKey &key = sector->props[propIndex];
			if (!stricmp(key.key, "lm_lightcolorfloor"))
			{
				lightcolor = atoi(key.value);
			}
			else if (!stricmp(key.key, "lm_lightintensityfloor"))
			{
				lightintensity = atof(key.value);
			}
			else if (!stricmp(key.key, "lm_lightdistancefloor"))
			{
				lightdistance = atof(key.value);
			}
		}

		if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
		{
			SurfaceLightDef desc;
			desc.intensity = lightintensity;
			desc.distance = lightdistance;
			desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
			desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
			desc.rgb.z = (lightcolor & 0xff) / 255.0f;
			sector->floorlightdef = SurfaceLights.Push(desc);
		}

		lightcolor = 0xffffff;
		lightintensity = 1.0f;
		lightdistance = 0.0f;

		for (unsigned int propIndex = 0; propIndex < sector->props.Size(); propIndex++)
		{
			const UDMFKey &key = sector->props[propIndex];
			if (!stricmp(key.key, "lm_lightcolorceiling"))
			{
				lightcolor = atoi(key.value);
			}
			else if (!stricmp(key.key, "lm_lightintensityceiling"))
			{
				lightintensity = atof(key.value);
			}
			else if (!stricmp(key.key, "lm_lightdistanceceiling"))
			{
				lightdistance = atof(key.value);
			}
		}

		if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
		{
			SurfaceLightDef desc;
			desc.intensity = lightintensity;
			desc.distance = lightdistance;
			desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
			desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
			desc.rgb.z = (lightcolor & 0xff) / 255.0f;
			sector->ceilinglightdef = SurfaceLights.Push(desc);
		}
		*/
	}

	//printf("Surface lights: %i\n", (int)SurfaceLights.Size());
}

vec3 FLevel::GetLightProbePosition(int index)
{
	int thingIndex = ThingLightProbes[index];
	const IntThing& thing = Things[thingIndex];

	float x = (float)(thing.x >> FRACBITS);
	float y = (float)(thing.y >> FRACBITS);
	float z = 0.0f;

	MapSubsectorEx* ssect = PointInSubSector(x, y);
	if (ssect)
	{
		IntSector* sector = GetSectorFromSubSector(ssect);
		if (sector)
		{
			z = sector->floorplane.zAt(x, y) + thing.height;
		}
	}
	return vec3(x, y, z);
}
