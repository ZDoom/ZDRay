
#pragma once

#include <string>

class PNGWriter
{
public:
	static void save(const std::string& filename, int width, int height, int bytes_per_pixel, void* pixels);

	struct DataBuffer
	{
		DataBuffer(int size) : size(size) { data = new uint8_t[size]; }
		~DataBuffer() { delete[] data; }
		int size;
		uint8_t* data;
	};

private:
	struct PNGImage
	{
		int width;
		int height;
		int bytes_per_pixel;
		void* data;
		float pixel_ratio;
	};

	const PNGImage* image;
	FILE* file;

	class PNGCRC32
	{
	public:
		static unsigned long crc(const char name[4], const void* data, int len)
		{
			static PNGCRC32 impl;

			const unsigned char* buf = reinterpret_cast<const unsigned char*>(data);

			unsigned int c = 0xffffffff;

			for (int n = 0; n < 4; n++)
				c = impl.crc_table[(c ^ name[n]) & 0xff] ^ (c >> 8);

			for (int n = 0; n < len; n++)
				c = impl.crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

			return c ^ 0xffffffff;
		}

	private:
		unsigned int crc_table[256];

		PNGCRC32()
		{
			for (unsigned int n = 0; n < 256; n++)
			{
				unsigned int c = n;
				for (unsigned int k = 0; k < 8; k++)
				{
					if ((c & 1) == 1)
						c = 0xedb88320 ^ (c >> 1);
					else
						c = c >> 1;
				}
				crc_table[n] = c;
			}
		}
	};

	void write_magic();
	void write_headers();
	void write_data();
	void write_chunk(const char name[4], const void* data, int size);
	void write(const void* data, int size);
	size_t compressdata(DataBuffer* out, const DataBuffer* data, bool raw);
};
