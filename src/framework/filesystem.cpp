
#include "filesystem.h"
#include "zipreader.h"
#include "file.h"
#include <map>
#include <stdexcept>

FFileSystem fileSystem;

/////////////////////////////////////////////////////////////////////////////

class ZipFileSystemSource : public IFileSystemSource
{
public:
	ZipFileSystemSource(const FString& filename)
	{
		zip = ZipReader::open(filename.GetChars());
	}

	int GetLumpCount() override
	{
		return zip->get_num_files();
	}
	
	int CheckNumForFullName(const FString& fullname) override
	{
		return zip->locate_file(fullname.GetChars());
	}

	int FileLength(int lump) override
	{
		uint64_t size = zip->get_uncompressed_size(lump);
		// For safety. The filesystem API clearly wasn't designed to handle large files.
		if (size >= 0x7fffffff) throw std::runtime_error("File is too big");
		return (int)size;
	}

	FileData ReadFile(int lump) override
	{
		FileData data;
		data.Buffer = zip->read_all_bytes(lump);
		return data;
	}

	const char* GetFileFullName(int lump, bool returnshort) override
	{
		static std::string tempstring;
		tempstring = zip->get_filename(lump);
		return tempstring.c_str();
	}

	std::unique_ptr<ZipReader> zip;
};

/////////////////////////////////////////////////////////////////////////////

class FolderFileSystemSource : public IFileSystemSource
{
public:
	FolderFileSystemSource(const FString& foldername)
	{
		ScanFolder(foldername.GetChars());
	}

	int GetLumpCount() override
	{
		return (int)Filenames.size();
	}

	int CheckNumForFullName(const FString& fullname) override
	{
		auto it = FilenameKeyToIndex.find(fullname.GetChars());
		if (it == FilenameKeyToIndex.end())
			return -1;
		return (int)it->second;
	}

	int FileLength(int lump) override
	{
		int64_t size = File::open_existing(Filenames[lump])->size();
		// For safety. The filesystem API clearly wasn't designed to handle large files.
		if (size >= 0x7fffffff) throw std::runtime_error("File is too big");
		return (int)size;
	}

	FileData ReadFile(int lump) override
	{
		FileData data;
		data.Buffer = File::read_all_bytes(Filenames[lump]);
		return data;
	}

	const char* GetFileFullName(int lump, bool returnshort) override
	{
		return Filenames[lump].c_str();
	}

	void ScanFolder(const std::string& foldername, int depth = 0)
	{
		for (const std::string& filename : Directory::files(foldername))
		{
			std::string fullname = FilePath::combine(foldername, filename);
			FilenameKeyToIndex[fullname] = Filenames.size();
			Filenames.push_back(fullname);
		}

		if (depth < 16)
		{
			for (const std::string& subfolder : Directory::folders(foldername))
			{
				ScanFolder(FilePath::combine(foldername, subfolder), depth + 1);
			}
		}
	}

	std::vector<std::string> Filenames;
	std::map<std::string, size_t> FilenameKeyToIndex;
};

/////////////////////////////////////////////////////////////////////////////

class WadFileEntry
{
public:
	uint32_t offset = 0;
	uint32_t size = 0;
	std::string name;
	void *data = nullptr;
};

class WadFileSystemSource : public IFileSystemSource
{
public:
	WadFileSystemSource(const FString& filename)
	{
		file = File::open_existing(filename.GetChars());

		char magic[4];
		file->read(magic, 4);
		pwad = memcmp(magic, "PWAD", 4) == 0;
		iwad = memcmp(magic, "IWAD", 4) == 0;
		if (!pwad && !iwad)
			throw std::runtime_error("Not a valid WAD file");

		uint32_t num_entries = file->read_uint32();
		uint32_t directory_offset = file->read_uint32();
		file->seek(directory_offset);
		for (uint32_t i = 0; i < num_entries; i++)
		{
			WadFileEntry entry;
			entry.offset = file->read_uint32();
			entry.size = file->read_uint32();

			char name[9];
			name[8] = 0;
			file->read(name, 8);
			entry.name = name;

			FilenameKeyToIndex[entry.name] = entries.size();
			entries.push_back(entry);
		}
	}

	int GetLumpCount() override
	{
		return (int)entries.size();
	}

	int CheckNumForFullName(const FString& fullname) override
	{
		auto it = FilenameKeyToIndex.find(fullname.GetChars());
		if (it == FilenameKeyToIndex.end())
			return -1;
		return (int)it->second;
	}

	int FileLength(int lump) override
	{
		return entries[lump].size;
	}

	FileData ReadFile(int lump) override
	{
		FileData data;
		data.Buffer.resize(entries[lump].size);
		file->seek(entries[lump].offset);
		file->read(data.Buffer.data(), data.Buffer.size());
		return data;
	}

	const char* GetFileFullName(int lump, bool returnshort) override
	{
		return entries[lump].name.c_str();
	}

	std::shared_ptr<File> file;
	bool pwad = false;
	bool iwad = false;
	std::vector<WadFileEntry> entries;
	std::map<std::string, size_t> FilenameKeyToIndex;
};

/////////////////////////////////////////////////////////////////////////////

void FFileSystem::AddZipSource(const FString& filename)
{
	Sources.push_back(std::make_unique<ZipFileSystemSource>(filename));
}

void FFileSystem::AddFolderSource(const FString& foldername)
{
	Sources.push_back(std::make_unique<FolderFileSystemSource>(foldername));
}

void FFileSystem::AddWadSource(const FString& filename)
{
	Sources.push_back(std::make_unique<WadFileSystemSource>(filename));
}

int FFileSystem::CheckNumForFullName(const FString& fullname)
{
	int pos = 0;
	for (auto& source : Sources)
	{
		int index = source->CheckNumForFullName(fullname);
		if (index != -1)
			return pos + index;
		pos += source->GetLumpCount();
	}
	return -1;
}

int FFileSystem::FileLength(int lump)
{
	int pos = 0;
	for (auto& source : Sources)
	{
		if (lump - pos < source->GetLumpCount())
		{
			return source->FileLength(lump - pos);
		}
		pos += source->GetLumpCount();
	}
	return 0;
}

FileData FFileSystem::ReadFile(int lump)
{
	int pos = 0;
	for (auto& source : Sources)
	{
		if (lump - pos < source->GetLumpCount())
		{
			return source->ReadFile(lump - pos);
		}
		pos += source->GetLumpCount();
	}
	return {};
}

const char* FFileSystem::GetFileFullName(int lump, bool returnshort) const
{
	int pos = 0;
	for (auto& source : Sources)
	{
		if (lump - pos < source->GetLumpCount())
		{
			return source->GetFileFullName(lump - pos, returnshort);
		}
		pos += source->GetLumpCount();
	}
	return {};
}
