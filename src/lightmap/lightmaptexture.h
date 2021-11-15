
#pragma once

#include <vector>

class LightmapTexture
{
public:
	LightmapTexture(int width, int height);

	bool MakeRoomForBlock(const int width, const int height, int* x, int* y);

	int Width() const { return textureWidth; }
	int Height() const { return textureHeight; }
	uint16_t* Pixels() { return mPixels.data(); }

private:
	int textureWidth;
	int textureHeight;
	std::vector<uint16_t> mPixels;
	std::vector<int> allocBlocks;
};
