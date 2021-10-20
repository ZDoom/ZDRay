
#pragma once

#include "wad/wad.h"
#include "level/doomdata.h"
#include "level/workdata.h"
#include "framework/tarray.h"
#include "nodebuilder/nodebuild.h"
#include "blockmapbuilder/blockmapbuilder.h"
#include "lightmap/surfaces.h"
#include <zlib.h>

#define DEFINE_SPECIAL(name, num, min, max, map) name = num,

typedef enum {
#include "actionspecials.h"
} linespecial_t;
#undef DEFINE_SPECIAL

typedef enum {
	Init_Gravity = 0,
	Init_Color = 1,
	Init_Damage = 2,
	Init_SectorLink = 3,
	NUM_STATIC_INITS,
	Init_EDSector = 253,
	Init_EDLine = 254,
	Init_TransferSky = 255
} staticinit_t;

class ZLibOut
{
public:
	ZLibOut(FWadWriter &out);
	~ZLibOut();

	ZLibOut &operator << (uint8_t);
	ZLibOut &operator << (uint16_t);
	ZLibOut &operator << (int16_t);
	ZLibOut &operator << (uint32_t);
	ZLibOut &operator << (fixed_t);
	void Write(uint8_t *data, int len);

private:
	enum { BUFFER_SIZE = 8192 };

	z_stream Stream;
	uint8_t Buffer[BUFFER_SIZE];

	FWadWriter &Out;
};

class FProcessor
{
public:
	FProcessor(FWadReader &inwad, int lump);

	void BuildNodes();
	void BuildLightmaps();
	void Write(FWadWriter &out);

private:
	void LoadUDMF();
	void LoadThings();
	void LoadLines();
	void LoadVertices();
	void LoadSides();
	void LoadSectors();
	void GetPolySpots();
	void SetLineID(IntLineDef *ld);

	MapNodeEx *NodesToEx(const MapNode *nodes, int count);
	MapSubsectorEx *SubsectorsToEx(const MapSubsector *ssec, int count);
	MapSegGLEx *SegGLsToEx(const MapSegGL *segs, int count);

	uint8_t *FixReject(const uint8_t *oldreject);
	bool CheckForFracSplitters(const MapNodeEx *nodes, int count);

	void WriteLines(FWadWriter &out);
	void WriteVertices(FWadWriter &out, int count);
	void WriteSectors(FWadWriter &out);
	void WriteSides(FWadWriter &out);
	void WriteSegs(FWadWriter &out);
	void WriteSSectors(FWadWriter &out) const;
	void WriteNodes(FWadWriter &out) const;
	void WriteBlockmap(FWadWriter &out);
	void WriteReject(FWadWriter &out);

	void WriteGLVertices(FWadWriter &out, bool v5);
	void WriteGLSegs(FWadWriter &out, bool v5);
	void WriteGLSegs5(FWadWriter &out);
	void WriteGLSSect(FWadWriter &out, bool v5);
	void WriteGLNodes(FWadWriter &out, bool v5);

	void WriteBSPZ(FWadWriter &out, const char *label);
	void WriteGLBSPZ(FWadWriter &out, const char *label);

	void WriteVerticesZ(ZLibOut &out, const WideVertex *verts, int orgverts, int newverts);
	void WriteSubsectorsZ(ZLibOut &out, const MapSubsectorEx *subs, int numsubs);
	void WriteSegsZ(ZLibOut &out, const MapSegEx *segs, int numsegs);
	void WriteGLSegsZ(ZLibOut &out, const MapSegGLEx *segs, int numsegs, int nodever);
	void WriteNodesZ(ZLibOut &out, const MapNodeEx *nodes, int numnodes, int nodever);

	void WriteBSPX(FWadWriter &out, const char *label);
	void WriteGLBSPX(FWadWriter &out, const char *label);

	void WriteVerticesX(FWadWriter &out, const WideVertex *verts, int orgverts, int newverts);
	void WriteSubsectorsX(FWadWriter &out, const MapSubsectorEx *subs, int numsubs);
	void WriteSegsX(FWadWriter &out, const MapSegEx *segs, int numsegs);
	void WriteGLSegsX(FWadWriter &out, const MapSegGLEx *segs, int numsegs, int nodever);
	void WriteNodesX(FWadWriter &out, const MapNodeEx *nodes, int numnodes, int nodever);

	void WriteNodes2(FWadWriter &out, const char *name, const MapNodeEx *zaNodes, int count) const;
	void WriteSSectors2(FWadWriter &out, const char *name, const MapSubsectorEx *zaSubs, int count) const;
	void WriteNodes5(FWadWriter &out, const char *name, const MapNodeEx *zaNodes, int count) const;
	void WriteSSectors5(FWadWriter &out, const char *name, const MapSubsectorEx *zaSubs, int count) const;

	const char *ParseKey(const char *&value);
	bool CheckKey(const char *&key, const char *&value);
	void ParseThing(IntThing *th);
	void ParseLinedef(IntLineDef *ld);
	void ParseSidedef(IntSideDef *sd);
	void ParseSector(IntSector *sec);
	void ParseVertex(WideVertex *vt, IntVertex *vtp);
	void ParseMapProperties();
	void ParseTextMap(int lump);

	void WriteProps(FWadWriter &out, TArray<UDMFKey> &props);
	void WriteIntProp(FWadWriter &out, const char *key, int value);
	void WriteThingUDMF(FWadWriter &out, IntThing *th, int num);
	void WriteLinedefUDMF(FWadWriter &out, IntLineDef *ld, int num);
	void WriteSidedefUDMF(FWadWriter &out, IntSideDef *sd, int num);
	void WriteSectorUDMF(FWadWriter &out, IntSector *sec, int num);
	void WriteVertexUDMF(FWadWriter &out, IntVertex *vt, int num);
	void WriteTextMap(FWadWriter &out);
	void WriteUDMF(FWadWriter &out);

	FLevel Level;

	TArray<FNodeBuilder::FPolyStart> PolyStarts;
	TArray<FNodeBuilder::FPolyStart> PolyAnchors;

	bool Extended;
	bool isUDMF;

	FWadReader &Wad;
	int Lump;

	bool NodesBuilt = false;
	std::unique_ptr<LevelMesh> LightmapMesh;
};
