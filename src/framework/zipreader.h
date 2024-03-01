#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

class ZipReader
{
public:
	static std::unique_ptr<ZipReader> open(std::string filename);

	virtual ~ZipReader() = default;

	virtual int get_num_files() = 0;
	virtual std::string get_filename(int file_index) = 0;
	virtual uint64_t get_uncompressed_size(int file_index) = 0;
	virtual uint32_t get_crc32(int file_index) = 0;
	virtual std::vector<uint8_t> read_all_bytes(int file_index) = 0;
	virtual std::string read_all_text(int file_index) = 0;
	virtual int locate_file(const std::string& filename) = 0;

	virtual bool file_exists(const std::string& filename) = 0;
	virtual uint32_t get_crc32(const std::string& filename) = 0;
	virtual std::vector<uint8_t> read_all_bytes(const std::string& filename) = 0;
	virtual std::string read_all_text(const std::string& filename) = 0;
};
