
#pragma once

#include "framework/tarray.h"
#include "lightmap/kexlib/math/mathlib.h"
#undef MIN
#undef MAX

enum
{
	BOXTOP, BOXBOTTOM, BOXLEFT, BOXRIGHT
};

struct UDMFKey
{
	const char *key;
	const char *value;
};

struct MapVertex
{
	short x, y;
};

struct WideVertex
{
	fixed_t x, y;
	int index;
};

struct MapSideDef
{
	short	textureoffset;
	short	rowoffset;
	char	toptexture[8];
	char	bottomtexture[8];
	char	midtexture[8];
	WORD	sector;
};

struct IntSideDef
{
	// the first 5 values are only used for binary format maps
	short	textureoffset;
	short	rowoffset;
	char	toptexture[8];
	char	bottomtexture[8];
	char	midtexture[8];

	int sector;

	TArray<UDMFKey> props;
};

struct MapLineDef
{
	WORD	v1;
	WORD	v2;
	short	flags;
	short	special;
	short	tag;
	WORD	sidenum[2];
};

struct MapLineDef2
{
	WORD	v1;
	WORD	v2;
	short	flags;
	unsigned char	special;
	unsigned char	args[5];
	WORD	sidenum[2];
};

struct IntLineDef
{
	DWORD v1;
	DWORD v2;
	int flags;
	int special;
	int args[5];
	DWORD sidenum[2];

	TArray<UDMFKey> props;
};

struct MapSector
{
	short	floorheight;
	short	ceilingheight;
	char	floorpic[8];
	char	ceilingpic[8];
	short	lightlevel;
	short	special;
	short	tag;
};

struct IntSector
{
	// none of the sector properties are used by the node builder
	// so there's no need to store them in their expanded form for
	// UDMF. Just storing the UDMF keys and leaving the binary fields
	// empty is enough
	MapSector data;

	TArray<UDMFKey> props;
};

struct MapSubsector
{
	WORD	numlines;
	WORD	firstline;
};

struct MapSubsectorEx
{
	DWORD	numlines;
	DWORD	firstline;
};

struct MapSeg
{
	WORD	v1;
	WORD	v2;
	WORD	angle;
	WORD	linedef;
	short	side;
	short	offset;
};

struct MapSegEx
{
	DWORD	v1;
	DWORD	v2;
	WORD	angle;
	WORD	linedef;
	short	side;
	short	offset;
};

struct MapSegGL
{
	WORD	v1;
	WORD	v2;
	WORD	linedef;
	WORD	side;
	WORD	partner;
};

struct MapSegGLEx
{
	DWORD	v1;
	DWORD	v2;
	DWORD	linedef;
	WORD	side;
	DWORD	partner;
};

#define NF_SUBSECTOR	0x8000
#define NFX_SUBSECTOR	0x80000000

struct MapNode
{
	short 	x,y,dx,dy;
	short 	bbox[2][4];
	WORD	children[2];
};

struct MapNodeExO
{
	short	x,y,dx,dy;
	short	bbox[2][4];
	DWORD	children[2];
};

struct MapNodeEx
{
	int		x,y,dx,dy;
	short	bbox[2][4];
	DWORD	children[2];
};

struct MapThing
{
	short		x;
	short		y;
	short		angle;
	short		type;
	short		flags;
};

struct MapThing2
{
	unsigned short thingid;
	short		x;
	short		y;
	short		z;
	short		angle;
	short		type;
	short		flags;
	char		special;
	char		args[5];
};

struct IntThing
{
	unsigned short thingid;
	fixed_t		x;	// full precision coordinates for UDMF support
	fixed_t		y;
	// everything else is not needed or has no extended form in UDMF
	short		z;
	short		angle;
	short		type;
	short		flags;
	char		special;
	char		args[5];

	TArray<UDMFKey> props;
};

struct IntVertex
{
	TArray<UDMFKey> props;
};

class kexBBox;
class kexVec3;
class kexVec2;
class kexLightSurface;
struct vertex_t;
struct surface_t;
struct thingLight_t;

struct FloatVertex
{
	float x;
	float y;
};

struct leaf_t
{
	FloatVertex vertex;
	MapSegGLEx *seg;
};

struct lightDef_t
{
	int             doomednum;
	float           height;
	float           radius;
	float           intensity;
	float           falloff;
	bool            bCeiling;
	kexVec3         rgb;
};

struct mapDef_t
{
	int             map;
	int             sunIgnoreTag;
	kexVec3         sunDir;
	kexVec3         sunColor;
};

struct thingLight_t
{
	IntThing        *mapThing;
	kexVec2         origin;
	kexVec3         rgb;
	float           intensity;
	float           falloff;
	float           height;
	float           radius;
	bool            bCeiling;
	IntSector       *sector;
	MapSubsectorEx  *ssect;
};

