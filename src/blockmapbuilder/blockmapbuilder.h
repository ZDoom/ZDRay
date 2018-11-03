
#pragma once

#include "level/doomdata.h"
#include "level/workdata.h"
#include "framework/tarray.h"

class FBlockmapBuilder
{
public:
	FBlockmapBuilder (FLevel &level);
	uint16_t *GetBlockmap (int &size);

private:
	FLevel &Level;
	TArray<uint16_t> BlockMap;

	void BuildBlockmap ();
	void CreateUnpackedBlockmap (TArray<uint16_t> *blocks, int bmapwidth, int bmapheight);
	void CreatePackedBlockmap (TArray<uint16_t> *blocks, int bmapwidth, int bmapheight);
};
