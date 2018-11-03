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
// DESCRIPTION: Prepares geometry from map structures
//
//-----------------------------------------------------------------------------

#include "common.h"
#include "mapdata.h"
#include "surfaces.h"

std::vector<surface_t*> surfaces;

static void CreateSideSurfaces(FLevel &doomMap, IntSideDef *side)
{
	surface_t *surf;
	IntSector *front;
	IntSector *back;

	front = doomMap.GetFrontSector(side);
	back = doomMap.GetBackSector(side);

	if (front->controlsector)
		return;

	FloatVertex v1 = doomMap.GetSegVertex(side->line->v1);
	FloatVertex v2 = doomMap.GetSegVertex(side->line->v2);

	if (side->line->sidenum[0] != (ptrdiff_t)(side - &doomMap.Sides[0]))
	{
		std::swap(v1, v2);
	}

	float v1Top = front->ceilingplane.zAt(v1.x, v1.y);
	float v1Bottom = front->floorplane.zAt(v1.x, v1.y);
	float v2Top = front->ceilingplane.zAt(v2.x, v2.y);
	float v2Bottom = front->floorplane.zAt(v2.x, v2.y);

	if (back)
	{
		float v1TopBack = back->ceilingplane.zAt(v1.x, v1.y);
		float v1BottomBack = back->floorplane.zAt(v1.x, v1.y);
		float v2TopBack = back->ceilingplane.zAt(v2.x, v2.y);
		float v2BottomBack = back->floorplane.zAt(v2.x, v2.y);

		if (v1Top == v1TopBack && v1Bottom == v1BottomBack && v2Top == v2TopBack && v2Bottom == v2BottomBack)
		{
			return;
		}

		// bottom seg
		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			if (side->bottomtexture[0] != '-')
			{
				surf = (surface_t*)Mem_Calloc(sizeof(surface_t), hb_static);
				surf->numVerts = 4;
				surf->verts = (kexVec3*)Mem_Calloc(sizeof(kexVec3) * 4, hb_static);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1Bottom;
				surf->verts[1].z = v2Bottom;
				surf->verts[2].z = v1BottomBack;
				surf->verts[3].z = v2BottomBack;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_LOWERSIDE;
				surf->typeIndex = side - &doomMap.Sides[0];

				surfaces.push_back(surf);
			}

			v1Bottom = v1BottomBack;
			v2Bottom = v2BottomBack;
		}

		// top seg
		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			bool bSky = false;
			int frontidx = front - &doomMap.Sectors[0];
			int backidx = back - &doomMap.Sectors[0];

			if (doomMap.bSkySectors[frontidx] && doomMap.bSkySectors[backidx])
			{
				if (front->data.ceilingheight != back->data.ceilingheight && side->toptexture[0] == '-')
				{
					bSky = true;
				}
			}

			if (side->toptexture[0] != '-' || bSky)
			{
				surf = (surface_t*)Mem_Calloc(sizeof(surface_t), hb_static);
				surf->numVerts = 4;
				surf->verts = (kexVec3*)Mem_Calloc(sizeof(kexVec3) * 4, hb_static);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1TopBack;
				surf->verts[1].z = v2TopBack;
				surf->verts[2].z = v1Top;
				surf->verts[3].z = v2Top;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_UPPERSIDE;
				surf->typeIndex = side - &doomMap.Sides[0];
				surf->bSky = bSky;

				surfaces.push_back(surf);
			}

			v1Top = v1TopBack;
			v2Top = v2TopBack;
		}
	}

	// middle seg
	if (back == NULL)
	{
		surf = (surface_t*)Mem_Calloc(sizeof(surface_t), hb_static);
		surf->numVerts = 4;
		surf->verts = (kexVec3*)Mem_Calloc(sizeof(kexVec3) * 4, hb_static);

		surf->verts[0].x = surf->verts[2].x = v1.x;
		surf->verts[0].y = surf->verts[2].y = v1.y;
		surf->verts[1].x = surf->verts[3].x = v2.x;
		surf->verts[1].y = surf->verts[3].y = v2.y;
		surf->verts[0].z = v1Bottom;
		surf->verts[1].z = v2Bottom;
		surf->verts[2].z = v1Top;
		surf->verts[3].z = v2Top;

		surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
		surf->plane.SetDistance(surf->verts[0]);
		surf->type = ST_MIDDLESIDE;
		surf->typeIndex = side - &doomMap.Sides[0];

		surfaces.push_back(surf);
	}
}

