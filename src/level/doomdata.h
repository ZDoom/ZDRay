
#pragma once

#include "framework/tarray.h"
#include "math/mathlib.h"
#include <memory>
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
	uint16_t	sector;
};

struct IntLineDef;

struct IntSideDef
{
	// the first 5 values are only used for binary format maps
	short	textureoffset;
	short	rowoffset;
	char	toptexture[8];
	char	bottomtexture[8];
	char	midtexture[8];

	int sector;
	int lightdef;

	IntLineDef *line;

	TArray<UDMFKey> props;
};

struct MapLineDef
{
	uint16_t	v1;
	uint16_t	v2;
	short	flags;
	short	special;
	short	tag;
	uint16_t	sidenum[2];
};

struct MapLineDef2
{
	uint16_t	v1;
	uint16_t	v2;
	short	flags;
	unsigned char	special;
	unsigned char	args[5];
	uint16_t	sidenum[2];
};

struct IntLineDef
{
	uint32_t v1;
	uint32_t v2;
	int flags;
	int special;
	int args[5];
	uint32_t sidenum[2];

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

	Plane ceilingplane;
	Plane floorplane;

	int floorlightdef;
	int ceilinglightdef;

	bool controlsector;
	TArray<IntSector*> x3dfloors;
	bool skySector;

	TArray<UDMFKey> props;
};

struct MapSubsector
{
	uint16_t	numlines;
	uint16_t	firstline;
};

struct MapSubsectorEx
{
	uint32_t	numlines;
	uint32_t	firstline;
};

struct MapSeg
{
	uint16_t	v1;
	uint16_t	v2;
	uint16_t	angle;
	uint16_t	linedef;
	short	side;
	short	offset;
};

struct MapSegEx
{
	uint32_t	v1;
	uint32_t	v2;
	uint16_t	angle;
	uint16_t	linedef;
	short	side;
	short	offset;
};

struct MapSegGL
{
	uint16_t	v1;
	uint16_t	v2;
	uint16_t	linedef;
	uint16_t	side;
	uint16_t	partner;
};

struct MapSegGLEx
{
	uint32_t	v1;
	uint32_t	v2;
	uint32_t	linedef;
	uint16_t	side;
	uint32_t	partner;
};

#define NF_SUBSECTOR	0x8000
#define NFX_SUBSECTOR	0x80000000

struct MapNode
{
	short 	x,y,dx,dy;
	short 	bbox[2][4];
	uint16_t	children[2];
};

struct MapNodeExO
{
	short	x,y,dx,dy;
	short	bbox[2][4];
	uint32_t	children[2];
};

struct MapNodeEx
{
	int		x,y,dx,dy;
	short	bbox[2][4];
	uint32_t	children[2];
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

	short pitch; // UDMF
	float height; // UDMF

	TArray<UDMFKey> props;
};

struct IntVertex
{
	TArray<UDMFKey> props;
};

class BBox;
class Vec3;
class Vec2;
struct vertex_t;
struct Surface;
struct thingLight_t;

struct FloatVertex
{
	float x;
	float y;
};

struct lightDef_t
{
	int             doomednum;
	float           height;
	float           radius;
	float           intensity;
	float           falloff;
	bool            bCeiling;
	Vec3         rgb;
};

struct mapDef_t
{
	int             map;
	int             sunIgnoreTag;
	Vec3         sunDir;
	Vec3         sunColor;
};

struct thingLight_t
{
	IntThing        *mapThing;
	Vec2         origin;
	Vec3         rgb;
	float           intensity;
	float           innerAngleCos;
	float           outerAngleCos;
	float           height;
	float           radius;
	bool            bCeiling;
	IntSector       *sector;
	MapSubsectorEx  *ssect;
};

struct surfaceLightDef
{
	float           distance;
	float           intensity;
	Vec3         rgb;
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
#define NO_LINE_INDEX           0xffffffff

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
	uint16_t *Blockmap;				int BlockmapSize;
	uint8_t *Reject;				int RejectSize;

	MapSubsectorEx *GLSubsectors;	int NumGLSubsectors;
	MapSegGLEx *GLSegs;				int NumGLSegs;
	MapNodeEx *GLNodes;				int NumGLNodes;
	WideVertex *GLVertices;			int NumGLVertices;
	uint8_t *GLPVS;					int GLPVSSize;

	int NumOrgVerts;

	uint32_t *OrgSectorMap;			int NumOrgSectors;

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

	TArray<thingLight_t> ThingLights;
	TArray<surfaceLightDef> SurfaceLights;

	void SetupDlight();
	void CreateLights();

	const Vec3 &GetSunColor() const;
	const Vec3 &GetSunDirection() const;
	IntSector *GetFrontSector(const IntSideDef *side);
	IntSector *GetBackSector(const IntSideDef *side);
	IntSector *GetSectorFromSubSector(const MapSubsectorEx *sub);
	MapSubsectorEx *PointInSubSector(const int x, const int y);
	FloatVertex GetSegVertex(int index);

private:
	void CheckSkySectors();
};

const int BLOCKSIZE = 128;
const int BLOCKFRACSIZE = BLOCKSIZE<<FRACBITS;
const int BLOCKBITS = 7;
const int BLOCKFRACBITS = FRACBITS+7;
