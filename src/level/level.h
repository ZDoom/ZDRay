
#pragma once

#include "wad/wad.h"
#include "level/doomdata.h"
#include "level/workdata.h"
#include "framework/tarray.h"
#include "nodebuilder/nodebuild.h"
#include "blockmapbuilder/blockmapbuilder.h"
#include <zlib.h>

class ZLibOut
{
public:
	ZLibOut (FWadWriter &out);
	~ZLibOut ();

	ZLibOut &operator << (BYTE);
	ZLibOut &operator << (WORD);
	ZLibOut &operator << (SWORD);
	ZLibOut &operator << (DWORD);
	ZLibOut &operator << (fixed_t);
	void Write (BYTE *data, int len);

private:
	enum { BUFFER_SIZE = 8192 };

	z_stream Stream;
	BYTE Buffer[BUFFER_SIZE];

	FWadWriter &Out;
};

class FLevelLoader
{
public:
	FLevelLoader (FWadReader &inwad, int lump);

	void BuildNodes ();

private:
	void LoadUDMF();
	void LoadThings ();
	void LoadLines ();
	void LoadVertices ();
	void LoadSides ();
	void LoadSectors ();
	void GetPolySpots ();

	MapNodeEx *NodesToEx (const MapNode *nodes, int count);
	MapSubsectorEx *SubsectorsToEx (const MapSubsector *ssec, int count);
	MapSegGLEx *SegGLsToEx (const MapSegGL *segs, int count);

	BYTE *FixReject (const BYTE *oldreject);
	bool CheckForFracSplitters(const MapNodeEx *nodes, int count);

	const char *ParseKey(const char *&value);
	bool CheckKey(const char *&key, const char *&value);
	void ParseThing(IntThing *th);
	void ParseLinedef(IntLineDef *ld);
	void ParseSidedef(IntSideDef *sd);
	void ParseSector(IntSector *sec);
	void ParseVertex(WideVertex *vt, IntVertex *vtp);
	void ParseMapProperties();
	void ParseTextMap(int lump);

	FLevel Level;

	TArray<FNodeBuilder::FPolyStart> PolyStarts;
	TArray<FNodeBuilder::FPolyStart> PolyAnchors;

	bool Extended;
	bool isUDMF;

	FWadReader &Wad;
	int Lump;
};
