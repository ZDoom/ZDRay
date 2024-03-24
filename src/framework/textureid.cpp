
#include "textureid.h"
#include "filesystem.h"
#include "picopng/picopng.h"

FTextureManager TexMan;

FGameTexture::FGameTexture(FString name) : Name(name)
{
	// To do: improve this to support subfolders?
	// To do: figure out what type of data we got here instead of assuming it is a png.

	int lump = fileSystem.CheckNumForFullName(name);
	if (lump < 0)
		lump = fileSystem.CheckNumForFullName("textures/" + name + ".png");
	if (lump < 0)
		lump = fileSystem.CheckNumForFullName("flats/" + name + ".png");

	if (lump < 0)
	{
		// Not found - should we mark it as invalid or use a dummy texture?
		DisplayWidth = 64.0f;
		DisplayHeight = 64.0f;
	}
	else
	{
		FileData filedata = fileSystem.ReadFile(lump);
		int result = decodePNG(Pixels, Width, Height, (const unsigned char*)filedata.GetMem(), fileSystem.FileLength(lump), true);
		if (result == 0)
		{
			DisplayWidth = (float)Width;
			DisplayHeight = (float)Height;
		}
		else
		{
			// Not a png.
			DisplayWidth = 64.0f;
			DisplayHeight = 64.0f;
		}
	}
}
