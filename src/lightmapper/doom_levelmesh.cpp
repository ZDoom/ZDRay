
#include "doom_levelmesh.h"

DoomLevelMesh::DoomLevelMesh(FLevel& level, int samples, int lmdims)
{
}

int DoomLevelMesh::AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize)
{
	return 0;
}

void DoomLevelMesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
}

void DoomLevelMesh::AddLightmapLump(FWadWriter& out)
{
}
