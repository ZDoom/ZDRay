
#include "lightmaptexture.h"
#include "framework/halffloat.h"
#include <algorithm>

LightmapTexture::LightmapTexture(int width, int height) : textureWidth(width), textureHeight(height)
{
#ifdef _DEBUG
	mPixels.resize(width * height * 3, floatToHalf(0.5f));
#else
	mPixels.resize(width * height * 3, 0);
#endif
}