static void CreateSubsectorSurfaces(FLevel &doomMap)
{
	surface_t *surf;
	IntSector *sector = NULL;
	int i;
	int j;

	printf("------------- Building subsector surfaces -------------\n");

	for (i = 0; i < doomMap.NumGLSubsectors; i++)
	{
		printf("subsectors: %i / %i\r", i + 1, doomMap.NumGLSubsectors);

		MapSubsectorEx *sub = &doomMap.GLSubsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		sector = doomMap.GetSectorFromSubSector(sub);

		// I will be NOT surprised if some users tries to do something stupid with
		// sector hacks
		if (sector == NULL)
		{
			Error("CreateSubsectorSurfaces: subsector %i has no sector\n", i);
			return;
		}

		if (sector->controlsector)
			continue;

		surf = (surface_t*)Mem_Calloc(sizeof(surface_t), hb_static);
		surf->numVerts = sub->numlines;
		surf->verts = (kexVec3*)Mem_Calloc(sizeof(kexVec3) * surf->numVerts, hb_static);

		// floor verts
		for (j = 0; j < surf->numVerts; j++)
		{
			MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + (surf->numVerts - 1) - j];
			FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

			surf->verts[j].x = v1.x;
			surf->verts[j].y = v1.y;
			surf->verts[j].z = sector->floorplane.zAt(surf->verts[j].x, surf->verts[j].y);
		}

		surf->plane = sector->floorplane;
		surf->type = ST_FLOOR;
		surf->typeIndex = i;

		surfaces.push_back(surf);

		surf = (surface_t*)Mem_Calloc(sizeof(surface_t), hb_static);
		surf->numVerts = sub->numlines;
		surf->verts = (kexVec3*)Mem_Calloc(sizeof(kexVec3) * surf->numVerts, hb_static);

		if (doomMap.bSkySectors[sector - &doomMap.Sectors[0]])
		{
			surf->bSky = true;
		}

		// ceiling verts
		for (j = 0; j < surf->numVerts; j++)
		{
			MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + j];
			FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

			surf->verts[j].x = v1.x;
			surf->verts[j].y = v1.y;
			surf->verts[j].z = sector->ceilingplane.zAt(surf->verts[j].x, surf->verts[j].y);
		}

		surf->plane = sector->ceilingplane;
		surf->type = ST_CEILING;
		surf->typeIndex = i;

		surfaces.push_back(surf);
	}

	printf("\nLeaf surfaces: %i\n", (int)surfaces.size() - doomMap.NumGLSubsectors);
}

static bool IsDegenerate(const kexVec3 &v0, const kexVec3 &v1, const kexVec3 &v2)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = v1.x - v0.x;
	float ay = v1.y - v0.y;
	float az = v1.z - v0.z;
	float bx = v2.x - v0.x;
	float by = v2.y - v0.y;
	float bz = v2.z - v0.z;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

void CreateSurfaces(FLevel &doomMap)
{
	for (size_t i = 0; i < surfaces.size(); i++)
		Mem_Free(surfaces[i]);
	surfaces = {};

	for (unsigned int i = 0; i < doomMap.Sectors.Size(); i++)
		doomMap.Sectors[i].controlsector = false;

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
		doomMap.Sides[i].line = nullptr;

	for (unsigned int i = 0; i < doomMap.Lines.Size(); i++)
	{
		IntLineDef *line = &doomMap.Lines[i];

		// Link sides to lines
		if (line->sidenum[0] < doomMap.Sides.Size())
			doomMap.Sides[line->sidenum[0]].line = line;
		if (line->sidenum[1] < doomMap.Sides.Size())
			doomMap.Sides[line->sidenum[1]].line = line;

		if (line->special == 160) // Sector_Set3dFloor
		{
			int sectorTag = line->args[0];
			int type = line->args[1];
			//int opacity = line.args[3];

			IntSector *controlsector = &doomMap.Sectors[doomMap.Sides[doomMap.Lines[i].sidenum[0]].sector];
			controlsector->controlsector = true;

			for (unsigned int j = 0; j < doomMap.Sectors.Size(); j++)
			{
				if (doomMap.Sectors[j].data.tag == sectorTag)
				{
					doomMap.Sectors[j].x3dfloors.Push(controlsector);
				}
			}
		}
	}

	printf("------------- Building side surfaces -------------\n");

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
		printf("sides: %i / %i\r", i + 1, doomMap.Sides.Size());
	}

	printf("\nSide surfaces: %i\n", (int)surfaces.size());

	CreateSubsectorSurfaces(doomMap);

	printf("Surfaces total: %i\n\n", (int)surfaces.size());

	printf("Building collision mesh..\n\n");

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		const auto &s = surfaces[i];
		int numVerts = s->numVerts;
		unsigned int pos = doomMap.MeshVertices.Size();

		for (int j = 0; j < numVerts; j++)
			doomMap.MeshVertices.Push(s->verts[j]);

		if (s->type == ST_FLOOR || s->type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(s->verts[0], s->verts[j - 1], s->verts[j]))
				{
					doomMap.MeshElements.Push(pos);
					doomMap.MeshElements.Push(pos + j - 1);
					doomMap.MeshElements.Push(pos + j);
					doomMap.MeshSurfaces.Push(i);
				}
			}
		}
		else if (s->type == ST_MIDDLESIDE || s->type == ST_UPPERSIDE || s->type == ST_LOWERSIDE)
		{
			if (!IsDegenerate(s->verts[0], s->verts[1], s->verts[2]))
			{
				doomMap.MeshElements.Push(pos + 0);
				doomMap.MeshElements.Push(pos + 1);
				doomMap.MeshElements.Push(pos + 2);
				doomMap.MeshSurfaces.Push(i);
			}
			if (!IsDegenerate(s->verts[1], s->verts[2], s->verts[3]))
			{
				doomMap.MeshElements.Push(pos + 1);
				doomMap.MeshElements.Push(pos + 2);
				doomMap.MeshElements.Push(pos + 3);
				doomMap.MeshSurfaces.Push(i);
			}
		}
	}

	doomMap.CollisionMesh.reset(new TriangleMeshShape(&doomMap.MeshVertices[0], doomMap.MeshVertices.Size(), &doomMap.MeshElements[0], doomMap.MeshElements.Size(), &doomMap.MeshSurfaces[0]));
}
