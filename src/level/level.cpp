/*
    Reads wad files, builds nodes, and saves new wad files.
    Copyright (C) 2002-2006 Randy Heit

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "level/level.h"
//#include "rejectbuilder.h"
#include <memory>

extern void ShowView (FLevel *level);

int SSELevel = 2;

enum
{
	// Thing numbers used in Hexen maps
    PO_HEX_ANCHOR_TYPE = 3000,
    PO_HEX_SPAWN_TYPE,
    PO_HEX_SPAWNCRUSH_TYPE,

    // Thing numbers used in Doom and Heretic maps
    PO_ANCHOR_TYPE = 9300,
    PO_SPAWN_TYPE,
    PO_SPAWNCRUSH_TYPE,
	PO_SPAWNHURT_TYPE
};

FLevel::FLevel ()
{
	memset (this, 0, sizeof(*this));
}

FLevel::~FLevel ()
{
	if (Vertices)		delete[] Vertices;
	if (Subsectors)		delete[] Subsectors;
	if (Segs)			delete[] Segs;
	if (Nodes)			delete[] Nodes;
	if (Blockmap)		delete[] Blockmap;
	if (Reject)			delete[] Reject;
	if (GLSubsectors)	delete[] GLSubsectors;
	if (GLSegs)			delete[] GLSegs;
	if (GLNodes)		delete[] GLNodes;
	if (GLPVS)			delete[] GLPVS;
	if (OrgSectorMap)	delete[] OrgSectorMap;
}

FLevelLoader::FLevelLoader (FWadReader &inwad, int lump)
:
  Wad (inwad), Lump (lump)
{
	printf ("----%s----\n", Wad.LumpName (Lump));

	isUDMF = Wad.isUDMF(lump);

	if (isUDMF)
	{
		Extended = false;
		LoadUDMF();
	}
	else
	{
		Extended = Wad.MapHasBehavior (lump);
		LoadThings ();
		LoadVertices ();
		LoadLines ();
		LoadSides ();
		LoadSectors ();
	}

	if (Level.NumLines() == 0 || Level.NumVertices == 0 || Level.NumSides() == 0 || Level.NumSectors() == 0)
	{
		printf ("   Map is incomplete\n");
	}
	else
	{
		// Removing extra vertices is done by the node builder.
		Level.RemoveExtraLines ();
		if (!NoPrune)
		{
			Level.RemoveExtraSides ();
			Level.RemoveExtraSectors ();
		}

		GetPolySpots ();

		Level.FindMapBounds ();
	}
}

void FLevelLoader::LoadThings ()
{
	int NumThings;

	if (Extended)
	{
		MapThing2 *Things;
		ReadMapLump<MapThing2> (Wad, "THINGS", Lump, Things, NumThings);

		Level.Things.Resize(NumThings);
		for (int i = 0; i < NumThings; ++i)
		{
			Level.Things[i].thingid = Things[i].thingid;
			Level.Things[i].x = LittleShort(Things[i].x) << FRACBITS;
			Level.Things[i].y = LittleShort(Things[i].y) << FRACBITS;
			Level.Things[i].z = LittleShort(Things[i].z);
			Level.Things[i].angle = LittleShort(Things[i].angle);
			Level.Things[i].type = LittleShort(Things[i].type);
			Level.Things[i].flags = LittleShort(Things[i].flags);
			Level.Things[i].special = Things[i].special;
			Level.Things[i].args[0] = Things[i].args[0];
			Level.Things[i].args[1] = Things[i].args[1];
			Level.Things[i].args[2] = Things[i].args[2];
			Level.Things[i].args[3] = Things[i].args[3];
			Level.Things[i].args[4] = Things[i].args[4];
		}
		delete[] Things;
	}
	else
	{
		MapThing *mt;
		ReadMapLump<MapThing> (Wad, "THINGS", Lump, mt, NumThings);

		Level.Things.Resize(NumThings);
		for (int i = 0; i < NumThings; ++i)
		{
			Level.Things[i].x = LittleShort(mt[i].x) << FRACBITS;
			Level.Things[i].y = LittleShort(mt[i].y) << FRACBITS;
			Level.Things[i].angle = LittleShort(mt[i].angle);
			Level.Things[i].type = LittleShort(mt[i].type);
			Level.Things[i].flags = LittleShort(mt[i].flags);
			Level.Things[i].z = 0;
			Level.Things[i].special = 0;
			Level.Things[i].args[0] = 0;
			Level.Things[i].args[1] = 0;
			Level.Things[i].args[2] = 0;
			Level.Things[i].args[3] = 0;
			Level.Things[i].args[4] = 0;
		}
		delete[] mt;
	}
}

void FLevelLoader::LoadLines ()
{
	int NumLines;

	if (Extended)
	{
		MapLineDef2 *Lines;

		ReadMapLump<MapLineDef2> (Wad, "LINEDEFS", Lump, Lines, NumLines);

		Level.Lines.Resize(NumLines);
		for (int i = 0; i < NumLines; ++i)
		{
			Level.Lines[i].special = Lines[i].special;
			Level.Lines[i].args[0] = Lines[i].args[0];
			Level.Lines[i].args[1] = Lines[i].args[1];
			Level.Lines[i].args[2] = Lines[i].args[2];
			Level.Lines[i].args[3] = Lines[i].args[3];
			Level.Lines[i].args[4] = Lines[i].args[4];
			Level.Lines[i].v1 = LittleShort(Lines[i].v1);
			Level.Lines[i].v2 = LittleShort(Lines[i].v2);
			Level.Lines[i].flags = LittleShort(Lines[i].flags);
			Level.Lines[i].sidenum[0] = LittleShort(Lines[i].sidenum[0]);
			Level.Lines[i].sidenum[1] = LittleShort(Lines[i].sidenum[1]);
			if (Level.Lines[i].sidenum[0] == NO_MAP_INDEX) Level.Lines[i].sidenum[0] = NO_INDEX;
			if (Level.Lines[i].sidenum[1] == NO_MAP_INDEX) Level.Lines[i].sidenum[1] = NO_INDEX;
		}
		delete[] Lines;
	}
	else
	{
		MapLineDef *ml;
		ReadMapLump<MapLineDef> (Wad, "LINEDEFS", Lump, ml, NumLines);

		Level.Lines.Resize(NumLines);
		for (int i = 0; i < NumLines; ++i)
		{
			Level.Lines[i].v1 = LittleShort(ml[i].v1);
			Level.Lines[i].v2 = LittleShort(ml[i].v2);
			Level.Lines[i].flags = LittleShort(ml[i].flags);
			Level.Lines[i].sidenum[0] = LittleShort(ml[i].sidenum[0]);
			Level.Lines[i].sidenum[1] = LittleShort(ml[i].sidenum[1]);
			if (Level.Lines[i].sidenum[0] == NO_MAP_INDEX) Level.Lines[i].sidenum[0] = NO_INDEX;
			if (Level.Lines[i].sidenum[1] == NO_MAP_INDEX) Level.Lines[i].sidenum[1] = NO_INDEX;

			// Store the special and tag in the args array so we don't lose them
			Level.Lines[i].special = 0;
			Level.Lines[i].args[0] = LittleShort(ml[i].special);
			Level.Lines[i].args[1] = LittleShort(ml[i].tag);
		}
		delete[] ml;
	}
}

void FLevelLoader::LoadVertices ()
{
	MapVertex *verts;
	ReadMapLump<MapVertex> (Wad, "VERTEXES", Lump, verts, Level.NumVertices);

	Level.Vertices = new WideVertex[Level.NumVertices];

	for (int i = 0; i < Level.NumVertices; ++i)
	{
		Level.Vertices[i].x = LittleShort(verts[i].x) << FRACBITS;
		Level.Vertices[i].y = LittleShort(verts[i].y) << FRACBITS;
		Level.Vertices[i].index = 0; // we don't need this value for non-UDMF maps
	}
}

void FLevelLoader::LoadSides ()
{
	MapSideDef *Sides;
	int NumSides;
	ReadMapLump<MapSideDef> (Wad, "SIDEDEFS", Lump, Sides, NumSides);

	Level.Sides.Resize(NumSides);
	for (int i = 0; i < NumSides; ++i)
	{
		Level.Sides[i].textureoffset = Sides[i].textureoffset;
		Level.Sides[i].rowoffset = Sides[i].rowoffset;
		memcpy(Level.Sides[i].toptexture, Sides[i].toptexture, 8);
		memcpy(Level.Sides[i].bottomtexture, Sides[i].bottomtexture, 8);
		memcpy(Level.Sides[i].midtexture, Sides[i].midtexture, 8);

		Level.Sides[i].sector = LittleShort(Sides[i].sector);
		if (Level.Sides[i].sector == NO_MAP_INDEX) Level.Sides[i].sector = NO_INDEX;
	}
	delete [] Sides;
}

void FLevelLoader::LoadSectors ()
{
	MapSector *Sectors;
	int NumSectors;

	ReadMapLump<MapSector> (Wad, "SECTORS", Lump, Sectors, NumSectors);
	Level.Sectors.Resize(NumSectors);

	for (int i = 0; i < NumSectors; ++i)
	{
		Level.Sectors[i].data = Sectors[i];
	}
}

void FLevel::FindMapBounds ()
{
	fixed_t minx, maxx, miny, maxy;

	minx = maxx = Vertices[0].x;
	miny = maxy = Vertices[0].y;

	for (int i = 1; i < NumVertices; ++i)
	{
			 if (Vertices[i].x < minx) minx = Vertices[i].x;
		else if (Vertices[i].x > maxx) maxx = Vertices[i].x;
			 if (Vertices[i].y < miny) miny = Vertices[i].y;
		else if (Vertices[i].y > maxy) maxy = Vertices[i].y;
	}

	MinX = minx;
	MinY = miny;
	MaxX = maxx;
	MaxY = maxy;
}

void FLevel::RemoveExtraLines ()
{
	int i, newNumLines;

	// Extra lines are those with 0 length. Collision detection against
	// one of those could cause a divide by 0, so it's best to remove them.

	for (i = newNumLines = 0; i < NumLines(); ++i)
	{
		if (Vertices[Lines[i].v1].x != Vertices[Lines[i].v2].x ||
			Vertices[Lines[i].v1].y != Vertices[Lines[i].v2].y)
		{
			if (i != newNumLines)
			{
				Lines[newNumLines] = Lines[i];
			}
			++newNumLines;
		}
	}
	if (newNumLines < NumLines())
	{
		int diff = NumLines() - newNumLines;

		printf ("   Removed %d line%s with 0 length.\n", diff, diff > 1 ? "s" : "");
	}
	Lines.Resize(newNumLines);
}

void FLevel::RemoveExtraSides ()
{
	BYTE *used;
	int *remap;
	int i, newNumSides;

	// Extra sides are those that aren't referenced by any lines.
	// They just waste space, so get rid of them.
	int NumSides = this->NumSides();

	used = new BYTE[NumSides];
	memset (used, 0, NumSides*sizeof(*used));
	remap = new int[NumSides];

	// Mark all used sides
	for (i = 0; i < NumLines(); ++i)
	{
		if (Lines[i].sidenum[0] != NO_INDEX)
		{
			used[Lines[i].sidenum[0]] = 1;
		}
		else
		{
			printf ("   Line %d needs a front sidedef before it will run with ZDoom.\n", i);
		}
		if (Lines[i].sidenum[1] != NO_INDEX)
		{
			used[Lines[i].sidenum[1]] = 1;
		}
	}

	// Shift out any unused sides
	for (i = newNumSides = 0; i < NumSides; ++i)
	{
		if (used[i])
		{
			if (i != newNumSides)
			{
				Sides[newNumSides] = Sides[i];
			}
			remap[i] = newNumSides++;
		}
		else
		{
			remap[i] = NO_INDEX;
		}
	}

	if (newNumSides < NumSides)
	{
		int diff = NumSides - newNumSides;

		printf ("   Removed %d unused sidedef%s.\n", diff, diff > 1 ? "s" : "");
		Sides.Resize(newNumSides);

		// Renumber side references in lines
		for (i = 0; i < NumLines(); ++i)
		{
			if (Lines[i].sidenum[0] != NO_INDEX)
			{
				Lines[i].sidenum[0] = remap[Lines[i].sidenum[0]];
			}
			if (Lines[i].sidenum[1] != NO_INDEX)
			{
				Lines[i].sidenum[1] = remap[Lines[i].sidenum[1]];
			}
		}
	}
	delete[] used;
	delete[] remap;
}

void FLevel::RemoveExtraSectors ()
{
	BYTE *used;
	DWORD *remap;
	int i, newNumSectors;

	// Extra sectors are those that aren't referenced by any sides.
	// They just waste space, so get rid of them.

	NumOrgSectors = NumSectors();
	used = new BYTE[NumSectors()];
	memset (used, 0, NumSectors()*sizeof(*used));
	remap = new DWORD[NumSectors()];

	// Mark all used sectors
	for (i = 0; i < NumSides(); ++i)
	{
		if ((DWORD)Sides[i].sector != NO_INDEX)
		{
			used[Sides[i].sector] = 1;
		}
		else
		{
			printf ("   Sidedef %d needs a front sector before it will run with ZDoom.\n", i);
		}
	}

	// Shift out any unused sides
	for (i = newNumSectors = 0; i < NumSectors(); ++i)
	{
		if (used[i])
		{
			if (i != newNumSectors)
			{
				Sectors[newNumSectors] = Sectors[i];
			}
			remap[i] = newNumSectors++;
		}
		else
		{
			remap[i] = NO_INDEX;
		}
	}

	if (newNumSectors < NumSectors())
	{
		int diff = NumSectors() - newNumSectors;
		printf ("   Removed %d unused sector%s.\n", diff, diff > 1 ? "s" : "");

		// Renumber sector references in sides
		for (i = 0; i < NumSides(); ++i)
		{
			if ((DWORD)Sides[i].sector != NO_INDEX)
			{
				Sides[i].sector = remap[Sides[i].sector];
			}
		}
		// Make a reverse map for fixing reject lumps
		OrgSectorMap = new DWORD[newNumSectors];
		for (i = 0; i < NumSectors(); ++i)
		{
			if (remap[i] != NO_INDEX)
			{
				OrgSectorMap[remap[i]] = i;
			}
		}

		Sectors.Resize(newNumSectors);
	}

	delete[] used;
	delete[] remap;
}

void FLevelLoader::GetPolySpots ()
{
	if (Extended && CheckPolyobjs)
	{
		int spot1, spot2, anchor, i;

		// Determine if this is a Hexen map by looking for things of type 3000
		// Only Hexen maps use them, and they are the polyobject anchors
		for (i = 0; i < Level.NumThings(); ++i)
		{
			if (Level.Things[i].type == PO_HEX_ANCHOR_TYPE)
			{
				break;
			}
		}

		if (i < Level.NumThings())
		{
			spot1 = PO_HEX_SPAWN_TYPE;
			spot2 = PO_HEX_SPAWNCRUSH_TYPE;
			anchor = PO_HEX_ANCHOR_TYPE;
		}
		else
		{
			spot1 = PO_SPAWN_TYPE;
			spot2 = PO_SPAWNCRUSH_TYPE;
			anchor = PO_ANCHOR_TYPE;
		}

		for (i = 0; i < Level.NumThings(); ++i)
		{
			if (Level.Things[i].type == spot1 ||
				Level.Things[i].type == spot2 ||
				Level.Things[i].type == PO_SPAWNHURT_TYPE ||
				Level.Things[i].type == anchor)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = Level.Things[i].x;
				newvert.y = Level.Things[i].y;
				newvert.polynum = Level.Things[i].angle;
				if (Level.Things[i].type == anchor)
				{
					PolyAnchors.Push (newvert);
				}
				else
				{
					PolyStarts.Push (newvert);
				}
			}
		}
	}
}

void FLevelLoader::BuildNodes()
{
	if (Level.NumLines() == 0 || Level.NumSides() == 0 || Level.NumSectors() == 0 || Level.NumVertices == 0)
		return;

	std::unique_ptr<FNodeBuilder> builder;

	builder.reset(new FNodeBuilder (Level, PolyStarts, PolyAnchors, Wad.LumpName (Lump), true/*BuildGLNodes*/));

	delete[] Level.Vertices;
	builder->GetVertices (Level.Vertices, Level.NumVertices);

	//if (ConformNodes)
	{
		// When the nodes are "conformed", the normal and GL nodes use the same
		// basic information. This creates normal nodes that are less "good" than
		// possible, but it makes it easier to compare the two sets of nodes to
		// determine the correctness of the GL nodes.
		builder->GetNodes (Level.Nodes, Level.NumNodes,
			Level.Segs, Level.NumSegs,
			Level.Subsectors, Level.NumSubsectors);
		builder->GetVertices (Level.GLVertices, Level.NumGLVertices);
		builder->GetGLNodes (Level.GLNodes, Level.NumGLNodes,
			Level.GLSegs, Level.NumGLSegs,
			Level.GLSubsectors, Level.NumGLSubsectors);
	}
	/*else
	{
		if (BuildGLNodes)
		{
			builder->GetVertices (Level.GLVertices, Level.NumGLVertices);
			builder->GetGLNodes (Level.GLNodes, Level.NumGLNodes,
				Level.GLSegs, Level.NumGLSegs,
				Level.GLSubsectors, Level.NumGLSubsectors);

			if (!GLOnly)
			{
				// Now repeat the process to obtain regular nodes
				builder.reset();
				builder.reset(new FNodeBuilder (Level, PolyStarts, PolyAnchors, Wad.LumpName (Lump), false));
				if (builder == nullptr)
				{
					throw std::runtime_error("   Not enough memory to build regular nodes!");
				}
				delete[] Level.Vertices;
				builder->GetVertices (Level.Vertices, Level.NumVertices);
			}
		}
		if (!GLOnly)
		{
			builder->GetNodes (Level.Nodes, Level.NumNodes,
				Level.Segs, Level.NumSegs,
				Level.Subsectors, Level.NumSubsectors);
		}
	}*/
	builder.reset();

	if (!isUDMF)
	{
		FBlockmapBuilder bbuilder (Level);
		WORD *blocks = bbuilder.GetBlockmap (Level.BlockmapSize);
		Level.Blockmap = new WORD[Level.BlockmapSize];
		memcpy (Level.Blockmap, blocks, Level.BlockmapSize*sizeof(WORD));

		Level.RejectSize = (Level.NumSectors()*Level.NumSectors() + 7) / 8;
		Level.Reject = nullptr;

		switch (RejectMode)
		{
		case ERM_Rebuild:
			//FRejectBuilder reject (Level);
			//Level.Reject = reject.GetReject ();
			printf ("   Rebuilding the reject is unsupported.\n");
			// Intentional fall-through

		case ERM_DontTouch:
			{
				int lump = Wad.FindMapLump ("REJECT", Lump);

				if (lump >= 0)
				{
					ReadLump<BYTE> (Wad, lump, Level.Reject, Level.RejectSize);
					if (Level.RejectSize != (Level.NumOrgSectors*Level.NumOrgSectors + 7) / 8)
					{
						// If the reject is the wrong size, don't use it.
						delete[] Level.Reject;
						Level.Reject = nullptr;
						if (Level.RejectSize != 0)
						{ // Do not warn about 0-length rejects
							printf ("   REJECT is the wrong size, so it will be removed.\n");
						}
						Level.RejectSize = 0;
					}
					else if (Level.NumOrgSectors != Level.NumSectors())
					{
						// Some sectors have been removed, so fix the reject.
						BYTE *newreject = FixReject (Level.Reject);
						delete[] Level.Reject;
						Level.Reject = newreject;
						Level.RejectSize = (Level.NumSectors() * Level.NumSectors() + 7) / 8;
					}
				}
			}
			break;

		case ERM_Create0:
			break;

		case ERM_CreateZeroes:
			Level.Reject = new BYTE[Level.RejectSize];
			memset (Level.Reject, 0, Level.RejectSize);
			break;
		}
	}

	if (ShowMap)
	{
#ifndef NO_MAP_VIEWER
		ShowView (&Level);
#else
		puts ("  This version of ZDRay was not compiled with the viewer enabled.");
#endif
	}
}

