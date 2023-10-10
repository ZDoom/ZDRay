#pragma once

#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include "framework/vectors.h"
#include "framework/textureid.h"
#include <stdint.h>
#include <string>
#include <memory>

class FModelRenderer;
class IModelVertexBuffer;
class FModel;
struct FSpriteModelFrame;

struct FModelVertex
{
	float x, y, z;	// world position
	float u, v;		// texture coordinates
	unsigned packedNormal;	// normal vector as GL_INT_2_10_10_10_REV.
	float lu, lv;	// lightmap texture coordinates
	float lindex;	// lightmap texture index

	void Set(float xx, float yy, float zz, float uu, float vv)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
		lindex = -1.0f;
	}

	void SetNormal(float nx, float ny, float nz)
	{
		int inx = clamp(int(nx * 512), -512, 511);
		int iny = clamp(int(ny * 512), -512, 511);
		int inz = clamp(int(nz * 512), -512, 511);
		int inw = 0;
		packedNormal = (inw << 30) | ((inz & 1023) << 20) | ((iny & 1023) << 10) | (inx & 1023);
	}
};

#define VMO ((FModelVertex*)nullptr)

class IModelVertexBuffer
{
public:
	virtual ~IModelVertexBuffer() { }

	virtual FModelVertex* LockVertexBuffer(unsigned int size) = 0;
	virtual void UnlockVertexBuffer() = 0;

	virtual unsigned int* LockIndexBuffer(unsigned int size) = 0;
	virtual void UnlockIndexBuffer() = 0;
};

FTextureID LoadSkin(const char* path, const char* fn);

#define MD3_MAX_SURFACES	32
#define MIN_MODELS	4

struct FSpriteModelFrame
{
	uint8_t modelsAmount = 0;
	TArray<int> modelIDs;
	TArray<FTextureID> skinIDs;
	TArray<FTextureID> surfaceskinIDs;
	TArray<int> modelframes;
	float xscale, yscale, zscale;
	// [BB] Added zoffset, rotation parameters and flags.
	// Added xoffset, yoffset
	float xoffset, yoffset, zoffset;
	float xrotate, yrotate, zrotate;
	float rotationCenterX, rotationCenterY, rotationCenterZ;
	float rotationSpeed;
	unsigned int flags;
	const void* type;	// used for hashing, must point to something usable as identifier for the model's owner.
	short sprite;
	short frame;
	int hashnext;
	float angleoffset;
	// added pithoffset, rolloffset.
	float pitchoffset, rolloffset; // I don't want to bother with type transformations, so I made this variables float.
	bool isVoxel;
};

enum ModelRendererType
{
	GLModelRendererType,
	SWModelRendererType,
	PolyModelRendererType,
	NumModelRendererTypes
};

class FModel
{
public:
	FModel();
	virtual ~FModel();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) = 0;
	virtual int FindFrame(const char * name) = 0;
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation=0) = 0;
	virtual void BuildVertexBuffer(FModelRenderer *renderer) = 0;
	virtual void AddSkins(uint8_t *hitlist) = 0;
	virtual float getAspectFactor(float vscale) { return 1.f; }

	void SetVertexBuffer(int type, IModelVertexBuffer *buffer) { mVBuf[type] = buffer; }
	IModelVertexBuffer *GetVertexBuffer(int type) const { return mVBuf[type]; }
	void DestroyVertexBuffer();

	const FSpriteModelFrame *curSpriteMDLFrame;
	int curMDLIndex;
	void PushSpriteMDLFrame(const FSpriteModelFrame *smf, int index) { curSpriteMDLFrame = smf; curMDLIndex = index; };

	FString mFileName;

private:
	IModelVertexBuffer *mVBuf[NumModelRendererTypes];
};

//int ModelFrameHash(FSpriteModelFrame* smf);
std::unique_ptr<FModel> LoadModel(const char* path, const char* modelfile);

