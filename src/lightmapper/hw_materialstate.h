
#pragma once

enum
{
	CLAMP_NONE = 0,
	CLAMP_X,
	CLAMP_Y,
	CLAMP_XY,
	CLAMP_XY_NOMIP,
	CLAMP_NOFILTER,
	CLAMP_NOFILTER_X,
	CLAMP_NOFILTER_Y,
	CLAMP_NOFILTER_XY,
	CLAMP_CAMTEX,
	NUMSAMPLERS
};

class FMaterial;

struct FMaterialState
{
	FMaterial* mMaterial = nullptr;
	int mClampMode;
	int mTranslation;
	int mOverrideShader;
	bool mChanged;

	void Reset()
	{
		mMaterial = nullptr;
		mTranslation = 0;
		mClampMode = CLAMP_NONE;
		mOverrideShader = -1;
		mChanged = false;
	}
};
