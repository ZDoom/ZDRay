
#include "math/mathlib.h"
#include "pngwriter.h"
#include "framework/binfile.h"
#include "framework/templates.h"
#include "framework/halffloat.h"
#include <map>
#include <vector>
#include <algorithm>
#include <zlib.h>
#include <stdexcept>
#include <memory>
#include <cmath>

void PNGWriter::save(const std::string& filename, int width, int height, int bytes_per_pixel, void* pixels)
{
	PNGImage image;
	image.width = width;
	image.height = height;
	image.bytes_per_pixel = bytes_per_pixel;
	image.pixel_ratio = 1.0f;
	image.data = pixels;

	FILE *file = fopen(filename.c_str(), "wb");
	if (file)
	{
		PNGWriter writer;
		writer.file = file;
		writer.image = &image;
		writer.write_magic();
		writer.write_headers();
		writer.write_data();
		writer.write_chunk("IEND", nullptr, 0);
		fclose(file);
	}
}

void PNGWriter::write_magic()
{
	unsigned char png_magic[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	write(png_magic, 8);
}

void PNGWriter::write_headers()
{
	int ppm = (int)std::round(3800 * image->pixel_ratio);
	int ppm_x = ppm;
	int ppm_y = ppm;

	int width = image->width;
	int height = image->height;
	int bit_depth = image->bytes_per_pixel == 8 ? 16 : 8;
	int color_type = 6;
	int compression_method = 0;
	int filter_method = 0;
	int interlace_method = 0;

	unsigned char idhr[13];
	idhr[0] = (width >> 24) & 0xff;
	idhr[1] = (width >> 16) & 0xff;
	idhr[2] = (width >> 8) & 0xff;
	idhr[3] = width & 0xff;
	idhr[4] = (height >> 24) & 0xff;
	idhr[5] = (height >> 16) & 0xff;
	idhr[6] = (height >> 8) & 0xff;
	idhr[7] = height & 0xff;
	idhr[8] = bit_depth;
	idhr[9] = color_type;
	idhr[10] = compression_method;
	idhr[11] = filter_method;
	idhr[12] = interlace_method;

	//unsigned char srgb[1];
	//srgb[0] = 0;

	unsigned char phys[9];
	phys[0] = (ppm_x >> 24) & 0xff;
	phys[1] = (ppm_x >> 16) & 0xff;
	phys[2] = (ppm_x >> 8) & 0xff;
	phys[3] = ppm_x & 0xff;
	phys[4] = (ppm_y >> 24) & 0xff;
	phys[5] = (ppm_y >> 16) & 0xff;
	phys[6] = (ppm_y >> 8) & 0xff;
	phys[7] = ppm_y & 0xff;
	phys[8] = 1; // pixels per meter

	write_chunk("IHDR", idhr, 13);

	if (ppm != 0)
		write_chunk("pHYs", phys, 9);

	//write_chunk("sRGB", srgb, 1);
}

void PNGWriter::write_data()
{
	//int width = image->width;
	int height = image->height;
	int bytes_per_pixel = image->bytes_per_pixel;
	int pitch = image->width * bytes_per_pixel;

	std::vector<unsigned char> scanline_orig;
	std::vector<unsigned char> scanline_filtered;
	scanline_orig.resize((image->width + 1) * bytes_per_pixel);
	scanline_filtered.resize(image->width * bytes_per_pixel + 1);

	auto idat_uncompressed = std::make_shared<DataBuffer>((int)(height * scanline_filtered.size()));

	for (int y = 0; y < height; y++)
	{
		// Grab scanline
		memcpy(scanline_orig.data() + bytes_per_pixel, (uint8_t*)image->data + y * pitch, scanline_orig.size() - bytes_per_pixel);

		// Convert to big endian for 16 bit
		if (bytes_per_pixel == 8)
		{
			for (size_t x = 0; x < scanline_orig.size(); x += 2)
			{
				std::swap(scanline_orig[x], scanline_orig[x + 1]);
			}
		}

		// Filter scanline
		/*
		scanline_filtered[0] = 0; // None filter type
		for (int i = bytes_per_pixel; i < scanline_orig.size(); i++)
		{
			scanline_filtered[i - bytes_per_pixel + 1] = scanline_orig[i];
		}
		*/
		scanline_filtered[0] = 1; // Sub filter type
		for (int i = bytes_per_pixel; i < scanline_orig.size(); i++)
		{
			unsigned char a = scanline_orig[i - bytes_per_pixel];
			unsigned char x = scanline_orig[i];
			scanline_filtered[i - bytes_per_pixel + 1] = x - a;
		}

		// Output scanline
		memcpy((uint8_t*)idat_uncompressed->data + y * scanline_filtered.size(), scanline_filtered.data(), scanline_filtered.size());
	}

	auto idat = std::make_unique<DataBuffer>(idat_uncompressed->size * 125 / 100);
	idat->size = (int)compress(idat.get(), idat_uncompressed.get(), false);

	write_chunk("IDAT", idat->data, (int)idat->size);
}

void PNGWriter::write_chunk(const char name[4], const void *data, int size)
{
	unsigned char size_data[4];
	size_data[0] = (size >> 24) & 0xff;
	size_data[1] = (size >> 16) & 0xff;
	size_data[2] = (size >> 8) & 0xff;
	size_data[3] = size & 0xff;
	write(size_data, 4);

	write(name, 4);

	write(data, size);
	unsigned int crc32 = PNGCRC32::crc(name, data, size);

	unsigned char crc32_data[4];
	crc32_data[0] = (crc32 >> 24) & 0xff;
	crc32_data[1] = (crc32 >> 16) & 0xff;
	crc32_data[2] = (crc32 >> 8) & 0xff;
	crc32_data[3] = crc32 & 0xff;
	write(crc32_data, 4);
}

void PNGWriter::write(const void *data, int size)
{
	fwrite(data, size, 1, file);
}

size_t PNGWriter::compress(DataBuffer *out, const DataBuffer *data, bool raw)
{
	if (data->size > (size_t)0xffffffff || out->size > (size_t)0xffffffff)
		throw std::runtime_error("Data is too big");

	const int window_bits = 15;

	int compression_level = 6;
	int strategy = Z_DEFAULT_STRATEGY;

	z_stream zs;
	memset(&zs, 0, sizeof(z_stream));
	int result = deflateInit2(&zs, compression_level, Z_DEFLATED, raw ? -window_bits : window_bits, 8, strategy); // Undocumented: if wbits is negative, zlib skips header check
	if (result != Z_OK)
		throw std::runtime_error("Zlib deflateInit failed");

	zs.next_in = (unsigned char *)data->data;
	zs.avail_in = (unsigned int)data->size;
	zs.next_out = (unsigned char *)out->data;
	zs.avail_out = (unsigned int)out->size;

	size_t outSize = 0;
	try
	{
		int result = deflate(&zs, Z_FINISH);
		if (result == Z_NEED_DICT) throw std::runtime_error("Zlib deflate wants a dictionary!");
		if (result == Z_DATA_ERROR) throw std::runtime_error("Zip data stream is corrupted");
		if (result == Z_STREAM_ERROR) throw std::runtime_error("Zip stream structure was inconsistent!");
		if (result == Z_MEM_ERROR) throw std::runtime_error("Zlib did not have enough memory to compress file!");
		if (result == Z_BUF_ERROR) throw std::runtime_error("Not enough data in buffer when Z_FINISH was used");
		if (result != Z_STREAM_END) throw std::runtime_error("Zlib deflate failed while compressing zip file!");
		outSize = zs.total_out;
	}
	catch (...)
	{
		deflateEnd(&zs);
		throw;
	}
	deflateEnd(&zs);

	return outSize;
}
