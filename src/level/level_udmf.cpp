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

#include "framework/xs_Float.h"

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

static void CopyUDMFString(char *dest, int destlen, const char *udmfvalue)
{
	destlen--;

	char endchar = 0;
	if (udmfvalue[0] == '"' || udmfvalue[0] == '\'')
	{
		endchar = udmfvalue[0];
		udmfvalue++;
	}

	for (int i = 0; i < destlen && udmfvalue[i] != 0 && udmfvalue[i] != endchar; i++)
	{
		*(dest++) = udmfvalue[i];
	}

	*dest = 0;
}

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
		return p != nullptr? strcpy(Alloc(strlen(p)+1) , p) : nullptr;
	}
};

StringBuffer stbuf;


//===========================================================================
//
// Parses a 'key = value;' line of the map
//
//===========================================================================

const char *FProcessor::ParseKey(const char *&value)
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

bool FProcessor::CheckKey(const char *&key, const char *&value)
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

void FProcessor::ParseThing(IntThing *th)
{
	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

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
		if (!stricmp(key, "pitch"))
		{
			th->pitch = (short)CheckInt(key);
		}
		if (!stricmp(key, "type"))
		{
			th->type = (short)CheckInt(key);
		}
		if (!stricmp(key, "height"))
		{
			th->height = CheckInt(key);
		}
		if (!stricmp(key, "special"))
		{
			th->special = CheckInt(key);
		}
		if (!stricmp(key, "arg0"))
		{
			th->args[0] = CheckInt(key);
		}
		if (!stricmp(key, "arg1"))
		{
			th->args[1] = CheckInt(key);
		}
		if (!stricmp(key, "arg2"))
		{
			th->args[2] = CheckInt(key);
		}
		if (!stricmp(key, "arg3"))
		{
			th->args[3] = CheckInt(key);
		}
		if (!stricmp(key, "arg4"))
		{
			th->args[4] = CheckInt(key);
		}
		if (!stricmp(key, "alpha"))
		{
			th->alpha = CheckFloat(key);
		}
		if (!stricmp(key, "arg0str"))
		{
			th->arg0str = value;
			th->arg0str.StripChars("\"");
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

void FProcessor::ParseLinedef(IntLineDef *ld)
{
	ld->sampling.SetGeneralSampleDistance(0);
	ld->sampling.SetSampleDistance(WallPart::TOP, 0);
	ld->sampling.SetSampleDistance(WallPart::MIDDLE, 0);
	ld->sampling.SetSampleDistance(WallPart::BOTTOM, 0);

	std::vector<int> moreids;
	SC_MustGetStringName("{");
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
		else if (Extended && !stricmp(key, "arg1"))
		{
			ld->args[1] = CheckInt(key);
		}
		else if (Extended && !stricmp(key, "arg2"))
		{
			ld->args[2] = CheckInt(key);
		}
		else if (Extended && !stricmp(key, "arg3"))
		{
			ld->args[3] = CheckInt(key);
		}
		else if (Extended && !stricmp(key, "arg4"))
		{
			ld->args[4] = CheckInt(key);
		}
		else if (stricmp(key, "moreids") == 0)
		{
			// delay parsing of the tag string until parsing of the sector is complete
			// This ensures that the ID is always the first tag in the list.
			auto tagstring = value;
			if (tagstring != nullptr && *tagstring == '"')
			{
				// skip the quotation mark
				auto workstring = strdup(tagstring + 1);
				for (char* token = strtok(workstring, " \""); token; token = strtok(nullptr, " \""))
				{
					auto tag = strtoll(token, nullptr, 0);
					if (tag != -1 && (int)tag == tag)
					{
						moreids.push_back(tag);
					}
				}
				free(workstring);
			}
		}
		else if (!stricmp(key, "blocking") && !stricmp(value, "true"))
		{
			ld->flags |= ML_BLOCKING;
		}
		else if (!stricmp(key, "blockmonsters") && !stricmp(value, "true"))
		{
			ld->flags |= ML_BLOCKMONSTERS;
		}
		else if (!stricmp(key, "twosided") && !stricmp(value, "true"))
		{
			ld->flags |= ML_TWOSIDED;
		}
		else if (Extended && !stricmp(key, "id"))
		{
			int id = CheckInt(key);
			ld->ids.Clear();
			if (id != -1) ld->ids.Push(id);
		}
		else if (stricmp(key, "lm_sampledist") == 0)
		{
			ld->sampling.SetGeneralSampleDistance(CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_top") == 0)
		{
			ld->sampling.SetSampleDistance(WallPart::TOP, CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_mid") == 0)
		{
			ld->sampling.SetSampleDistance(WallPart::MIDDLE, CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_bot") == 0)
		{
			ld->sampling.SetSampleDistance(WallPart::BOTTOM, CheckInt(key));
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

	for (int tag : moreids)
		ld->ids.Push(tag);	// don't bother with duplicates, they don't pose a problem.
}

//===========================================================================
//
// Parse a sidedef block
//
//===========================================================================

void FProcessor::ParseSidedef(IntSideDef *sd)
{
	SC_MustGetStringName("{");
	sd->sector = NO_INDEX;
	sd->textureoffset = 0;
	sd->rowoffset = 0;
	sd->toptexture[0] = '-';
	sd->toptexture[1] = 0;
	sd->midtexture[0] = '-';
	sd->midtexture[1] = 0;
	sd->bottomtexture[0] = '-';
	sd->bottomtexture[1] = 0;
	sd->sampling.SetGeneralSampleDistance(0);
	sd->sampling.SetSampleDistance(WallPart::TOP, 0);
	sd->sampling.SetSampleDistance(WallPart::MIDDLE, 0);
	sd->sampling.SetSampleDistance(WallPart::BOTTOM, 0);
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		if (!stricmp(key, "sector"))
		{
			sd->sector = CheckInt(key);
			continue;	// do not store in props
		}

		if (stricmp(key, "texturetop") == 0)
		{
			CopyUDMFString(sd->toptexture, 64, value);
		}
		else if (stricmp(key, "texturemiddle") == 0)
		{
			CopyUDMFString(sd->midtexture, 64, value);
		}
		else if (stricmp(key, "texturebottom") == 0)
		{
			CopyUDMFString(sd->bottomtexture, 64, value);
		}
		else if (stricmp(key, "offsetx_mid") == 0)
		{
			sd->textureoffset = CheckInt(key);
		}
		else if (stricmp(key, "offsety_mid") == 0)
		{
			sd->rowoffset = CheckInt(key);
		}
		else if (stricmp(key, "lm_sampledist") == 0)
		{
			sd->sampling.SetGeneralSampleDistance(CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_top") == 0)
		{
			sd->sampling.SetSampleDistance(WallPart::TOP, CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_mid") == 0)
		{
			sd->sampling.SetSampleDistance(WallPart::MIDDLE, CheckInt(key));
		}
		else if (stricmp(key, "lm_sampledist_bot") == 0)
		{
			sd->sampling.SetSampleDistance(WallPart::BOTTOM, CheckInt(key));
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

void FProcessor::ParseSector(IntSector *sec)
{
	std::vector<int> moreids;
	memset(&sec->data, 0, sizeof(sec->data));
	sec->data.lightlevel = 160;
	sec->sampleDistanceCeiling = 0;
	sec->sampleDistanceFloor = 0;

	int ceilingplane = 0, floorplane = 0;
	bool floorTexZSet = false;
	bool ceilingTexZSet = false;

	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		const char *value;
		const char *key = ParseKey(value);

		if (stricmp(key, "heightfloor") == 0)
		{
			sec->floorTexZ = CheckFloat(key);
			floorTexZSet = true;
		}
		else if (stricmp(key, "heightceiling") == 0)
		{
			sec->ceilingTexZ = CheckFloat(key);
			ceilingTexZSet = true;
		}
		if (stricmp(key, "textureceiling") == 0)
		{
			CopyUDMFString(sec->data.ceilingpic, 64, value);
		}
		else if (stricmp(key, "texturefloor") == 0)
		{
			CopyUDMFString(sec->data.floorpic, 64, value);
		}
		else if (stricmp(key, "heightceiling") == 0)
		{
			sec->data.ceilingheight = CheckFloat(key);
			if (!ceilingTexZSet)
				sec->ceilingTexZ = sec->data.ceilingheight;
		}
		else if (stricmp(key, "heightfloor") == 0)
		{
			sec->data.floorheight = CheckFloat(key);
			if (!floorTexZSet)
				sec->floorTexZ = sec->data.floorheight;
		}
		else if (stricmp(key, "lightlevel") == 0)
		{
			sec->data.lightlevel = CheckInt(key);
		}
		else if (stricmp(key, "special") == 0)
		{
			sec->data.special = CheckInt(key);
		}
		else if (stricmp(key, "id") == 0)
		{
			int id = CheckInt(key);
			sec->data.tag = (short)id;
			sec->tags.Clear();
			if (id != 0) sec->tags.Push(id);
		}
		else if (stricmp(key, "ceilingplane_a") == 0)
		{
			ceilingplane|=1;
			sec->ceilingplane.a = CheckFloat(key);
		}
		else if (stricmp(key, "ceilingplane_b") == 0)
		{
			ceilingplane|=2;
			sec->ceilingplane.b = CheckFloat(key);
		}
		else if (stricmp(key, "ceilingplane_c") == 0)
		{
			ceilingplane|=4;
			sec->ceilingplane.c = CheckFloat(key);
		}
		else if (stricmp(key, "ceilingplane_d") == 0)
		{
			ceilingplane|=8;
			sec->ceilingplane.d = CheckFloat(key);
		}
		else if (stricmp(key, "floorplane_a") == 0)
		{
			floorplane|=1;
			sec->floorplane.a = CheckFloat(key);
		}
		else if (stricmp(key, "floorplane_b") == 0)
		{
			floorplane|=2;
			sec->floorplane.b = CheckFloat(key);
		}
		else if (stricmp(key, "floorplane_c") == 0)
		{
			floorplane|=4;
			sec->floorplane.c = CheckFloat(key);
		}
		else if (stricmp(key, "floorplane_d") == 0)
		{
			floorplane|=8;
			sec->floorplane.d = CheckFloat(key);
		}
		else if (stricmp(key, "moreids") == 0)
		{
			// delay parsing of the tag string until parsing of the sector is complete
			// This ensures that the ID is always the first tag in the list.
			auto tagstring = value;
			if (tagstring != nullptr && *tagstring == '"')
			{
				// skip the quotation mark
				auto workstring = strdup(tagstring + 1);
				for (char* token = strtok(workstring, " \""); token; token = strtok(nullptr, " \""))
				{
					auto tag = strtoll(token, nullptr, 0);
					if (tag != 0 && (int)tag == tag)
					{
						moreids.push_back(tag);
					}
				}
				free(workstring);
			}
		}
		else if (stricmp(key, "lm_sampledist_floor") == 0)
		{
			sec->sampleDistanceFloor = CheckInt(key);
		}
		else if (stricmp(key, "lm_sampledist_ceiling") == 0)
		{
			sec->sampleDistanceCeiling = CheckInt(key);
		}

		// now store the key in its unprocessed form
		UDMFKey k = {key, value};
		sec->props.Push(k);
	}

	if (ceilingplane != 15)
	{
		sec->ceilingplane.a = 0.0f;
		sec->ceilingplane.b = 0.0f;
		sec->ceilingplane.c = -1.0f;
		sec->ceilingplane.d = -sec->data.ceilingheight;
	}
	else
	{
		float scale = 1.0f / sec->ceilingplane.Normal().Length();
		sec->ceilingplane.a *= scale;
		sec->ceilingplane.b *= scale;
		sec->ceilingplane.c *= scale;
		sec->ceilingplane.d *= scale;
		sec->ceilingplane.d = -sec->ceilingplane.d;
	}

	if (floorplane != 15)
	{
		sec->floorplane.a = 0.0f;
		sec->floorplane.b = 0.0f;
		sec->floorplane.c = 1.0f;
		sec->floorplane.d = sec->data.floorheight;
	}
	else
	{
		float scale = 1.0f / sec->floorplane.Normal().Length();
		sec->floorplane.a *= scale;
		sec->floorplane.b *= scale;
		sec->floorplane.c *= scale;
		sec->floorplane.d *= scale;
		sec->floorplane.d = -sec->floorplane.d;
	}

	for (int tag : moreids)
		sec->tags.Push(tag);	// don't bother with duplicates, they don't pose a problem.
}

//===========================================================================
//
// parse a vertex block
//
//===========================================================================

void FProcessor::ParseVertex(WideVertex *vt, IntVertex *vtp)
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
		if (!stricmp(key, "zfloor"))
		{
			vtp->zfloor = CheckFloat(key);
		}
		else if (!stricmp(key, "zceiling"))
		{
			vtp->zceiling = CheckFloat(key);
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

void FProcessor::ParseMapProperties()
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

void FProcessor::ParseTextMap(int lump)
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

void FProcessor::LoadUDMF()
{
	ParseTextMap(Lump+1);
}

//===========================================================================
//
// writes a property list
//
//===========================================================================

void FProcessor::WriteProps(FWadWriter &out, TArray<UDMFKey> &props)
{
	for(unsigned i=0; i< props.Size(); i++)
	{
		out.AddToLump(props[i].key, (int)strlen(props[i].key));
		out.AddToLump(" = ", 3);
		out.AddToLump(props[i].value, (int)strlen(props[i].value));
		out.AddToLump(";\n", 2);
	}
}

//===========================================================================
//
// writes an integer property
//
//===========================================================================

void FProcessor::WriteIntProp(FWadWriter &out, const char *key, int value)
{
	char buffer[20];

	out.AddToLump(key, (int)strlen(key));
	out.AddToLump(" = ", 3);
	sprintf(buffer, "%d;\n", value);
	out.AddToLump(buffer, (int)strlen(buffer));
}

//===========================================================================
//
// writes a UDMF thing
//
//===========================================================================

void FProcessor::WriteThingUDMF(FWadWriter &out, IntThing *th, int num)
{
	out.AddToLump("thing", 5);
	if (WriteComments)
	{
		char buffer[32];
		int len = sprintf(buffer, " // %d", num);
		out.AddToLump(buffer, len);
	}
	out.AddToLump("\n{\n", 3);
	WriteProps(out, th->props);
	out.AddToLump("}\n\n", 3);
}

//===========================================================================
//
// writes a UDMF linedef
//
//===========================================================================

void FProcessor::WriteLinedefUDMF(FWadWriter &out, IntLineDef *ld, int num)
{
	out.AddToLump("linedef", 7);
	if (WriteComments)
	{
		char buffer[32];
		int len = sprintf(buffer, " // %d", num);
		out.AddToLump(buffer, len);
	}
	out.AddToLump("\n{\n", 3);
	WriteIntProp(out, "v1", ld->v1);
	WriteIntProp(out, "v2", ld->v2);
	if (ld->sidenum[0] != NO_INDEX) WriteIntProp(out, "sidefront", ld->sidenum[0]);
	if (ld->sidenum[1] != NO_INDEX) WriteIntProp(out, "sideback", ld->sidenum[1]);
	WriteProps(out, ld->props);
	out.AddToLump("}\n\n", 3);
}

//===========================================================================
//
// writes a UDMF sidedef
//
//===========================================================================

void FProcessor::WriteSidedefUDMF(FWadWriter &out, IntSideDef *sd, int num)
{
	out.AddToLump("sidedef", 7);
	if (WriteComments)
	{
		char buffer[32];
		int len = sprintf(buffer, " // %d", num);
		out.AddToLump(buffer, len);
	}
	out.AddToLump("\n{\n", 3);
	WriteIntProp(out, "sector", sd->sector);
	WriteProps(out, sd->props);
	out.AddToLump("}\n\n", 3);
}

//===========================================================================
//
// writes a UDMF sector
//
//===========================================================================

void FProcessor::WriteSectorUDMF(FWadWriter &out, IntSector *sec, int num)
{
	out.AddToLump("sector", 6);
	if (WriteComments)
	{
		char buffer[32];
		int len = sprintf(buffer, " // %d", num);
		out.AddToLump(buffer, len);
	}
	out.AddToLump("\n{\n", 3);
	WriteProps(out, sec->props);
	out.AddToLump("}\n\n", 3);
}

//===========================================================================
//
// writes a UDMF vertex
//
//===========================================================================

void FProcessor::WriteVertexUDMF(FWadWriter &out, IntVertex *vt, int num)
{
	out.AddToLump("vertex", 6);
	if (WriteComments)
	{
		char buffer[32];
		int len = sprintf(buffer, " // %d", num);
		out.AddToLump(buffer, len);
	}
	out.AddToLump("\n{\n", 3);
	WriteProps(out, vt->props);
	out.AddToLump("}\n\n", 3);
}

//===========================================================================
//
// writes a UDMF text map
//
//===========================================================================

void FProcessor::WriteTextMap(FWadWriter &out)
{
	out.StartWritingLump("TEXTMAP");
	WriteProps(out, Level.props);
	for(int i = 0; i < Level.NumThings(); i++)
	{
		WriteThingUDMF(out, &Level.Things[i], i);
	}

	for(int i = 0; i < Level.NumOrgVerts; i++)
	{
		WideVertex *vt = &Level.Vertices[i];
		if (vt->index <= 0)
		{
			// not valid!
			throw std::runtime_error("Invalid vertex data.");
		}
		WriteVertexUDMF(out, &Level.VertexProps[vt->index-1], i);
	}

	for(int i = 0; i < Level.NumLines(); i++)
	{
		WriteLinedefUDMF(out, &Level.Lines[i], i);
	}

	for(int i = 0; i < Level.NumSides(); i++)
	{
		WriteSidedefUDMF(out, &Level.Sides[i], i);
	}

	for(int i = 0; i < Level.NumSectors(); i++)
	{
		WriteSectorUDMF(out, &Level.Sectors[i], i);
	}
}

//===========================================================================
//
// writes an UDMF map
//
//===========================================================================

void FProcessor::WriteUDMF(FWadWriter &out)
{
	out.CopyLump (Wad, Lump);
	WriteTextMap(out);
	if (ForceCompression) WriteGLBSPZ (out, "ZNODES");
	else WriteGLBSPX (out, "ZNODES");

	// copy everything except existing nodes, blockmap and reject
	for(int i=Lump+2; stricmp(Wad.LumpName(i), "ENDMAP") && i < Wad.NumLumps(); i++)
	{
		const char *lumpname = Wad.LumpName(i);
		if (stricmp(lumpname, "ZNODES") &&
			stricmp(lumpname, "BLOCKMAP") &&
			stricmp(lumpname, "REJECT") &&
			stricmp(lumpname, "LIGHTMAP"))
		{
			out.CopyLump(Wad, i);
		}
	}

	if (LightmapMesh)
	{
		LightmapMesh->AddLightmapLump(Level, out);
	}

	out.CreateLabel("ENDMAP");
}
