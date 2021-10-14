#pragma once

#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include <stdint.h>
#include <string>
#include <memory>

class FModelRenderer;
class FGameTexture;
class IModelVertexBuffer;
class FModel;
struct FSpriteModelFrame;

struct FileData
{
	char* GetMem() { return nullptr; }
};

class FFileSystem
{
public:
	int CheckNumForFullName(const FString& fullname) { return -1; }
	int FileLength(int lump) { return 0; }
	FileData ReadFile(int lump) { return {}; }
	const char* GetFileFullName(int lump, bool returnshort = true) const { return ""; }
};

extern FFileSystem fileSystem;

class FTextureID
{
public:
	bool isValid() const { return false; }
	int GetIndex() const { return 0; }
};

class FNullTextureID : public FTextureID
{
public:
};

class FGameTexture
{
public:
};

enum class ETextureType : uint8_t
{
	Any,
	Wall,
	Flat,
	Sprite,
	WallPatch,
	Build,		// no longer used but needs to remain for ZScript
	SkinSprite,
	Decal,
	MiscPatch,
	FontChar,
	Override,	// For patches between TX_START/TX_END
	Autopage,	// Automap background - used to enable the use of FAutomapTexture
	SkinGraphic,
	Null,
	FirstDefined,
	Special,
	SWCanvas,
};

class FTextureManager
{
public:
	FGameTexture* GetGameTexture()
	{
		return nullptr;
	}

	FGameTexture* GetGameTexture(FTextureID, bool)
	{
		return nullptr;
	}

	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8,
		TEXMAN_ShortNameOnly = 16,
		TEXMAN_DontCreate = 32,
		TEXMAN_Localize = 64,
		TEXMAN_ForceLookup = 128,
		TEXMAN_NoAlias = 256,
	};

	enum
	{
		HIT_Wall = 1,
		HIT_Flat = 2,
		HIT_Sky = 4,
		HIT_Sprite = 8,

		HIT_Columnmode = HIT_Wall | HIT_Sky | HIT_Sprite
	};

	FTextureID CheckForTexture(const char* name, ETextureType usetype, uint32_t flags = TEXMAN_TryAny) { return {}; }
};

extern FTextureManager TexMan;

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

