
#pragma once

#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include "math/mathlib.h"
#include <memory>
#include <cmath>
#undef MIN
#undef MAX
#undef min
#undef max

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
	char	toptexture[64/*8*/];
	char	bottomtexture[64/*8*/];
	char	midtexture[64/*8*/];
	uint16_t	sector;
};

struct IntLineDef;

struct IntSideDef
{
	// the first 5 values are only used for binary format maps
	short	textureoffset;
	short	rowoffset;
	char	toptexture[64/*8*/];
	char	bottomtexture[64/*8*/];
	char	midtexture[64/*8*/];

	int sector;
	int lightdef;

	IntLineDef *line;

	int sampleDistance;
	int sampleDistanceTop;
	int sampleDistanceMiddle;
	int sampleDistanceBottom;

	inline int GetSampleDistanceTop() const { return sampleDistanceTop ? sampleDistanceTop : sampleDistance; }
	inline int GetSampleDistanceMiddle() const { return sampleDistanceMiddle ? sampleDistanceMiddle : sampleDistance; }
	inline int GetSampleDistanceBottom() const { return sampleDistanceBottom ? sampleDistanceBottom : sampleDistance; }

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

struct IntSector;

struct IntLineDef
{
	uint32_t v1;
	uint32_t v2;
	int flags;
	int special;
	int args[5];
	uint32_t sidenum[2];

	TArray<UDMFKey> props;
	TArray<int> ids;

	IntSector *frontsector, *backsector;
};

struct MapSector
{
	short	floorheight;
	short	ceilingheight;
	char	floorpic[64/*8*/];
	char	ceilingpic[64/*8*/];
	short	lightlevel;
	short	special;
	short	tag;
};

enum SecPlaneType
{
	PLANE_FLOOR,
	PLANE_CEILING,
};

struct IntSector
{
	// none of the sector properties are used by the node builder
	// so there's no need to store them in their expanded form for
	// UDMF. Just storing the UDMF keys and leaving the binary fields
	// empty is enough
	MapSector data;
	TArray<int> tags;

	Plane ceilingplane;
	Plane floorplane;

	int sampleDistanceCeiling;
	int sampleDistanceFloor;

	int floorlightdef;
	int ceilinglightdef;

	bool controlsector;
	TArray<IntSector*> x3dfloors;

	union
	{
		bool skyPlanes[2];
		struct { bool skyFloor, skyCeiling; };
	};

	inline const char* GetTextureName(int plane) const { return plane != PLANE_FLOOR ? data.ceilingpic : data.floorpic; }

	TArray<UDMFKey> props;

	TArray<IntLineDef*> lines;
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
	float		alpha;
	uint8_t		special;
	uint8_t		args[5];
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
	int			special;
	int			args[5];
	FString		arg0str;

	short pitch; // UDMF
	float height; // UDMF
	float alpha;

	TArray<UDMFKey> props;
};

struct IntVertex
{
	TArray<UDMFKey> props;
	double zfloor = 100000, zceiling = 100000;

	inline bool HasZFloor() const { return zfloor != 100000; }
	inline bool HasZCeiling() const { return zceiling != 100000; }
};

class BBox;
struct vertex_t;
struct Surface;
struct ThingLight;

struct FloatVertex
{
	float x;
	float y;
};

#define THING_POINTLIGHT_STATIC	9876
#define THING_SPOTLIGHT_STATIC	9881
#define THING_LIGHTPROBE		9875
#define THING_ZDRAYINFO			9890

struct ThingLight
{
	IntThing        *mapThing;
	vec2            origin;
	vec3            rgb;
	float           intensity;
	float           innerAngleCos;
	float           outerAngleCos;
	float           height;
	float           radius;
	bool            bCeiling;
	IntSector       *sector;
	MapSubsectorEx  *ssect;

	vec3 LightOrigin() const
	{
		float originZ;
		if (!bCeiling)
			originZ = sector->floorplane.zAt(origin.x, origin.y) + height;
		else
			originZ = sector->ceilingplane.zAt(origin.x, origin.y) - height;
		return vec3(origin.x, origin.y, originZ);
	}

	float LightRadius() const
	{
		return radius + radius; // 2.0 because gzdoom's dynlights do this and we want them to match
	}

	float SpotAttenuation(const vec3& dir) const
	{
		float spotAttenuation = 1.0f;
		if (outerAngleCos > -1.0f)
		{
			float negPitch = -radians(mapThing->pitch);
			float xyLen = std::cos(negPitch);
			vec3 spotDir;
			spotDir.x = -std::cos(radians(mapThing->angle)) * xyLen;
			spotDir.y = -std::sin(radians(mapThing->angle)) * xyLen;
			spotDir.z = -std::sin(negPitch);
			float cosDir = dot(dir, spotDir);
			spotAttenuation = smoothstep(outerAngleCos, innerAngleCos, cosDir);
			spotAttenuation = std::max(spotAttenuation, 0.0f);
		}
		return spotAttenuation;
	}

	vec3 SpotDir() const
	{
		if (outerAngleCos > -1.0f)
		{
			float negPitch = -radians(mapThing->pitch);
			float xyLen = std::cos(negPitch);
			vec3 spotDir;
			spotDir.x = -std::cos(radians(mapThing->angle)) * xyLen;
			spotDir.y = -std::sin(radians(mapThing->angle)) * xyLen;
			spotDir.z = -std::sin(negPitch);
			return spotDir;
		}
		else
		{
			return vec3(0.0f);
		}
	}

	float DistAttenuation(float distance) const
	{
		return std::max(1.0f - (distance / LightRadius()), 0.0f);
	}
};

struct SurfaceLightDef
{
	float           distance;
	float           intensity;
	vec3            rgb;
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

	TArray<ThingLight> ThingLights;
	TArray<SurfaceLightDef> SurfaceLights;
	TArray<int> ThingLightProbes;

	vec3 defaultSunColor;
	vec3 defaultSunDirection;
	int DefaultSamples;
	int LightBounce;
	float GridSize;

	void FindMapBounds ();
	void RemoveExtraLines ();
	void RemoveExtraSides ();
	void RemoveExtraSectors ();

	void PostLoadInitialization();

	void SetupLights();

	int NumSides() const { return Sides.Size(); }
	int NumLines() const { return Lines.Size(); }
	int NumSectors() const { return Sectors.Size(); }
	int NumThings() const { return Things.Size(); }

	const vec3 &GetSunColor() const;
	const vec3 &GetSunDirection() const;
	IntSector *GetFrontSector(const IntSideDef *side);
	IntSector *GetBackSector(const IntSideDef *side);
	IntSector *GetSectorFromSubSector(const MapSubsectorEx *sub);
	MapSubsectorEx *PointInSubSector(const int x, const int y);
	FloatVertex GetSegVertex(unsigned int index);

	vec3 GetLightProbePosition(int index);

	int FindFirstSectorFromTag(int tag);

	inline IntSector* PointInSector(const dvec2& pos) { return GetSectorFromSubSector(PointInSubSector(int(pos.x), int(pos.y))); }
private:
	void CheckSkySectors();
	void CreateLights();
};

const int BLOCKSIZE = 128;
const int BLOCKFRACSIZE = BLOCKSIZE<<FRACBITS;
const int BLOCKBITS = 7;
const int BLOCKFRACBITS = FRACBITS+7;
