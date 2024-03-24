
#pragma once

#include "framework/zdray.h"
#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include "framework/vectors.h"
#include "framework/textureid.h"
#include <memory>
#include <cmath>
#include <optional>
#undef MIN
#undef MAX
#undef min
#undef max

struct FLevel;
struct DoomLevelMeshSurface;

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

enum class WallPart
{
	TOP,
	MIDDLE,
	BOTTOM,
};

struct SurfaceSampleProps
{
	int sampleDistance = 0;
};

struct SideDefSampleProps
{
	SurfaceSampleProps line;
	SurfaceSampleProps lineSegments[3];

	inline int GetSampleDistance(WallPart part) const
	{
		auto sampleDistance = lineSegments[static_cast<int>(part)].sampleDistance;
		return sampleDistance ? sampleDistance : line.sampleDistance;
	}

	inline void SetSampleDistance(WallPart part, int dist) { lineSegments[int(part)].sampleDistance = dist; }
	inline void SetGeneralSampleDistance(int dist) { line.sampleDistance = dist; }
};

struct IntLineDef;
struct IntSector;
class FTextureID;

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

	IntSector* sectordef;
	IntLineDef *line;

	SideDefSampleProps sampling;
	TArray<UDMFKey> props;

	FTextureID GetTexture(WallPart part)
	{
		const char* names[3] = { toptexture, midtexture, bottomtexture };
		return TexMan.CheckForTexture(names[(int)part], ETextureType::Wall);
	}

	float GetTextureYOffset(WallPart part) { return 0.0f; }
	float GetTextureYScale(WallPart part) { return 1.0f; }

	inline int GetSampleDistance(WallPart part) const;
	inline int GetSectorGroup() const;

	int Index(const FLevel& level) const;

	FVector2 V1(const FLevel& level) const;
	FVector2 V2(const FLevel& level) const;
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
	uint32_t v1 = NO_INDEX;
	uint32_t v2 = NO_INDEX;
	int flags = 0;
	int special = 0;
	int args[5] = {};
	uint32_t sidenum[2] = {NO_INDEX, NO_INDEX};

	TArray<UDMFKey> props;
	TArray<int> ids;

	IntSideDef* sidedef[2] = { nullptr, nullptr };
	IntSector *frontsector = nullptr, *backsector = nullptr;

	SideDefSampleProps sampling;

	inline int GetSectorGroup() const;

	int Index(const FLevel& level) const;
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

struct X3DFloor
{
	IntSector* Sector = nullptr;
	IntLineDef* Line = nullptr;
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

	double floorTexZ;
	double ceilingTexZ;

	int sampleDistanceCeiling;
	int sampleDistanceFloor;

	int floorlightdef;
	int ceilinglightdef;

	bool controlsector;
	TArray<X3DFloor> x3dfloors;

	bool HasLightmaps = false;

	union
	{
		bool skyPlanes[2];
		struct { bool skyFloor, skyCeiling; };
	};

	TArray<UDMFKey> props;

	TArray<IntLineDef*> lines;
	TArray<IntLineDef*> portals;

	int group = 0;

	FTextureID GetTexture(SecPlaneType plane)
	{
		return TexMan.CheckForTexture(GetTextureName(plane), ETextureType::Flat);
	}

	// Utility functions
	inline const char* GetTextureName(int plane) const { return plane != PLANE_FLOOR ? data.ceilingpic : data.floorpic; }

	inline bool HasTag(int sectorTag) const
	{
		if (tags.Size() <= 0 && sectorTag == 0)
			return true;
		for (auto tag : tags)
		{
			if (tag == sectorTag) return true;
		}
		return false;
	}

	int Index(const FLevel& level) const;
};

inline int IntLineDef::GetSectorGroup() const
{
	return frontsector ? frontsector->group : (backsector ? backsector->group : 0);
}

inline int IntSideDef::GetSampleDistance(WallPart part) const
{
	auto sampleDistance = sampling.GetSampleDistance(part);
	return sampleDistance ? sampleDistance : line->sampling.GetSampleDistance(part);
}

inline int IntSideDef::GetSectorGroup() const
{
	return line ? line->GetSectorGroup() : 0;
}

struct MapSubsector
{
	uint16_t	numlines;
	uint16_t	firstline;
};

struct MapSegGLEx;
struct IntSector;

struct MapSubsectorEx
{
	uint32_t	numlines;
	uint32_t	firstline;

