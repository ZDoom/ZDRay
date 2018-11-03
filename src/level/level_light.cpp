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
// DESCRIPTION: General doom map utilities and data preperation
//
//-----------------------------------------------------------------------------

#include "math/mathlib.h"
#include "level/level.h"
#include "lightmap/lightsurface.h"
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

static const kexVec3 defaultSunColor(1, 1, 1);
static const kexVec3 defaultSunDirection(0.45f, 0.3f, 0.9f);

void FLevel::SetupDlight()
{
	CheckSkySectors();
}

void FLevel::CheckSkySectors()
{
	char name[9];

	bSkySectors.resize(Sectors.Size());

	for (int i = 0; i < (int)Sectors.Size(); ++i)
	{
		//if (mapDef && mapDef->sunIgnoreTag != 0 && Sectors[i].data.tag == mapDef->sunIgnoreTag)
		//	continue;

		strncpy(name, Sectors[i].data.ceilingpic, 8);
		name[8] = 0;

		if (!strncmp(name, "F_SKY001", 8) || !strncmp(name, "F_SKY1", 8) || !strncmp(name, "F_SKY", 8))
		{
			bSkySectors[i] = true;
		}
	}
}

const kexVec3 &FLevel::GetSunColor() const
{
	return defaultSunColor;
}

const kexVec3 &FLevel::GetSunDirection() const
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
	kexVec3     dp1;
	kexVec3     dp2;
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

		kexVec3 pt1(F(node->x), F(node->y), 0);
		kexVec3 pt2(F(node->dx), F(node->dy), 0);
		//kexVec3 pt1(F(node->x << 16), F(node->y << 16), 0);
		//kexVec3 pt2(F(node->dx << 16), F(node->dy << 16), 0);
		kexVec3 pos(F(x << 16), F(y << 16), 0);

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
	thingLight_t *thingLight;
	size_t j;
	int numSurfLights;
	kexVec2 pt;

	//
	// add lights from thing sources
	//
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
				innerAngleCos = std::cosf(atof(key.value) * 3.14159265359f / 180.0f);
			}
			else if (!stricmp(key.key, "lightouterangle"))
			{
				outerAngleCos = std::cosf(atof(key.value) * 3.14159265359f / 180.0f);
			}
		}

		if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
		{
			int x = thing->x >> FRACBITS;
			int y = thing->y >> FRACBITS;

			thingLight = new thingLight_t();

			thingLight->mapThing = thing;
			thingLight->rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
			thingLight->rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
			thingLight->rgb.z = (lightcolor & 0xff) / 255.0f;
			thingLight->intensity = lightintensity;
			thingLight->innerAngleCos = std::max(innerAngleCos, outerAngleCos);
			thingLight->outerAngleCos = outerAngleCos;
			thingLight->radius = lightdistance;
			thingLight->height = thing->height;
			thingLight->bCeiling = false;
			thingLight->ssect = PointInSubSector(x, y);
			thingLight->sector = GetSectorFromSubSector(thingLight->ssect);

			thingLight->origin.Set(x, y);
			thingLights.Push(thingLight);
		}
	}

	printf("Thing lights: %i\n", thingLights.Size());

	numSurfLights = 0;

	//
	// add surface lights
	//
	for (j = 0; j < surfaces.size(); ++j)
	{
		surface_t *surface = surfaces[j];

		if (surface->type >= ST_MIDDLESIDE && surface->type <= ST_LOWERSIDE)
		{
			IntLineDef *line = Sides[surface->typeIndex].line;
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
					surfaceLightDef desc;
					desc.intensity = lightintensity;
					desc.distance = lightdistance;
					desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
					desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
					desc.rgb.z = (lightcolor & 0xff) / 255.0f;

					kexLightSurface *lightSurface = new kexLightSurface();
					lightSurface->Init(desc, surface, true);
					lightSurface->Subdivide(16);
					//lightSurface->CreateCenterOrigin();
					lightSurfaces.Push(lightSurface);
					numSurfLights++;
				}
			}
		}
		else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
		{
			MapSubsectorEx *sub = &GLSubsectors[surface->typeIndex];
			IntSector *sector = GetSectorFromSubSector(sub);

			if (sector && surface->numVerts > 0)
			{
				uint32_t lightcolor = 0xffffff;
				float lightintensity = 1.0f;
				float lightdistance = 0.0f;

				for (unsigned int propIndex = 0; propIndex < sector->props.Size(); propIndex++)
				{
					const UDMFKey &key = sector->props[propIndex];
					if (surface->type == ST_FLOOR)
					{
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
					else
					{
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
				}

				if (lightdistance > 0.0f && lightintensity > 0.0f && lightcolor != 0)
				{
					surfaceLightDef desc;
					desc.intensity = lightintensity;
					desc.distance = lightdistance;
					desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
					desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
					desc.rgb.z = (lightcolor & 0xff) / 255.0f;

					kexLightSurface *lightSurface = new kexLightSurface();
					lightSurface->Init(desc, surface, false);
					lightSurface->Subdivide(16);
					lightSurfaces.Push(lightSurface);
					numSurfLights++;
				}
			}
		}
	}

	printf("Surface lights: %i\n", numSurfLights);
}

void FLevel::CleanupThingLights()
{
	for (unsigned int i = 0; i < thingLights.Size(); i++)
	{
		delete thingLights[i];
	}
}

LevelTraceHit FLevel::Trace(const kexVec3 &startVec, const kexVec3 &endVec)
{
	TraceHit hit = TriangleMeshShape::find_first_hit(CollisionMesh.get(), startVec, endVec);

	LevelTraceHit trace;
	trace.start = startVec;
	trace.end = endVec;
	trace.fraction = hit.fraction;
	trace.hitSurface = (trace.fraction < 1.0f) ? surfaces[hit.surface] : nullptr;
	return trace;
}
