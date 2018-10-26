//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//

#pragma once

#include "wad.h"
#include "surfaces.h"
#include "lightSurface.h"

#define NO_SIDE_INDEX           -1
#define NO_LINE_INDEX           0xFFFF
#define NF_SUBSECTOR            0x8000

enum
{
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT
};

enum mapFlags_t
{
    ML_BLOCKING             = 1,    // Solid, is an obstacle.
    ML_BLOCKMONSTERS        = 2,    // Blocks monsters only.
    ML_TWOSIDED             = 4,    // Backside will not be present at all if not two sided.
    ML_TRANSPARENT1         = 2048, // 25% or 75% transcluency?
    ML_TRANSPARENT2         = 4096  // 25% or 75% transcluency?
};

struct mapVertex_t
{
    short           x;
    short           y;
};

struct mapSideDef_t
{
    short           textureoffset;
    short           rowoffset;
    char            toptexture[8];
    char            bottomtexture[8];
    char            midtexture[8];
    short           sector;
};

struct mapLineDef_t
{
    short           v1;
    short           v2;
    short           flags;
    short           special;
    short           tag;
    short           sidenum[2]; // sidenum[1] will be -1 if one sided
};

struct mapSector_t
{
    short           floorheight;
    short           ceilingheight;
    char            floorpic[8];
    char            ceilingpic[8];
    short           lightlevel;
    short           special;
    short           tag;
};

struct mapNode_t
{
    // Partition line from (x,y) to x+dx,y+dy)
    short           x;
    short           y;
    short           dx;
    short           dy;

    // Bounding box for each child,
    // clip against view frustum.
    short           bbox[2][4];

    // If NF_SUBSECTOR its a subsector,
    // else it's a node of another subtree.
    word            children[2];
};

struct mapSeg_t
{
    word            v1;
    word            v2;
    short           angle;
    word            linedef;
    short           side;
    short           offset;
};

struct mapSubSector_t
{
    word            numsegs;
    word            firstseg;
};

struct mapThing_t
{
    short           x;
    short           y;
    short           angle;
    short           type;
    short           options;
};

struct glVert_t
{
    int             x;
    int             y;
};

struct glSeg_t
{
    word            v1;
    word            v2;
    word            linedef;
    int16_t         side;
    word            partner;
};

struct vertex_t
{
    float           x;
    float           y;
};

struct leaf_t
{
    vertex_t        *vertex;
    glSeg_t         *seg;
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
    mapThing_t      *mapThing;
    kexVec2         origin;
    kexVec3         rgb;
    float           intensity;
    float           falloff;
    float           height;
    float           radius;
    bool            bCeiling;
    mapSector_t     *sector;
    mapSubSector_t  *ssect;
};

class kexDoomMap
{
public:
    kexDoomMap();
    ~kexDoomMap();

    void                        BuildMapFromWad(kexWadFile &wadFile);
    mapSideDef_t                *GetSideDef(const glSeg_t *seg);
    mapSector_t                 *GetFrontSector(const glSeg_t *seg);
    mapSector_t                 *GetBackSector(const glSeg_t *seg);
    mapSector_t                 *GetSectorFromSubSector(const mapSubSector_t *sub);
    mapSubSector_t              *PointInSubSector(const int x, const int y);
    bool                        PointInsideSubSector(const float x, const float y, const mapSubSector_t *sub);
    bool                        LineIntersectSubSector(const kexVec3 &start, const kexVec3 &end, const mapSubSector_t *sub, kexVec2 &out);
    vertex_t                    *GetSegVertex(int index);
    bool                        CheckPVS(mapSubSector_t *s1, mapSubSector_t *s2);

    void                        ParseConfigFile(const char *file);
    void                        CreateLights();
    void                        CleanupThingLights();

    const kexVec3               &GetSunColor() const;
    const kexVec3               &GetSunDirection() const;

    mapThing_t                  *mapThings;
    mapLineDef_t                *mapLines;
    mapVertex_t                 *mapVerts;
    mapSideDef_t                *mapSides;
    mapSector_t                 *mapSectors;
    glSeg_t                     *mapSegs;
    mapSubSector_t              *mapSSects;
    mapNode_t                   *nodes;
    leaf_t                      *leafs;
    vertex_t                    *vertexes;
    byte                        *mapPVS;

    bool                        *bSkySectors;
    bool                        *bSSectsVisibleToSky;

    int                         numThings;
    int                         numLines;
    int                         numVerts;
    int                         numSides;
    int                         numSectors;
    int                         numSegs;
    int                         numSSects;
    int                         numNodes;
    int                         numLeafs;
    int                         numVertexes;

    int                         *segLeafLookup;
    int                         *ssLeafLookup;
    int                         *ssLeafCount;
    kexBBox                     *ssLeafBounds;

    kexBBox                     *nodeBounds;

    surface_t                   **segSurfaces[3];
    surface_t                   **leafSurfaces[2];

    kexArray<thingLight_t*>     thingLights;
    kexArray<kexLightSurface*>  lightSurfaces;

private:
    void                        BuildLeafs();
    void                        BuildNodeBounds();
    void                        CheckSkySectors();
    void                        BuildVertexes(kexWadFile &wadFile);
    void                        BuildPVS();

    kexArray<lightDef_t>        lightDefs;
    kexArray<surfaceLightDef> surfaceLightDefs;
    kexArray<mapDef_t>          mapDefs;

    mapDef_t                    *mapDef;

    static const kexVec3        defaultSunColor;
    static const kexVec3        defaultSunDirection;
};
