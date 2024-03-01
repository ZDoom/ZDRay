#pragma once

#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include "framework/filesystem.h"
#include <map>
#include <memory>

class FGameTexture;

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

class FTextureID
{
	friend class FTextureManager;
	friend void R_InitSpriteDefs();

public:
	FTextureID() = default;
	bool isNull() const { return texnum == 0; }
	bool isValid() const { return texnum > 0; }
	bool Exists() const { return texnum >= 0; }
	void SetInvalid() { texnum = -1; }
	void SetNull() { texnum = 0; }
	bool operator ==(const FTextureID &other) const { return texnum == other.texnum; }
	bool operator !=(const FTextureID &other) const { return texnum != other.texnum; }
	FTextureID operator +(int offset) const noexcept(true);
	int GetIndex() const { return texnum; }	// Use this only if you absolutely need the index!
	void SetIndex(int index) { texnum = index; }	// Use this only if you absolutely need the index!

											// The switch list needs these to sort the switches by texture index
	int operator -(FTextureID other) const { return texnum - other.texnum; }
	bool operator < (FTextureID other) const { return texnum < other.texnum; }
	bool operator > (FTextureID other) const { return texnum > other.texnum; }

protected:
	constexpr FTextureID(int num) : texnum(num) { }
private:
	int texnum;
};

class FNullTextureID : public FTextureID
{
public:
	constexpr FNullTextureID() : FTextureID(0) {}
};

// This is for the script interface which needs to do casts from int to texture.
class FSetTextureID : public FTextureID
{
public:
	constexpr FSetTextureID(int v) : FTextureID(v) {}
};

class FGameTexture
{
public:
	bool isValid() const { return Valid; }
	float GetDisplayWidth() const { return 64.0f; }
	float GetDisplayHeight() const { return 64.0f; }
	float GetScaleY() const { return 1.0f; }

private:
	FString Name;
	bool Valid = true;

	friend class FTextureManager;
};

class FTextureManager
{
public:
	FTextureManager()
	{
		Textures.Push(std::make_unique<FGameTexture>());
		Textures.Last()->Name = "-";
		Textures.Last()->Valid = false;
	}

	FGameTexture* GetGameTexture(FTextureID texnum, bool animate = false)
	{
		if (!texnum.isValid() || texnum.isNull())
			return Textures[0].get();

		return Textures[texnum.GetIndex()].get();
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

	FTextureID CheckForTexture(const char* name, ETextureType usetype, uint32_t flags = TEXMAN_TryAny)
	{
		if (name == nullptr || name[0] == '\0')
			return FTextureID(-1);
		if (name[0] == '-' && name[1] == '\0')
			return FTextureID(0);

		auto it = NameToID.find(name);
		if (it != NameToID.end())
			return FTextureID(it->second);

		int id = Textures.Size();
		Textures.Push(std::make_unique<FGameTexture>());
		Textures.Last()->Name = name;
		NameToID[name] = id;

		return FTextureID(id);
	}

	std::map<FString, int> NameToID;
	TArray<std::unique_ptr<FGameTexture>> Textures;
};

extern FTextureManager TexMan;
