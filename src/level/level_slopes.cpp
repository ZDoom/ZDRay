/*
** p_slopes.cpp
** Slope creation
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "level.h"

#include <map>

#define EQUAL_EPSILON (1/65536.)

// convert from fixed point(FRACUNIT) to floating point
#define F(x)  (((float)(x))/65536.0f)

inline int P_PointOnLineSidePrecise(const FVector2& p, const FVector2& v1, const FVector2& v2)
{
	return (p.Y - v1.Y) * (v2.X - v1.X) + (v1.X - p.X) * (v2.Y - v1.Y) > EQUAL_EPSILON;
}

inline int P_PointOnLineSidePrecise(const FVector2& p, const FloatVertex& v1, const FloatVertex& v2)
{
	return P_PointOnLineSidePrecise(p, FVector2(v1.x, v1.y), FVector2(v2.x, v2.y));
}

enum
{
	// Thing numbers used to define slopes
	SMT_VavoomFloor = 1500,
	SMT_VavoomCeiling = 1501,
	SMT_VertexFloorZ = 1504,
	SMT_VertexCeilingZ = 1505,
	SMT_SlopeFloorPointLine = 9500,
	SMT_SlopeCeilingPointLine = 9501,
	SMT_SetFloorSlope = 9502,
	SMT_SetCeilingSlope = 9503,
	SMT_CopyFloorPlane = 9510,
	SMT_CopyCeilingPlane = 9511,
};

void FProcessor::SetSlopesFromVertexHeights(IntThing* firstmt, IntThing* lastmt, const int* oldvertextable)
{
	TMap<int, double> vt_heights[2];
	IntThing* mt;
	bool vt_found = false;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type == SMT_VertexFloorZ || mt->type == SMT_VertexCeilingZ)
		{
			for (int i = 0; i < Level.NumVertices; i++)
			{
				auto vertex = Level.GetSegVertex(i);

				if(F(mt->x) == vertex.x && F(mt->y) == vertex.y)
				{
					if (mt->type == SMT_VertexFloorZ)
					{
						vt_heights[0][i] = F(mt->z);
					}
					else
					{
						vt_heights[1][i] = F(mt->z);
					}
					vt_found = true;
				}
			}
			mt->type = 0;
		}
	}

	for (unsigned i = 0; i < Level.VertexProps.Size(); i++)
	{
		auto& vertex = Level.VertexProps[i];

		int ii = oldvertextable == NULL ? i : oldvertextable[i];

		if (vertex.HasZCeiling())
		{
			vt_heights[1][ii] = vertex.zceiling;
			vt_found = true;
		}

		if (vertex.HasZFloor())
		{
			vt_heights[0][ii] = vertex.zfloor;
			vt_found = true;
		}
	}

	if (vt_found)
	{
		for (auto& sec : Level.Sectors)
		{
			if (sec.lines.Size() != 3) continue;	// only works with triangular sectors

			DVector3 vt1, vt2, vt3;
			DVector3 vec1, vec2;
			int vi1, vi2, vi3;

			vi1 = sec.lines[0]->v1;
			vi2 = sec.lines[0]->v2;
			vi3 = (sec.lines[1]->v1 == sec.lines[0]->v1 || sec.lines[1]->v1 == sec.lines[0]->v2) ?
				sec.lines[1]->v2 : sec.lines[1]->v1;

			const auto sv1 = Level.GetSegVertex(vi1);
			const auto sv2 = Level.GetSegVertex(vi2);
			const auto sv3 = Level.GetSegVertex(vi3);

			vt1 = DVector3(sv1.x, sv1.y, 0);
			vt2 = DVector3(sv2.x, sv2.y, 0);
			vt3 = DVector3(sv3.x, sv3.y, 0);

			for (int j = 0; j < 2; j++)
			{
				double* h1 = vt_heights[j].CheckKey(vi1);
				double* h2 = vt_heights[j].CheckKey(vi2);
				double* h3 = vt_heights[j].CheckKey(vi3);

				if (h1 == NULL && h2 == NULL && h3 == NULL) continue;

				vt1.Z = h1 ? *h1 : j == 0 ? sec.data.floorheight : sec.data.ceilingheight;
				vt2.Z = h2 ? *h2 : j == 0 ? sec.data.floorheight : sec.data.ceilingheight;
				vt3.Z = h3 ? *h3 : j == 0 ? sec.data.floorheight : sec.data.ceilingheight;

				if (P_PointOnLineSidePrecise(FVector2(vt3.XY()), Level.GetSegVertex(sec.lines[0]->v1), Level.GetSegVertex(sec.lines[0]->v2)) == 0)
				{
					vec1 = vt2 - vt3;
					vec2 = vt1 - vt3;
				}
				else
				{
					vec1 = vt1 - vt3;
					vec2 = vt2 - vt3;
				}

				DVector3 cross = (vec1 ^ vec2);

				double len = cross.Length();
				if (len == 0)
				{
					// Only happens when all vertices in this sector are on the same line.
					// Let's just ignore this case.
					continue;
				}
				cross /= len;

				// Fix backward normals
				if ((cross.Z < 0 && j == 0) || (cross.Z > 0 && j == 1))
				{
					cross = -cross;
				}

				Plane* plane = j == 0 ? &sec.floorplane : &sec.ceilingplane;

				double dist = -cross[0] * vt3.X - cross[1] * vt3.Y - cross[2] * vt3.Z;
				plane->Set(float(cross[0]), float(cross[1]), float(cross[2]), float(-dist));
			}
		}
	}
}

void FProcessor::SpawnSlopeMakers(IntThing* firstmt, IntThing* lastmt, const int* oldvertextable)
{
	IntThing* mt;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type >= SMT_SlopeFloorPointLine && mt->type <= SMT_VavoomCeiling)
		{
			DVector3 pos = DVector3(F(mt->x), F(mt->y), F(mt->z));
			Plane* refplane;
			IntSector* sec;
			bool ceiling;

			sec = Level.PointInSector(pos.XY());
			if (mt->type == SMT_SlopeCeilingPointLine || mt->type == SMT_VavoomCeiling || mt->type == SMT_SetCeilingSlope)
			{
				refplane = &sec->ceilingplane;
				ceiling = true;
			}
			else
			{
				refplane = &sec->floorplane;
				ceiling = false;
			}

			pos.Z = double(refplane->zAt(float(pos.X), float(pos.Y))) + pos.Z;

			/*if (mt->type <= SMT_SlopeCeilingPointLine)
			{ // SlopeFloorPointLine and SlopCeilingPointLine
				SlopeLineToPoint(mt->args[0], pos, ceiling);
			}
			else if (mt->type <= SMT_SetCeilingSlope)
			{ // SetFloorSlope and SetCeilingSlope
				SetSlope(refplane, ceiling, mt->angle, mt->args[0], pos);
			}
			else
			{ // VavoomFloor and VavoomCeiling (these do not perform any sector height adjustment - z is absolute)
				VavoomSlope(sec, mt->thingid, mt->pos, ceiling);
			}*/
			mt->type = 0;
		}
	}
	SetSlopesFromVertexHeights(firstmt, lastmt, oldvertextable);

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type == SMT_CopyFloorPlane || mt->type == SMT_CopyCeilingPlane)
		{
			CopyPlane(mt->args[0], DVector2(F(mt->x), F(mt->y)), mt->type == SMT_CopyCeilingPlane);
			mt->type = 0;
		}
	}
	
}

