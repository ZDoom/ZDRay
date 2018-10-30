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

#include "common.h"
#include "wad.h"
#include "mapdata.h"
#include "lightsurface.h"

static const kexVec3 defaultSunColor(1, 1, 1);
static const kexVec3 defaultSunDirection(0.45f, 0.3f, 0.9f);

void FLevel::SetupDlight()
{
	BuildNodeBounds();
	BuildLeafs();
	BuildPVS();
	CheckSkySectors();
}

void FLevel::BuildNodeBounds()
{
	int     i;
	int     j;
	kexVec3 point;
	float   high = -M_INFINITY;
	float   low = M_INFINITY;

	nodeBounds = (kexBBox*)Mem_Calloc(sizeof(kexBBox) * NumGLNodes, hb_static);

	for (i = 0; i < (int)Sectors.Size(); ++i)
	{
		if (Sectors[i].data.ceilingheight > high)
		{
			high = Sectors[i].data.ceilingheight;
		}
		if (Sectors[i].data.floorheight < low)
		{
			low = Sectors[i].data.floorheight;
		}
	}

	for (i = 0; i < NumGLNodes; ++i)
	{
		nodeBounds[i].Clear();

		for (j = 0; j < 2; ++j)
		{
			point.Set(GLNodes[i].bbox[j][BOXLEFT], GLNodes[i].bbox[j][BOXBOTTOM], low);
			nodeBounds[i].AddPoint(point);
			point.Set(GLNodes[i].bbox[j][BOXRIGHT], GLNodes[i].bbox[j][BOXTOP], high);
			nodeBounds[i].AddPoint(point);
		}
	}
}

void FLevel::BuildLeafs()
{
	MapSubsectorEx  *ss;
	leaf_t          *lf;
	int             i;
	int             j;
	kexVec3         point;
	IntSector     *sector;
	int             count;

	leafs = (leaf_t*)Mem_Calloc(sizeof(leaf_t*) * NumGLSegs * 2, hb_static);
	numLeafs = NumGLSubsectors;

	ss = GLSubsectors;

	segLeafLookup = (int*)Mem_Calloc(sizeof(int) * NumGLSegs, hb_static);
	ssLeafLookup = (int*)Mem_Calloc(sizeof(int) * NumGLSubsectors, hb_static);
	ssLeafCount = (int*)Mem_Calloc(sizeof(int) * NumGLSubsectors, hb_static);
	ssLeafBounds = (kexBBox*)Mem_Calloc(sizeof(kexBBox) * NumGLSubsectors, hb_static);

	count = 0;

	for (i = 0; i < NumGLSubsectors; ++i, ++ss)
	{
		ssLeafCount[i] = ss->numlines;
		ssLeafLookup[i] = ss->firstline;

		ssLeafBounds[i].Clear();
		sector = GetSectorFromSubSector(ss);

		if (ss->numlines)
		{
			for (j = 0; j < (int)ss->numlines; ++j)
			{
				MapSegGLEx *seg = &GLSegs[ss->firstline + j];
				lf = &leafs[count++];

				segLeafLookup[ss->firstline + j] = i;

				lf->vertex = GetSegVertex(seg->v1);
				lf->seg = seg;

				point.Set(lf->vertex.x, lf->vertex.y, sector->data.floorheight);
				ssLeafBounds[i].AddPoint(point);

				point.z = sector->data.ceilingheight;
				ssLeafBounds[i].AddPoint(point);
			}
		}
	}
}

void FLevel::BuildPVS()
{
	// don't do anything if already loaded
	if (mapPVS != NULL)
	{
		return;
	}

	int len = ((NumGLSubsectors + 7) / 8) * NumGLSubsectors;
	mapPVS = (byte*)Mem_Malloc(len, hb_static);
	memset(mapPVS, 0xff, len);
}

