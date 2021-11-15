
#include "lightmaptexture.h"
#include <algorithm>

LightmapTexture::LightmapTexture(int width, int height) : textureWidth(width), textureHeight(height)
{
#ifdef _DEBUG
	mPixels.resize(width * height * 3, floatToHalf(0.5f));
#else
	mPixels.resize(width * height * 3, 0);
#endif
	allocBlocks.resize(width);
}

bool LightmapTexture::MakeRoomForBlock(const int width, const int height, int* x, int* y)
{
	int startY = 0;
	int startX = 0;
	for (int i = 0; i < textureHeight; i++)
	{
		startX = std::max(startX, allocBlocks[i]);
		int available = textureWidth - startX;
		if (available < width)
		{
			startY = i + 1;
			startX = 0;
		}
		else if (i - startY + 1 == height)
		{
			for (int yy = 0; yy < height; yy++)
			{
				allocBlocks[startY + yy] = startX + width;
			}
			*x = startX;
			*y = startY;
			return true;
		}
	}
	return false;
}