void FProcessor::AlignPlane(IntSector* sec, IntLineDef* line, int which)
{
	IntSector* refsec;
	double bestdist;
	FloatVertex refvert = Level.GetSegVertex(sec->lines[0]->v1);

	if (line->backsector == NULL)
		return;

	auto lv1 = Level.GetSegVertex(line->v1);
	auto lv2 = Level.GetSegVertex(line->v2);

	// Find furthest vertex from the reference line. It, along with the two ends
	// of the line, will define the plane.
	bestdist = 0;
	for (auto ln : sec->lines)
	{
		for (int i = 0; i < 2; i++)
		{
			double dist;
			FloatVertex vert;

			vert = Level.GetSegVertex(i == 0 ? ln->v1 : ln->v2);

			dist = fabs((lv1.y - vert.y) * (lv2.x - lv1.x) -
				(lv1.x - vert.x) * (lv2.y - lv1.y));

			if (dist > bestdist)
			{
				bestdist = dist;
				refvert = vert;
			}
		}
	}

	refsec = line->frontsector == sec ? line->backsector : line->frontsector;

	DVector3 p, v1, v2, cross;

	Plane* srcplane;
	double srcheight, destheight;

	srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
	srcheight = (which == 0) ? sec->data.floorheight : sec->data.ceilingheight;
	destheight = (which == 0) ? refsec->data.floorheight : refsec->data.ceilingheight;

	p[0] = lv1.x;
	p[1] = lv1.y;
	p[2] = destheight;
	v1[0] = double(lv2.x) - double(lv1.x);
	v1[1] = double(lv2.y) - double(lv1.y);
	v1[2] = 0;
	v2[0] = double(refvert.x) - double(lv1.x);
	v2[1] = double(refvert.y) - double(lv1.y);
	v2[2] = srcheight - destheight;

	cross = (v1 ^ v2).Unit();

	// Fix backward normals
	if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
	{
		cross = -cross;
	}

	double dist = -cross[0] * lv1.x - cross[1] * lv1.y - cross[2] * destheight;
	srcplane->Set(float(cross[0]), float(cross[1]), float(cross[2]), float(-dist));
}