void FLevel::CheckSkySectors()
{
	char name[9];

	bSkySectors = (bool*)Mem_Calloc(sizeof(bool) * Sectors.Size(), hb_static);
	bSSectsVisibleToSky = (bool*)Mem_Calloc(sizeof(bool) * NumGLSubsectors, hb_static);

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

	// try to early out by quickly checking which subsector can potentially
	// see a sky sector
	for (int i = 0; i < NumGLSubsectors; ++i)
	{
		for (int j = 0; j < NumGLSubsectors; ++j)
		{
			IntSector *sec = GetSectorFromSubSector(&GLSubsectors[j]);

			if (bSkySectors[sec - &Sectors[0]] == false)
			{
				continue;
			}

			if (CheckPVS(&GLSubsectors[i], &GLSubsectors[j]))
			{
				bSSectsVisibleToSky[i] = true;
				break;
			}
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

IntSideDef *FLevel::GetSideDef(const MapSegGLEx *seg)
{
	if (seg->linedef == NO_LINE_INDEX)
	{
		// skip minisegs
		return NULL;
	}

	IntLineDef *line = &Lines[seg->linedef];
	return &Sides[line->sidenum[seg->side]];
}

IntSector *FLevel::GetFrontSector(const MapSegGLEx *seg)
{
	IntSideDef *side = GetSideDef(seg);

	if (side == NULL)
	{
		return NULL;
	}

	return &Sectors[side->sector];
}

IntSector *FLevel::GetBackSector(const MapSegGLEx *seg)
{
	if (seg->linedef == NO_LINE_INDEX)
	{
		// skip minisegs
		return NULL;
	}

	IntLineDef *line = &Lines[seg->linedef];

	if ((line->flags & ML_TWOSIDED) && line->sidenum[seg->side ^ 1] != 0xffffffff)
	{
		IntSideDef *backSide = &Sides[line->sidenum[seg->side ^ 1]];
		return &Sectors[backSide->sector];
	}

	return NULL;
}

IntSector *FLevel::GetSectorFromSubSector(const MapSubsectorEx *sub)
{
	IntSector *sector = NULL;

	// try to find a sector that the subsector belongs to
	for (int i = 0; i < (int)sub->numlines; i++)
	{
		MapSegGLEx *seg = &GLSegs[sub->firstline + i];
		if (seg->side != NO_SIDE_INDEX)
		{
			sector = GetFrontSector(seg);
			break;
		}
	}

	return sector;
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

bool FLevel::PointInsideSubSector(const float x, const float y, const MapSubsectorEx *sub)
{
	surface_t *surf;
	int i;
	kexVec2 p(x, y);
	kexVec2 dp1, dp2;
	kexVec2 pt1, pt2;

	surf = leafSurfaces[0][sub - GLSubsectors];
	if (!surf)
		return false;

	// check to see if the point is inside the subsector leaf
	for (i = 0; i < surf->numVerts; i++)
	{
		pt1 = surf->verts[i].ToVec2();
		pt2 = surf->verts[(i + 1) % surf->numVerts].ToVec2();

		dp1 = pt1 - p;
		dp2 = pt2 - p;

		if (dp1.CrossScalar(dp2) < 0)
		{
			continue;
		}

		// this point is outside the subsector leaf
		return false;
	}

	return true;
}

bool FLevel::LineIntersectSubSector(const kexVec3 &start, const kexVec3 &end, const MapSubsectorEx *sub, kexVec2 &out)
{
	surface_t *surf;
	kexVec2 p1, p2;
	kexVec2 s1, s2;
	kexVec2 pt;
	kexVec2 v;
	float d, u;
	float newX;
	float ab;
	int i;

	surf = leafSurfaces[0][sub - GLSubsectors];
	p1 = start.ToVec2();
	p2 = end.ToVec2();

	for (i = 0; i < surf->numVerts; i++)
	{
		s1 = surf->verts[i].ToVec2();
		s2 = surf->verts[(i + 1) % surf->numVerts].ToVec2();

		if ((p1 == p2) || (s1 == s2))
		{
			// zero length
			continue;
		}

		if ((p1 == s1) || (p2 == s1) || (p1 == s2) || (p2 == s2))
		{
			// shares end point
			continue;
		}

		// translate to origin
		pt = p2 - p1;
		s1 -= p1;
		s2 -= p1;

		// normalize
		u = pt.UnitSq();
		d = kexMath::InvSqrt(u);
		v = (pt * d);

		// rotate points s1 and s2 so they're on the positive x axis
		newX = s1.Dot(v);
		s1.y = s1.CrossScalar(v);
		s1.x = newX;

		newX = s2.Dot(v);
		s2.y = s2.CrossScalar(v);
		s2.x = newX;

		if ((s1.y < 0 && s2.y < 0) || (s1.y >= 0 && s2.y >= 0))
		{
			// s1 and s2 didn't cross
			continue;
		}

		ab = s2.x + (s1.x - s2.x) * s2.y / (s2.y - s1.y);

		if (ab < 0 || ab >(u * d))
		{
			// s1 and s2 crosses but outside of points p1 and p2
			continue;
		}

		// intersected
		out = p1 + (v * ab);
		return true;
	}

	return false;
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

bool FLevel::CheckPVS(MapSubsectorEx *s1, MapSubsectorEx *s2)
{
	uint8_t *vis;
	int n1, n2;

	n1 = s1 - GLSubsectors;
	n2 = s2 - GLSubsectors;

	vis = &mapPVS[(((NumGLSubsectors + 7) / 8) * n1)];

	return ((vis[n2 >> 3] & (1 << (n2 & 7))) != 0);
}

void FLevel::CreateLights()
{
	thingLight_t *thingLight;
	unsigned int j;
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
			thingLight->falloff = 1.0f;
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
	for (j = 0; j < surfaces.Length(); ++j)
	{
		surface_t *surface = surfaces[j];

		if (surface->type >= ST_MIDDLESEG && surface->type <= ST_LOWERSEG)
		{
			IntLineDef *line = nullptr;
			if (GLSegs[surface->typeIndex].linedef != NO_LINE_INDEX)
				line = &Lines[GLSegs[surface->typeIndex].linedef];

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
					desc.tag = 0;
					desc.outerCone = 0.0f;
					desc.innerCone = 0.0f;
					desc.falloff = 1.0f;
					desc.intensity = lightintensity;
					desc.distance = lightdistance;
					desc.bIgnoreCeiling = false;
					desc.bIgnoreFloor = false;
					desc.bNoCenterPoint = false;
					desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
					desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
					desc.rgb.z = (lightcolor & 0xff) / 255.0f;

					kexLightSurface *lightSurface = new kexLightSurface();
					lightSurface->Init(desc, surface, true, false);
					lightSurface->CreateCenterOrigin();
					lightSurfaces.Push(lightSurface);
					numSurfLights++;
				}
			}
		}
		else if (surface->type == ST_FLOOR || surface->type == ST_CEILING)
		{
			MapSubsectorEx *sub = surface->subSector;
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
					desc.tag = 0;
					desc.outerCone = 0.0f;
					desc.innerCone = 0.0f;
					desc.falloff = 1.0f;
					desc.intensity = lightintensity;
					desc.distance = lightdistance;
					desc.bIgnoreCeiling = false;
					desc.bIgnoreFloor = false;
					desc.bNoCenterPoint = false;
					desc.rgb.x = ((lightcolor >> 16) & 0xff) / 255.0f;
					desc.rgb.y = ((lightcolor >> 8) & 0xff) / 255.0f;
					desc.rgb.z = (lightcolor & 0xff) / 255.0f;

					kexLightSurface *lightSurface = new kexLightSurface();
					lightSurface->Init(desc, surface, false, desc.bNoCenterPoint);
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
