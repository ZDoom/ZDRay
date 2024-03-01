#pragma once

#include "framework/tarray.h"
#include "framework/templates.h"
#include "framework/zstring.h"
#include <memory>
#include <vector>

struct FileData
{
	char* GetMem() { return (char*)Buffer.data(); }
	std::vector<uint8_t> Buffer;
};

class IFileSystemSource
{
public:
	virtual ~IFileSystemSource() = default;
	virtual int GetLumpCount() = 0;
	virtual int CheckNumForFullName(const FString& fullname) = 0;
	virtual int FileLength(int lump) = 0;
	virtual FileData ReadFile(int lump) = 0;
	virtual const char* GetFileFullName(int lump, bool returnshort) = 0;
};

class FFileSystem
{
public:
	void AddZipSource(const FString& filename);
	void AddFolderSource(const FString& foldername);
	void AddWadSource(const FString& filename);

	int CheckNumForFullName(const FString& fullname);
	int FileLength(int lump);
	FileData ReadFile(int lump);
	const char* GetFileFullName(int lump, bool returnshort = true) const;

private:
	std::vector<std::unique_ptr<IFileSystemSource>> Sources;
};

extern FFileSystem fileSystem;
