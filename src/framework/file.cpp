
#include "file.h"
#include "utf16.h"
#include <stdexcept>

#ifndef WIN32
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#endif

#ifdef WIN32

#define NOMINMAX
#include <Windows.h>

class FileImpl : public File
{
public:
	FileImpl(HANDLE handle) : handle(handle)
	{
	}

	~FileImpl()
	{
		CloseHandle(handle);
	}

	void write(const void* data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t writesize = std::min(size, (size_t)0xffffffff);
			BOOL result = WriteFile(handle, (const uint8_t*)data + pos, (DWORD)writesize, nullptr, nullptr);
			if (result == FALSE)
				throw std::runtime_error("WriteFile failed");
			pos += writesize;
		}
	}

	void read(void* data, size_t size) override
	{
		size_t pos = 0;
		while (pos < size)
		{
			size_t readsize = std::min(size, (size_t)0xffffffff);
			DWORD bytesRead = 0;
			BOOL result = ReadFile(handle, (uint8_t*)data + pos, (DWORD)readsize, &bytesRead, nullptr);
			if (result == FALSE || bytesRead != readsize)
				throw std::runtime_error("ReadFile failed");
			pos += readsize;
		}
	}

	int64_t size() override
	{
		LARGE_INTEGER fileSize;
		BOOL result = GetFileSizeEx(handle, &fileSize);
		if (result == FALSE)
			throw std::runtime_error("GetFileSizeEx failed");
		return fileSize.QuadPart;
	}

	void seek(int64_t offset, SeekPoint origin) override
	{
		LARGE_INTEGER off, newoff;
		off.QuadPart = offset;
		DWORD moveMethod = FILE_BEGIN;
		if (origin == SeekPoint::current) moveMethod = FILE_CURRENT;
		else if (origin == SeekPoint::end) moveMethod = FILE_END;
		BOOL result = SetFilePointerEx(handle, off, &newoff, moveMethod);
		if (result == FALSE)
			throw std::runtime_error("SetFilePointerEx failed");
	}

	uint64_t tell() override
	{
		LARGE_INTEGER offset, delta;
		delta.QuadPart = 0;
		BOOL result = SetFilePointerEx(handle, delta, &offset, FILE_CURRENT);
		if (result == FALSE)
			throw std::runtime_error("SetFilePointerEx failed");
		return offset.QuadPart;
	}

	HANDLE handle = INVALID_HANDLE_VALUE;
};