void FProcessor::SetSlopes()
{
	int s;

	for (auto& line : Level.Lines)
	{
		if (line.special == Plane_Align)
		{
			line.special = 0;
			if (line.backsector != nullptr)
			{
				// args[0] is for floor, args[1] is for ceiling
				//
				// As a special case, if args[1] is 0,
				// then args[0], bits 2-3 are for ceiling.
				for (s = 0; s < 2; s++)
				{
					int bits = line.args[s] & 3;

					if (s == 1 && bits == 0)
						bits = (line.args[0] >> 2) & 3;

					if (bits == 1)			// align front side to back
						AlignPlane(line.frontsector, &line, s);
					else if (bits == 2)		// align back side to front
						AlignPlane(line.backsector, &line, s);
				}
			}
		}
	}
}

void FProcessor::CopySlopes()
{
	for (auto& line : Level.Lines)
	{
		if (line.special == Plane_Copy)
		{
			// The args are used for the tags of sectors to copy:
			// args[0]: front floor
			// args[1]: front ceiling
			// args[2]: back floor
			// args[3]: back ceiling
			// args[4]: copy slopes from one side of the line to the other.
			line.special = 0;
			for (int s = 0; s < (line.backsector ? 4 : 2); s++)
			{
				if (line.args[s])
					CopyPlane(line.args[s], (s & 2 ? line.backsector : line.frontsector), s & 1);
			}

			if (line.backsector != NULL)
			{
				if ((line.args[4] & 3) == 1)
				{
					line.backsector->floorplane = line.frontsector->floorplane;
				}
				else if ((line.args[4] & 3) == 2)
				{
					line.frontsector->floorplane = line.backsector->floorplane;
				}
				if ((line.args[4] & 12) == 4)
				{
					line.backsector->ceilingplane = line.frontsector->ceilingplane;
				}
				else if ((line.args[4] & 12) == 8)
				{
					line.frontsector->ceilingplane = line.backsector->ceilingplane;
				}
			}
		}
	}
}

void FProcessor::CopyPlane(int tag, IntSector* dest, bool copyCeil)
{
	IntSector* source;
	int secnum;

	secnum = Level.FindFirstSectorFromTag(tag);
	if (secnum == -1)
	{
		return;
	}

	source = &Level.Sectors[secnum];

	if (copyCeil)
	{
		dest->ceilingplane = source->ceilingplane;
	}
	else
	{
		dest->floorplane = source->floorplane;
	}
}

void FProcessor::CopyPlane(int tag, const DVector2& pos, bool copyCeil)
{
	CopyPlane(tag, Level.PointInSector(pos), copyCeil);
}