	MapSegGLEx* GetFirstLine(const FLevel& level) const;
	IntSector* GetSector(const FLevel& level) const;
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
	unsigned short thingid = 0;
	fixed_t		x = 0;	// full precision coordinates for UDMF support
	fixed_t		y = 0;
	// everything else is not needed or has no extended form in UDMF
	short		z = 0;
	short		angle = 0;
	short		type = 0;
	short		flags = 0;
	int			special = 0;
	int			args[5] = {};
	FString		arg0str;

	short pitch = 0; // UDMF
	float height = 0; // UDMF
	float alpha = 1.0;

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

#define THING_POINTLIGHT_LM	9876
#define THING_SPOTLIGHT_LM	9881
#define THING_ZDRAYINFO		9890

struct ThingLight
{
	IntThing        *mapThing;
	FVector2            origin;
	FVector3            rgb;
	float           intensity;
	float           innerAngleCos;
	float           outerAngleCos;
	float           height;
	float           radius;
	float           sourceRadius;
	bool            bCeiling;
	IntSector       *sector;
	MapSubsectorEx  *ssect;

	// Locations in the level mesh light list. Ends with index = 0 or all entries used
	enum { max_levelmesh_entries = 4 };
	struct
	{
		int index = 0;
		int portalgroup = 0;
	} levelmesh[max_levelmesh_entries];

	// Portal related functionality
	std::optional<FVector3> relativePosition;
	int sectorGroup = 0;

	// Portal aware position
	FVector3 LightRelativeOrigin() const
	{
		if (relativePosition)
		{
			return *relativePosition;
		}
		return LightOrigin();
	}

	// Absolute X, Y, Z position of the light
	FVector3 LightOrigin() const
	{
		float originZ;
		if (!bCeiling)
			originZ = sector->floorplane.ZatPoint(origin.X, origin.Y) + height;
		else
			originZ = sector->ceilingplane.ZatPoint(origin.X, origin.Y) - height;
		return FVector3(origin.X, origin.Y, originZ);
	}

	float LightRadius() const
	{
		return radius + radius; // 2.0 because gzdoom's dynlights do this and we want them to match
	}

	float SpotAttenuation(const FVector3& dir) const
	{
		float spotAttenuation = 1.0f;
		if (outerAngleCos > -1.0f)
		{
			float negPitch = -radians(mapThing->pitch);
			float xyLen = std::cos(negPitch);
			FVector3 spotDir;
			spotDir.X = -std::cos(radians(mapThing->angle)) * xyLen;
			spotDir.Y = -std::sin(radians(mapThing->angle)) * xyLen;
			spotDir.Z = -std::sin(negPitch);
			float cosDir = (dir | spotDir);
			spotAttenuation = smoothstep(outerAngleCos, innerAngleCos, cosDir);
			spotAttenuation = std::max(spotAttenuation, 0.0f);
		}
		return spotAttenuation;
	}

	FVector3 SpotDir() const
	{
		if (outerAngleCos > -1.0f)
		{
			float negPitch = -radians(mapThing->pitch);
			float xyLen = std::cos(negPitch);
			FVector3 spotDir;
			spotDir.X = -std::cos(radians(mapThing->angle)) * xyLen;
			spotDir.Y = -std::sin(radians(mapThing->angle)) * xyLen;
			spotDir.Z = -std::sin(negPitch);
			return spotDir;
		}
		else
		{
			return FVector3(0.0f, 0.0f, 0.0f);
		}
	}

	float DistAttenuation(float distance) const
	{
		return std::max(1.0f - (distance / LightRadius()), 0.0f);
	}
};

enum mapFlags_t
{
	ML_TRANSPARENT1 = 2048, // 25% or 75% transcluency?
	ML_TRANSPARENT2 = 4096,  // 25% or 75% transcluency?

	ML_BLOCKING = 0x00000001,	// solid, is an obstacle
	ML_BLOCKMONSTERS = 0x00000002,	// blocks monsters only
	ML_TWOSIDED = 0x00000004,	// backside will not be present at all if not two sided

	// If a texture is pegged, the texture will have
	// the end exposed to air held constant at the
	// top or bottom of the texture (stairs or pulled
	// down things) and will move with a height change
	// of one of the neighbor sectors.
	// Unpegged textures always have the first row of
	// the texture at the top pixel of the line for both
	// top and bottom textures (use next to windows).

	ML_DONTPEGTOP = 0x00000008,	// upper texture unpegged
	ML_DONTPEGBOTTOM = 0x00000010,	// lower texture unpegged
	ML_SECRET = 0x00000020,	// don't map as two sided: IT'S A SECRET!
	ML_SOUNDBLOCK = 0x00000040,	// don't let sound cross two of these
	ML_DONTDRAW = 0x00000080,	// don't draw on the automap
	ML_MAPPED = 0x00000100,	// set if already drawn in automap
	ML_REPEAT_SPECIAL = 0x00000200,	// special is repeatable

