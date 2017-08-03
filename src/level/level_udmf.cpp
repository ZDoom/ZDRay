/*
    Reads and writes UDMF maps
    Copyright (C) 2009 Christoph Oelckers

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


#include <float.h>
#include "level/level.h"
#include "parse/sc_man.h"

typedef double real64;
typedef unsigned int uint32;
typedef signed int int32;
#include "framework/xs_Float.h"


class StringBuffer
{
	const static size_t BLOCK_SIZE = 100000;
	const static size_t BLOCK_ALIGN = sizeof(size_t);

	TDeletingArray<char *> blocks;
	size_t currentindex;

	char *Alloc(size_t size)
	{
		if (currentindex + size >= BLOCK_SIZE)
		{
			// Block is full - get a new one!
			char *newblock = new char[BLOCK_SIZE];
			blocks.Push(newblock);
			currentindex = 0;
		}
		size = (size + BLOCK_ALIGN-1) &~ (BLOCK_ALIGN-1);
		char *p = blocks[blocks.Size()-1] + currentindex;
		currentindex += size;
		return p;
	}
public:

	StringBuffer()
	{
		currentindex = BLOCK_SIZE;
	}

	char * Copy(const char * p)
	{
		return p != NULL? strcpy(Alloc(strlen(p)+1) , p) : NULL;
	}
};

StringBuffer stbuf;


//===========================================================================
//
// Parses a 'key = value;' line of the map
//
//===========================================================================

const char *FLevelLoader::ParseKey(const char *&value)
{
	SC_MustGetString();
	const char *key = stbuf.Copy(sc_String);
	SC_MustGetStringName("=");

	sc_Number = INT_MIN;
	sc_Float = DBL_MIN;
	if (!SC_CheckFloat())
	{
		SC_MustGetString();
	}
	value = stbuf.Copy(sc_String);
	SC_MustGetStringName(";");
	return key;
}

bool FLevelLoader::CheckKey(const char *&key, const char *&value)
{
	SC_SavePos();
	SC_MustGetString();
	if (SC_CheckString("="))
	{
		SC_RestorePos();
		key = ParseKey(value);
		return true;
	}
	SC_RestorePos();
	return false;
}

int CheckInt(const char *key)
{
	if (sc_Number == INT_MIN)
	{
		SC_ScriptError("Integer value expected for key '%s'", key);
	}
	return sc_Number;
}

double CheckFloat(const char *key)
{
	if (sc_Float == DBL_MIN)
	{
		SC_ScriptError("Floating point value expected for key '%s'", key);
	}
	return sc_Float;
}

fixed_t CheckFixed(const char *key)
{
	double val = CheckFloat(key);
	if (val < -32768 || val > 32767)
	{
		SC_ScriptError("Fixed point value is out of range for key '%s'\n\t%.2f should be within [-32768,32767]", key, val / 65536);
	}
	return xs_Fix<16>::ToFix(val);
}

//===========================================================================
//
// Parse a thing block
//
//===========================================================================

void FLevelLoader::ParseThing(IntThing *th)
{
	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		// The only properties we need from a thing are
		// x, y, angle and type.

		if (!stricmp(key, "x"))
		{
			th->x = CheckFixed(key);
		}
		else if (!stricmp(key, "y"))
		{
			th->y = CheckFixed(key);
		}
		if (!stricmp(key, "angle"))
		{
			th->angle = (short)CheckInt(key);
		}
		if (!stricmp(key, "type"))
		{
			th->type = (short)CheckInt(key);
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		th->props.Push(k);
	}
}

//===========================================================================
//
// Parse a linedef block
//
//===========================================================================

void FLevelLoader::ParseLinedef(IntLineDef *ld)
{
	SC_MustGetStringName("{");
	ld->v1 = ld->v2 = ld->sidenum[0] = ld->sidenum[1] = NO_INDEX;
	ld->special = 0;
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		if (!stricmp(key, "v1"))
		{
			ld->v1 = CheckInt(key);
			continue;	// do not store in props
		}
		else if (!stricmp(key, "v2"))
		{
			ld->v2 = CheckInt(key);
			continue;	// do not store in props
		}
		else if (Extended && !stricmp(key, "special"))
		{
			ld->special = CheckInt(key);
		}
		else if (Extended && !stricmp(key, "arg0"))
		{
			ld->args[0] = CheckInt(key);
		}
		if (!stricmp(key, "sidefront"))
		{
			ld->sidenum[0] = CheckInt(key);
			continue;	// do not store in props
		}
		else if (!stricmp(key, "sideback"))
		{
			ld->sidenum[1] = CheckInt(key);
			continue;	// do not store in props
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		ld->props.Push(k);
	}
}

//===========================================================================
//
// Parse a sidedef block
//
//===========================================================================

void FLevelLoader::ParseSidedef(IntSideDef *sd)
{
	SC_MustGetStringName("{");
	sd->sector = NO_INDEX;
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		if (!stricmp(key, "sector"))
		{
			sd->sector = CheckInt(key);
			continue;	// do not store in props
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		sd->props.Push(k);
	}
}

//===========================================================================
//
// Parse a sidedef block
//
//===========================================================================

void FLevelLoader::ParseSector(IntSector *sec)
{
	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		// No specific sector properties are ever used by the node builder
		// so everything can go directly to the props array.

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		sec->props.Push(k);
	}
}

//===========================================================================
//
// parse a vertex block
//
//===========================================================================

void FLevelLoader::ParseVertex(WideVertex *vt, IntVertex *vtp)
{
	vt->x = vt->y = 0;
	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		if (!stricmp(key, "x"))
		{
			vt->x = CheckFixed(key);
		}
		else if (!stricmp(key, "y"))
		{
			vt->y = CheckFixed(key);
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		vtp->props.Push(k);
	}
}


//===========================================================================
//
// parses global map properties
//
//===========================================================================

void FLevelLoader::ParseMapProperties()
{
	const char *key, *value;

	// all global keys must come before the first map element.

	while (CheckKey(key, value))
	{
		if (!stricmp(key, "namespace"))
		{
			// all unknown namespaces are assumed to be standard.
			Extended = !stricmp(value, "\"ZDoom\"") || !stricmp(value, "\"Hexen\"") || !stricmp(value, "\"Vavoom\"");
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		Level.props.Push(k);
	}
}


//===========================================================================
//
// Main parsing function
//
//===========================================================================

void FLevelLoader::ParseTextMap(int lump)
{
	char *buffer;
	int buffersize;
	TArray<WideVertex> Vertices;

	ReadLump<char> (Wad, lump, buffer, buffersize);
	SC_OpenMem("TEXTMAP", buffer, buffersize);

	SC_SetCMode(true);
	ParseMapProperties();

	while (SC_GetString())
	{
		if (SC_Compare("thing"))
		{
			IntThing *th = &Level.Things[Level.Things.Reserve(1)];
			ParseThing(th);
		}
		else if (SC_Compare("linedef"))
		{
			IntLineDef *ld = &Level.Lines[Level.Lines.Reserve(1)];
			ParseLinedef(ld);
		}
		else if (SC_Compare("sidedef"))
		{
			IntSideDef *sd = &Level.Sides[Level.Sides.Reserve(1)];
			ParseSidedef(sd);
		}
		else if (SC_Compare("sector"))
		{
			IntSector *sec = &Level.Sectors[Level.Sectors.Reserve(1)];
			ParseSector(sec);
		}
		else if (SC_Compare("vertex"))
		{
			WideVertex *vt = &Vertices[Vertices.Reserve(1)];
			IntVertex *vtp = &Level.VertexProps[Level.VertexProps.Reserve(1)];
			vt->index = Vertices.Size();
			ParseVertex(vt, vtp);
		}
	}
	Level.Vertices = new WideVertex[Vertices.Size()];
	Level.NumVertices = Vertices.Size();
	memcpy(Level.Vertices, &Vertices[0], Vertices.Size() * sizeof(WideVertex));
	SC_Close();
	delete[] buffer;
}


//===========================================================================
//
// parse an UDMF map
//
//===========================================================================

void FLevelLoader::LoadUDMF()
{
	ParseTextMap(Lump+1);
}
