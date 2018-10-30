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
#include "kexlib/parser.h"
#include "mapdata.h"
#include "lightsurface.h"

static const kexVec3 defaultSunColor(1, 1, 1);
static const kexVec3 defaultSunDirection(0.45f, 0.3f, 0.9f);

void FLevel::SetupDlight()
{
	/*
	for (unsigned int i = 0; i < mapDefs.Size(); ++i)
	{
		if (mapDefs[i].map == wadFile.currentmap)
		{
			mapDef = &mapDefs[i];
			break;
		}
	}
	*/
	//mapDef = &mapDefs[0];

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
		if (mapDef && mapDef->sunIgnoreTag != 0 && Sectors[i].data.tag == mapDef->sunIgnoreTag)
		{
			continue;
		}

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
	if (mapDef != NULL)
	{
		return mapDef->sunColor;
	}

	return defaultSunColor;
}

const kexVec3 &FLevel::GetSunDirection() const
{
	if (mapDef != NULL)
	{
		return mapDef->sunDir;
	}

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

void FLevel::ParseConfigFile(const char *file)
{
	kexLexer *lexer;

	if (!(lexer = parser->Open(file)))
	{
		Error("FLevel::ParseConfigFile: %s not found\n", file);
		return;
	}

	while (lexer->CheckState())
	{
		lexer->Find();

		// check for mapdef block
		if (lexer->Matches("mapdef"))
		{
			mapDef_t mapDef;

			mapDef.map = -1;
			mapDef.sunIgnoreTag = 0;

			lexer->ExpectNextToken(TK_LBRACK);
			lexer->Find();

			while (lexer->TokenType() != TK_RBRACK)
			{
				if (lexer->Matches("map"))
				{
					mapDef.map = lexer->GetNumber();
				}
				else if (lexer->Matches("sun_ignore_tag"))
				{
					mapDef.sunIgnoreTag = lexer->GetNumber();
				}
				else if (lexer->Matches("sun_direction"))
				{
					mapDef.sunDir = lexer->GetVectorString3();
				}
				else if (lexer->Matches("sun_color"))
				{
					mapDef.sunColor = lexer->GetVectorString3();
					mapDef.sunColor /= 255.0f;
				}

				lexer->Find();
			}

			mapDefs.Push(mapDef);
		}

		// check for lightdef block
		if (lexer->Matches("lightdef"))
		{
			lightDef_t lightDef;

			lightDef.doomednum = -1;
			lightDef.height = 0;
			lightDef.intensity = 2;
			lightDef.falloff = 1;
			lightDef.bCeiling = false;

			lexer->ExpectNextToken(TK_LBRACK);
			lexer->Find();

			while (lexer->TokenType() != TK_RBRACK)
			{
				if (lexer->Matches("doomednum"))
				{
					lightDef.doomednum = lexer->GetNumber();
				}
				else if (lexer->Matches("rgb"))
				{
					lightDef.rgb = lexer->GetVectorString3();
					lightDef.rgb /= 255.0f;
				}
				else if (lexer->Matches("height"))
				{
					lightDef.height = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("radius"))
				{
					lightDef.radius = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("intensity"))
				{
					lightDef.intensity = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("falloff"))
				{
					lightDef.falloff = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("ceiling"))
				{
					lightDef.bCeiling = true;
				}

				lexer->Find();
			}

			lightDefs.Push(lightDef);
		}

		if (lexer->Matches("surfaceLight"))
		{
			surfaceLightDef surfaceLight;

			surfaceLight.tag = 0;
			surfaceLight.outerCone = 0.0f;
			surfaceLight.innerCone = 0.0f;
			surfaceLight.falloff = 1.0f;
			surfaceLight.intensity = 1.0f;
			surfaceLight.distance = 400.0f;
			surfaceLight.bIgnoreCeiling = false;
			surfaceLight.bIgnoreFloor = false;
			surfaceLight.bNoCenterPoint = false;
			surfaceLight.rgb.x = 1.0f;
			surfaceLight.rgb.y = 1.0f;
			surfaceLight.rgb.z = 1.0f;

			lexer->ExpectNextToken(TK_LBRACK);
			lexer->Find();

			while (lexer->TokenType() != TK_RBRACK)
			{
				if (lexer->Matches("tag"))
				{
					surfaceLight.tag = lexer->GetNumber();
				}
				else if (lexer->Matches("rgb"))
				{
					surfaceLight.rgb = lexer->GetVectorString3();
					surfaceLight.rgb /= 255.0f;
				}
				else if (lexer->Matches("cone_outer"))
				{
					surfaceLight.outerCone = (float)lexer->GetFloat() / 180.0f;
				}
				else if (lexer->Matches("cone_inner"))
				{
					surfaceLight.innerCone = (float)lexer->GetFloat() / 180.0f;
				}
				else if (lexer->Matches("falloff"))
				{
					surfaceLight.falloff = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("intensity"))
				{
					surfaceLight.intensity = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("distance"))
				{
					surfaceLight.distance = (float)lexer->GetFloat();
				}
				else if (lexer->Matches("bIgnoreCeiling"))
				{
					surfaceLight.bIgnoreCeiling = true;
				}
				else if (lexer->Matches("bIgnoreFloor"))
				{
					surfaceLight.bIgnoreFloor = true;
				}
				else if (lexer->Matches("bNoCenterPoint"))
				{
					surfaceLight.bNoCenterPoint = true;
				}

				lexer->Find();
			}

			surfaceLightDefs.Push(surfaceLight);
		}
	}

	// we're done with the file
	parser->Close();
}

void FLevel::CreateLights()
{
	IntThing *thing;
	thingLight_t *thingLight;
	unsigned int j;
	int numSurfLights;
	kexVec2 pt;

	//
	// add lights from thing sources
	//
	for (int i = 0; i < (int)Things.Size(); ++i)
	{
		lightDef_t *lightDef = NULL;

		thing = &Things[i];

		for (j = 0; j < (int)lightDefs.Size(); ++j)
		{
			if (thing->type == lightDefs[j].doomednum)
			{
				lightDef = &lightDefs[j];
				break;
			}
		}

		if (!lightDef)
		{
			continue;
		}

		if (lightDef->radius >= 0)
		{
			// ignore if all skills aren't set
			if (!(thing->flags & 7))
			{
				continue;
			}
		}

		int x = thing->x >> FRACBITS;
		int y = thing->y >> FRACBITS;

		thingLight = new thingLight_t;

		thingLight->mapThing = thing;
		thingLight->rgb = lightDef->rgb;
		thingLight->intensity = lightDef->intensity;
		thingLight->falloff = lightDef->falloff;
		thingLight->radius = lightDef->radius >= 0 ? lightDef->radius : thing->angle;
		thingLight->height = lightDef->height;
		thingLight->bCeiling = lightDef->bCeiling;
		thingLight->ssect = PointInSubSector(x, y);
		thingLight->sector = GetSectorFromSubSector(thingLight->ssect);

		thingLight->origin.Set(x, y);
		thingLights.Push(thingLight);
	}

	printf("Thing lights: %i\n", thingLights.Size());

	numSurfLights = 0;

	//
	// add surface lights
	//
	for (j = 0; j < surfaces.Length(); ++j)
	{
		surface_t *surface = surfaces[j];

		for (unsigned int k = 0; k < surfaceLightDefs.Size(); ++k)
		{
			surfaceLightDef *surfaceLightDef = &surfaceLightDefs[k];

			if (surface->type >= ST_MIDDLESEG && surface->type <= ST_LOWERSEG)
			{
				MapSegGLEx *seg = (MapSegGLEx*)surface->data;

				if (Lines[seg->linedef].args[1] == surfaceLightDef->tag)
				{
					kexLightSurface *lightSurface = new kexLightSurface;

					lightSurface->Init(*surfaceLightDef, surface, true, false);
					lightSurface->CreateCenterOrigin();
					lightSurfaces.Push(lightSurface);
					numSurfLights++;
				}
			}
			else
			{
				MapSubsectorEx *sub = surface->subSector;
				IntSector *sector = GetSectorFromSubSector(sub);

				if (!sector || surface->numVerts <= 0)
				{
					// eh....
					continue;
				}

				if (surface->type == ST_CEILING && surfaceLightDef->bIgnoreCeiling)
				{
					continue;
				}

				if (surface->type == ST_FLOOR && surfaceLightDef->bIgnoreFloor)
				{
					continue;
				}

				if (sector->data.tag == surfaceLightDef->tag)
				{
					kexLightSurface *lightSurface = new kexLightSurface;

					lightSurface->Init(*surfaceLightDef, surface, false, surfaceLightDef->bNoCenterPoint);
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