	// 0x400, 0x800 and 0x1000 are ML_SPAC_MASK, they can be used for internal things but not for real map flags.
	ML_ADDTRANS = 0x00000400,	// additive translucency (can only be set internally)
	ML_COMPATSIDE = 0x00000800,	// for compatible PointOnLineSide checks. Using the global compatibility check would be a bit expensive for this check.

	// Extended flags
	ML_NOSKYWALLS = 0x00001000,	// Don't draw sky above or below walls
	ML_MONSTERSCANACTIVATE = 0x00002000,	// [RH] Monsters (as well as players) can activate the line
	ML_BLOCK_PLAYERS = 0x00004000,
	ML_BLOCKEVERYTHING = 0x00008000,	// [RH] Line blocks everything
	ML_ZONEBOUNDARY = 0x00010000,
	ML_RAILING = 0x00020000,
	ML_BLOCK_FLOATERS = 0x00040000,
	ML_CLIP_MIDTEX = 0x00080000,	// Automatic for every Strife line
	ML_WRAP_MIDTEX = 0x00100000,
	ML_3DMIDTEX = 0x00200000,
	ML_CHECKSWITCHRANGE = 0x00400000,
	ML_FIRSTSIDEONLY = 0x00800000,	// activated only when crossed from front side
	ML_BLOCKPROJECTILE = 0x01000000,
	ML_BLOCKUSE = 0x02000000,	// blocks all use actions through this line
	ML_BLOCKSIGHT = 0x04000000,	// blocks monster line of sight
	ML_BLOCKHITSCAN = 0x08000000,	// blocks hitscan attacks
	ML_3DMIDTEX_IMPASS = 0x10000000,	// [TP] if 3D midtex, behaves like a height-restricted ML_BLOCKING
	ML_REVEALED = 0x20000000,	// set if revealed in automap
	ML_DRAWFULLHEIGHT = 0x40000000,	// Draw the full height of the upper/lower sections
	ML_PORTALCONNECT = 0x80000000,	// for internal use only: This line connects to a sector with a linked portal (used to speed up sight checks.)
	// Flag words may not exceed 32 bit due to VM limitations.
	ML2_BLOCKLANDMONSTERS = 0x1,	// MBF21
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

	FVector3 defaultSunColor;
	FVector3 defaultSunDirection;
	int DefaultSamples;

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

	const FVector3 &GetSunColor() const;
	const FVector3 &GetSunDirection() const;
	IntSector* GetFrontSector(const IntSideDef* side) const;
	IntSector* GetBackSector(const IntSideDef* side) const;
	IntSector* GetSectorFromSubSector(const MapSubsectorEx* sub) const;
	MapSubsectorEx *PointInSubSector(const int x, const int y);
	FloatVertex GetSegVertex(unsigned int index) const;

	int FindFirstSectorFromTag(int tag);
	unsigned FindFirstLineId(int lineId);

	inline IntSector* PointInSector(const DVector2& pos) { return GetSectorFromSubSector(PointInSubSector(int(pos.X), int(pos.Y))); }
private:
	void CheckSkySectors();
	void CreateLights();
};

const int BLOCKSIZE = 128;
const int BLOCKFRACSIZE = BLOCKSIZE<<FRACBITS;
const int BLOCKBITS = 7;
const int BLOCKFRACBITS = FRACBITS+7;

inline int IntSector::Index(const FLevel& level) const { return (int)(ptrdiff_t)(this - level.Sectors.Data()); }
inline int IntSideDef::Index(const FLevel& level) const { return (int)(ptrdiff_t)(this - level.Sides.Data()); }
inline int IntLineDef::Index(const FLevel& level) const { return (int)(ptrdiff_t)(this - level.Lines.Data()); }

inline MapSegGLEx* MapSubsectorEx::GetFirstLine(const FLevel& level) const { return firstline != NO_INDEX ? &level.GLSegs[firstline] : nullptr; }
inline IntSector* MapSubsectorEx::GetSector(const FLevel& level) const { return level.GetSectorFromSubSector(this); }

inline FVector2 IntSideDef::V1(const FLevel& level) const { auto v = level.GetSegVertex(sectordef == line->frontsector ? line->v1 : line->v2); return FVector2(v.x, v.y); }
inline FVector2 IntSideDef::V2(const FLevel& level) const { auto v = level.GetSegVertex(sectordef == line->frontsector ? line->v2 : line->v1); return FVector2(v.x, v.y); }
