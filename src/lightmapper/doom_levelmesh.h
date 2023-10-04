#pragma once

#include "framework/zstring.h"
#include "hw_levelmesh.h"

struct FLevel;
class FWadWriter;

class DoomLevelMesh : public LevelMesh
{
public:
	DoomLevelMesh(FLevel& level, int samples, int lmdims);

	int AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize) override;
	void DumpMesh(const FString& objFilename, const FString& mtlFilename) const;

	void AddLightmapLump(FWadWriter& out);
};