struct surfaceLightDef
{
	int             tag;
	float           outerCone;
	float           innerCone;
	float           falloff;
	float           distance;
	float           intensity;
	bool            bIgnoreFloor;
	bool            bIgnoreCeiling;
	bool            bNoCenterPoint;
	kexVec3         rgb;
};

enum mapFlags_t
{
	ML_BLOCKING = 1,    // Solid, is an obstacle.
	ML_BLOCKMONSTERS = 2,    // Blocks monsters only.
	ML_TWOSIDED = 4,    // Backside will not be present at all if not two sided.
	ML_TRANSPARENT1 = 2048, // 25% or 75% transcluency?
	ML_TRANSPARENT2 = 4096  // 25% or 75% transcluency?
};

#define NO_SIDE_INDEX           -1
#define NO_LINE_INDEX           0xFFFF
#define NF_SUBSECTOR            0x8000

struct FLevel
{
	FLevel ();
	~FLevel ();

	WideVertex *Vertices;		int NumVertices;
	TArray<IntVertex>			VertexProps;
	TArray<IntSideDef>			Sides;
	TArray<IntLineDef>			Lines;
	TArray<IntSector>			Sectors;
	TArray<IntThing>			Things;
	MapSubsectorEx *Subsectors;	int NumSubsectors;
	MapSegEx *Segs;				int NumSegs;
	MapNodeEx *Nodes;			int NumNodes;
	WORD *Blockmap;				int BlockmapSize;
	BYTE *Reject;				int RejectSize;

	MapSubsectorEx *GLSubsectors;	int NumGLSubsectors;
	MapSegGLEx *GLSegs;				int NumGLSegs;
	MapNodeEx *GLNodes;				int NumGLNodes;
	WideVertex *GLVertices;			int NumGLVertices;
	BYTE *GLPVS;					int GLPVSSize;

	int NumOrgVerts;

	DWORD *OrgSectorMap;			int NumOrgSectors;

	fixed_t MinX, MinY, MaxX, MaxY;

	TArray<UDMFKey> props;

	void FindMapBounds ();
	void RemoveExtraLines ();
	void RemoveExtraSides ();
	void RemoveExtraSectors ();

	int NumSides() const { return Sides.Size(); }
	int NumLines() const { return Lines.Size(); }
	int NumSectors() const { return Sectors.Size(); }
	int NumThings() const { return Things.Size(); }

	// Dlight helpers

	leaf_t *leafs = nullptr;
	uint8_t *mapPVS = nullptr;

	bool *bSkySectors = nullptr;
	bool *bSSectsVisibleToSky = nullptr;

	int numLeafs = 0;

	int *segLeafLookup = nullptr;
	int *ssLeafLookup = nullptr;
	int *ssLeafCount = nullptr;
	kexBBox *ssLeafBounds = nullptr;

	kexBBox *nodeBounds = nullptr;

	surface_t **segSurfaces[3] = { nullptr, nullptr, nullptr };
	surface_t **leafSurfaces[2] = { nullptr, nullptr };

	TArray<thingLight_t*> thingLights;
	TArray<kexLightSurface*> lightSurfaces;

	void SetupDlight();
	void ParseConfigFile(const char *file);
	void CreateLights();
	void CleanupThingLights();

	const kexVec3 &GetSunColor() const;
	const kexVec3 &GetSunDirection() const;
	IntSideDef *GetSideDef(const MapSegGLEx *seg);
	IntSector *GetFrontSector(const MapSegGLEx *seg);
	IntSector *GetBackSector(const MapSegGLEx *seg);
	IntSector *GetSectorFromSubSector(const MapSubsectorEx *sub);
	MapSubsectorEx *PointInSubSector(const int x, const int y);
	bool PointInsideSubSector(const float x, const float y, const MapSubsectorEx *sub);
	bool LineIntersectSubSector(const kexVec3 &start, const kexVec3 &end, const MapSubsectorEx *sub, kexVec2 &out);
	FloatVertex GetSegVertex(int index);
	bool CheckPVS(MapSubsectorEx *s1, MapSubsectorEx *s2);

private:
	void BuildNodeBounds();
	void BuildLeafs();
	void BuildPVS();
	void CheckSkySectors();

	TArray<lightDef_t> lightDefs;
	TArray<surfaceLightDef> surfaceLightDefs;
	TArray<mapDef_t> mapDefs;

	mapDef_t *mapDef = nullptr;
};

const int BLOCKSIZE = 128;
const int BLOCKFRACSIZE = BLOCKSIZE<<FRACBITS;
const int BLOCKBITS = 7;
const int BLOCKFRACBITS = FRACBITS+7;