std::shared_ptr<File> File::create_always(const std::string& filename)
{
	HANDLE handle = CreateFileW(to_utf16(filename).c_str(), FILE_WRITE_ACCESS, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not create " + filename);

	return std::make_shared<FileImpl>(handle);
}

std::shared_ptr<File> File::open_existing(const std::string& filename)
{
	HANDLE handle = CreateFileW(to_utf16(filename).c_str(), FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not open " + filename);

	return std::make_shared<FileImpl>(handle);
}

#else

#include <stdio.h>

class FileImpl : public File
{
public:
	FileImpl(FILE* handle) : handle(handle)
	{
	}

	~FileImpl()
	{
		fclose(handle);
	}

	void write(const void* data, size_t size) override
	{
		size_t result = fwrite(data, 1, size, handle);
		if (result != size)
			throw std::runtime_error("File write failed");
	}

	void read(void* data, size_t size) override
	{
		size_t result = fread(data, 1, size, handle);
		if (result != size)
			throw std::runtime_error("File read failed");
	}

	int64_t size() override
	{
		auto pos = ftell(handle);
		auto result = fseek(handle, 0, SEEK_END);
		if (result == -1)
			throw std::runtime_error("File seek failed");
		auto length = ftell(handle);
		fseek(handle, pos, SEEK_SET);
		if (length == -1)
			throw std::runtime_error("File tell failed");
		return length;
	}

	void seek(int64_t offset, SeekPoint origin) override
	{
		if (origin == SeekPoint::current)
		{
			auto result = fseek(handle, offset, SEEK_CUR);
			if (result == -1)
				throw std::runtime_error("File seek failed");
		}
		else if (origin == SeekPoint::end)
		{
			auto result = fseek(handle, offset, SEEK_END);
			if (result == -1)
				throw std::runtime_error("File seek failed");
		}
		else
		{
			auto result = fseek(handle, offset, SEEK_SET);
			if (result == -1)
				throw std::runtime_error("File seek failed");
		}
	}

	uint64_t tell() override
	{
		auto result = ftell(handle);
		if (result == -1)
			throw std::runtime_error("File tell failed");
		return result;
	}

	FILE* handle = nullptr;
};

std::shared_ptr<File> File::create_always(const std::string& filename)
{
	FILE* handle = fopen(filename.c_str(), "wb");
	if (!handle)
		throw std::runtime_error("Could not create " + filename);

	return std::make_shared<FileImpl>(handle);
}

std::shared_ptr<File> File::open_existing(const std::string& filename)
{
	FILE* handle = fopen(filename.c_str(), "rb");
	if (!handle)
		throw std::runtime_error("Could not open " + filename);

	return std::make_shared<FileImpl>(handle);
}

#endif

void File::write_all_bytes(const std::string& filename, const void* data, size_t size)
{
	auto file = create_always(filename);
	file->write(data, size);
}

void File::write_all_text(const std::string& filename, const std::string& text)
{
	auto file = create_always(filename);
	file->write(text.data(), text.size());
}

std::vector<uint8_t> File::read_all_bytes(const std::string& filename)
{
	auto file = open_existing(filename);
	std::vector<uint8_t> buffer(file->size());
	file->read(buffer.data(), buffer.size());
	return buffer;
}

std::string File::read_all_text(const std::string& filename)
{
	auto file = open_existing(filename);
	auto size = file->size();
	if (size == 0) return {};
	std::string buffer;
	buffer.resize(size);
	file->read(&buffer[0], buffer.size());
	return buffer;
}

int64_t File::get_last_write_time(const std::string& filename)
{
#ifdef WIN32
	HANDLE handle = CreateFileW(to_utf16(filename).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not open " + filename);

	FILETIME filetime = {};
	BOOL result = GetFileTime(handle, nullptr, nullptr, &filetime);
	CloseHandle(handle);

	if (result == FALSE)
		throw std::runtime_error("GetFileTime failed for " + filename);

	LARGE_INTEGER li;
	li.LowPart = filetime.dwLowDateTime;
	li.HighPart = filetime.dwHighDateTime;
	return li.QuadPart;
#else
	throw std::runtime_error("File::get_last_write_time not implemented");
#endif
}

bool File::try_remove(const std::string& filename)
{
#ifdef WIN32
	return DeleteFileW(to_utf16(filename).c_str()) == TRUE;
#else
	throw std::runtime_error("File::try_remove not implemented");
#endif
}

/////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Directory::files(const std::string& filename)
{
#ifdef WIN32
	std::vector<std::string> files;

	WIN32_FIND_DATAW fileinfo;
	HANDLE handle = FindFirstFileW(to_utf16(filename).c_str(), &fileinfo);
	if (handle == INVALID_HANDLE_VALUE)
		return {};

	try
	{
		do
		{
			bool is_directory = !!(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			if (!is_directory)
				files.push_back(from_utf16(fileinfo.cFileName));
		} while (FindNextFileW(handle, &fileinfo) == TRUE);
		FindClose(handle);
	}
	catch (...)
	{
		FindClose(handle);
		throw;
	}

	return files;
#else
	throw std::runtime_error("Directory::files not implemented");
#endif
}

std::vector<std::string> Directory::folders(const std::string& filename)
{
#ifdef WIN32
	std::vector<std::string> files;

	WIN32_FIND_DATAW fileinfo;
	HANDLE handle = FindFirstFileW(to_utf16(filename).c_str(), &fileinfo);
	if (handle == INVALID_HANDLE_VALUE)
		return {};

	try
	{
		do
		{
			bool is_directory = !!(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			if (is_directory)
			{
				files.push_back(from_utf16(fileinfo.cFileName));
				if (files.back() == "." || files.back() == "..")
					files.pop_back();
			}
		} while (FindNextFileW(handle, &fileinfo) == TRUE);
		FindClose(handle);
	}
	catch (...)
	{
		FindClose(handle);
		throw;
	}

	return files;
#else
	throw std::runtime_error("Directory::folders not implemented");
#endif
}

void Directory::create(const std::string& path)
{
#ifdef WIN32
	BOOL result = CreateDirectoryW(to_utf16(path).c_str(), nullptr);
	if (result == FALSE)
	{
		DWORD error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS)
		{
			return;
		}
		else if (error == ERROR_PATH_NOT_FOUND)
		{
			try
			{
				std::string parent = FilePath::remove_last_component(path);
				if (!parent.empty())
				{
					Directory::create(parent);
					if (CreateDirectoryW(to_utf16(path).c_str(), nullptr) == TRUE)
						return;
				}
			}
			catch (...)
			{
			}
		}
		throw std::runtime_error("Could not create directory for path " + path);
	}
#else
	throw std::runtime_error("Directory::create not implemented");
#endif
}

/////////////////////////////////////////////////////////////////////////////

bool FilePath::has_extension(const std::string &filename, const char *checkext)
{
	auto fileext = extension(filename);
#ifdef WIN32
	return _stricmp(fileext.c_str(), checkext) == 0;
#else
	return strcasecmp(fileext.c_str(), checkext) == 0;
#endif
}

std::string FilePath::extension(const std::string &filename)
{
	std::string file = last_component(filename);
	std::string::size_type pos = file.find_last_of('.');
	if (pos == std::string::npos)
		return std::string();

#ifndef WIN32
	// Files beginning with a dot is not a filename extension in Unix.
	// This is different from Windows where it is considered the extension.
	if (pos == 0)
		return std::string();
#endif

	return file.substr(pos + 1);

}

std::string FilePath::remove_extension(const std::string &filename)
{
	std::string file = last_component(filename);
	std::string::size_type pos = file.find_last_of('.');
	if (pos == std::string::npos)
		return filename;
	else
		return filename.substr(0, filename.length() - file.length() + pos);
}

std::string FilePath::last_component(const std::string &path)
{
#ifdef WIN32
	auto last_slash = path.find_last_of("/\\");
	if (last_slash != std::string::npos)
		return path.substr(last_slash + 1);
	else
		return path;
#else
	auto last_slash = path.find_last_of('/');
	if (last_slash != std::string::npos)
		return path.substr(last_slash + 1);
	else
		return path;
#endif
}

std::string FilePath::remove_last_component(const std::string &path)
{
#ifdef WIN32
	auto last_slash = path.find_last_of("/\\");
	if (last_slash != std::string::npos)
		return path.substr(0, last_slash);
	else
		return std::string();
#else
	auto last_slash = path.find_last_of('/');
	if (last_slash != std::string::npos)
		return path.substr(0, last_slash + 1);
	else
		return std::string();
#endif
}

std::string FilePath::combine(const std::string &path1, const std::string &path2)
{
#ifdef WIN32
	if (path1.empty())
		return path2;
	else if (path2.empty())
		return path1;
	else if (path2.front() == '/' || path2.front() == '\\')
		return path2;
	else if (path1.back() != '/' && path1.back() != '\\')
		return path1 + "\\" + path2;
	else
		return path1 + path2;
#else
	if (path1.empty())
		return path2;
	else if (path2.empty())
		return path1;
	else if (path2.front() == '/')
		return path2;
	else if (path1.back() != '/')
		return path1 + "/" + path2;
	else
		return path1 + path2;
#endif
}

std::string FilePath::force_filesys_slash(std::string path)
{
#ifdef WIN32
	return force_backslash(std::move(path));
#else
	return force_slash(std::move(path));
#endif
}

std::string FilePath::force_slash(std::string path)
{
	for (char& c : path)
	{
		if (c == '\\') c = '/';
	}
	return path;
}

std::string FilePath::force_backslash(std::string path)
{
	for (char& c : path)
	{
		if (c == '/') c = '\\';
	}
	return path;
}