//
BYTE *FLevelLoader::FixReject (const BYTE *oldreject)
{
	int x, y, ox, oy, pnum, opnum;
	int rejectSize = (Level.NumSectors()*Level.NumSectors() + 7) / 8;
	BYTE *newreject = new BYTE[rejectSize];

	memset (newreject, 0, rejectSize);

	for (y = 0; y < Level.NumSectors(); ++y)
	{
		oy = Level.OrgSectorMap[y];
		for (x = 0; x < Level.NumSectors(); ++x)
		{
			ox = Level.OrgSectorMap[x];
			pnum = y*Level.NumSectors() + x;
			opnum = oy*Level.NumSectors() + ox;

			if (oldreject[opnum >> 3] & (1 << (opnum & 7)))
			{
				newreject[pnum >> 3] |= 1 << (pnum & 7);
			}
		}
	}
	return newreject;
}

MapNodeEx *FLevelLoader::NodesToEx (const MapNode *nodes, int count)
{
	if (count == 0)
	{
		return nullptr;
	}

	MapNodeEx *Nodes = new MapNodeEx[Level.NumNodes];
	int x;

	for (x = 0; x < count; ++x)
	{
		WORD child;
		int i;

		for (i = 0; i < 4+2*4; ++i)
		{
			*((WORD *)&Nodes[x] + i) = LittleShort(*((WORD *)&nodes[x] + i));
		}
		for (i = 0; i < 2; ++i)
		{
			child = LittleShort(nodes[x].children[i]);
			if (child & NF_SUBSECTOR)
			{
				Nodes[x].children[i] = child + (NFX_SUBSECTOR - NF_SUBSECTOR);
			}
			else
			{
				Nodes[x].children[i] = child;
			}
		}
	}
	return Nodes;
}

