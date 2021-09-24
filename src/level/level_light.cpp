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

static const Vec3 defaultSunColor(1, 1, 1);
static const Vec3 defaultSunDirection(0.45f, 0.3f, 0.9f);

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

const Vec3 &FLevel::GetSunColor() const
{
	return defaultSunColor;
}

const Vec3 &FLevel::GetSunDirection() const
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
	Vec3     dp1;
	Vec3     dp2;
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

		Vec3 pt1(F(node->x), F(node->y), 0);
		Vec3 pt2(F(node->dx), F(node->dy), 0);
		//Vec3 pt1(F(node->x << 16), F(node->y << 16), 0);
		//Vec3 pt2(F(node->dx << 16), F(node->dy << 16), 0);
		Vec3 pos(F(x << 16), F(y << 16), 0);

		dp1 = pt1 - pos;
		dp2 = (pt2 + pt1) - pos;
		d = dp1.Cross(dp2).z;

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

		uint32_t lightcolor = 0xffffff;
		float lightintensity = 1.0f;
		float lightdistance = 0.0f;
		float innerAngleCos = -1.0f;
		float outerAngleCos = -1.0f;

		for (unsigned int propIndex = 0; propIndex < thing->props.Size(); propIndex++)
		{
			const UDMFKey &key = thing->props[propIndex];
			if (!stricmp(key.key, "lightcolor"))
			{
				lightcolor = atoi(key.value);
			}
			else if (!stricmp(key.key, "lightintensity"))
			{
				lightintensity = atof(key.value);
			}
			else if (!stricmp(key.key, "lightdistance"))
			{
				lightdistance = atof(key.value);
			}
			else if (!stricmp(key.key, "lightinnerangle"))
			{
				innerAngleCos = std::cos(atof(key.value) * 3.14159265359f / 180.0f);
			}
			else if (!stricmp(key.key, "lightouterangle"))
			{
				outerAngleCos = std::cos(atof(key.value) * 3.14159265359f / 180.0f);
			}
		}

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
			thingLight.origin.Set(x, y);

			ThingLights.Push(thingLight);
		}
	}

	printf("Thing lights: %i\n", (int)ThingLights.Size());

	// add surface lights
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

			for (unsigned int propIndex = 0; propIndex < line->props.Size(); propIndex++)
			{
				const UDMFKey &key = line->props[propIndex];
				if (!stricmp(key.key, "lightcolor"))
				{
					lightcolor = atoi(key.value);
				}
				else if (!stricmp(key.key, "lightintensity"))
				{
					lightintensity = atof(key.value);
				}
				else if (!stricmp(key.key, "lightdistance"))
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

		for (unsigned int propIndex = 0; propIndex < sector->props.Size(); propIndex++)
		{
			const UDMFKey &key = sector->props[propIndex];
			if (!stricmp(key.key, "lightcolorfloor"))
			{
				lightcolor = atoi(key.value);
			}
			else if (!stricmp(key.key, "lightintensityfloor"))
			{
				lightintensity = atof(key.value);
			}
			else if (!stricmp(key.key, "lightdistancefloor"))
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
			if (!stricmp(key.key, "lightcolorceiling"))
			{
				lightcolor = atoi(key.value);
			}
			else if (!stricmp(key.key, "lightintensityceiling"))
			{
				lightintensity = atof(key.value);
			}
			else if (!stricmp(key.key, "lightdistanceceiling"))
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
	}

	printf("Surface lights: %i\n", (int)SurfaceLights.Size());
}