MapSubsectorEx *FLevelLoader::SubsectorsToEx (const MapSubsector *ssec, int count)
{
	if (count == 0)
	{
		return nullptr;
	}

	MapSubsectorEx *out = new MapSubsectorEx[Level.NumSubsectors];
	int x;

	for (x = 0; x < count; ++x)
	{
		out[x].numlines = LittleShort(ssec[x].numlines);
		out[x].firstline = LittleShort(ssec[x].firstline);
	}
	
	return out;
}

MapSegGLEx *FLevelLoader::SegGLsToEx (const MapSegGL *segs, int count)
{
	if (count == 0)
	{
		return nullptr;
	}

	MapSegGLEx *out = new MapSegGLEx[count];
	int x;

	for (x = 0; x < count; ++x)
	{
		out[x].v1 = LittleShort(segs[x].v1);
		out[x].v2 = LittleShort(segs[x].v2);
		out[x].linedef = LittleShort(segs[x].linedef);
		out[x].side = LittleShort(segs[x].side);
		out[x].partner = LittleShort(segs[x].partner);
	}

	return out;
}

bool FLevelLoader::CheckForFracSplitters(const MapNodeEx *nodes, int numnodes)
{
	for (int i = 0; i < numnodes; ++i)
	{
		if (0 != ((nodes[i].x | nodes[i].y | nodes[i].dx | nodes[i].dy) & 0x0000FFFF))
		{
			return true;
		}
	}
	return false;
}

// zlib lump writer ---------------------------------------------------------

ZLibOut::ZLibOut (FWadWriter &out)
	: Out (out)
{
	int err;

	Stream.next_in = Z_NULL;
	Stream.avail_in = 0;
	Stream.zalloc = Z_NULL;
	Stream.zfree = Z_NULL;
	err = deflateInit (&Stream, 9);

	if (err != Z_OK)
	{
		throw std::runtime_error("Could not initialize deflate buffer.");
	}

	Stream.next_out = Buffer;
	Stream.avail_out = BUFFER_SIZE;
}

ZLibOut::~ZLibOut ()
{
	int err;

	for (;;)
	{
		err = deflate (&Stream, Z_FINISH);
		if (err != Z_OK)
		{
			break;
		}
		if (Stream.avail_out == 0)
		{
			Out.AddToLump (Buffer, BUFFER_SIZE);
			Stream.next_out = Buffer;
			Stream.avail_out = BUFFER_SIZE;
		}
	}
	deflateEnd (&Stream);
	/*if (err != Z_STREAM_END)
	{
		throw std::runtime_error("Error deflating data.");
	}*/
	Out.AddToLump (Buffer, BUFFER_SIZE - Stream.avail_out);
}

void ZLibOut::Write (BYTE *data, int len)
{
	int err;

	Stream.next_in = data;
	Stream.avail_in = len;
	err = deflate (&Stream, 0);
	while (Stream.avail_out == 0 && err == Z_OK)
	{
		Out.AddToLump (Buffer, BUFFER_SIZE);
		Stream.next_out = Buffer;
		Stream.avail_out = BUFFER_SIZE;
		if (Stream.avail_in != 0)
		{
			err = deflate (&Stream, 0);
		}
	}
	if (err != Z_OK)
	{
		throw std::runtime_error("Error deflating data.");
	}
}

ZLibOut &ZLibOut::operator << (BYTE val)
{
	Write (&val, 1);
	return *this;
}

ZLibOut &ZLibOut::operator << (WORD val)
{
	val = LittleShort(val);
	Write ((BYTE *)&val, 2);
	return *this;
}

ZLibOut &ZLibOut::operator << (SWORD val)
{
	val = LittleShort(val);
	Write ((BYTE *)&val, 2);
	return *this;
}

ZLibOut &ZLibOut::operator << (DWORD val)
{
	val = LittleLong(val);
	Write ((BYTE *)&val, 4);
	return *this;
}

ZLibOut &ZLibOut::operator << (fixed_t val)
{
	val = LittleLong(val);
	Write ((BYTE *)&val, 4);
	return *this;
}
