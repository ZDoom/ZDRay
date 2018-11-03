//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION: Binary file operations
//
//-----------------------------------------------------------------------------

#include "lightmap/common.h"
#include "lightmap/kexlib/binfile.h"

byte kexBinFile::Read8()
{
	byte result;
	result = buffer[bufferOffset++];
	return result;
}

short kexBinFile::Read16()
{
	int result;
	result = Read8();
	result |= Read8() << 8;
	return result;
}

int kexBinFile::Read32()
{
	int result;
	result = Read8();
	result |= Read8() << 8;
	result |= Read8() << 16;
	result |= Read8() << 24;
	return result;
}

float kexBinFile::ReadFloat()
{
	fint_t fi;
	fi.i = Read32();
	return fi.f;
}

kexVec3 kexBinFile::ReadVector()
{
	kexVec3 vec;

	vec.x = ReadFloat();
	vec.y = ReadFloat();
	vec.z = ReadFloat();

	return vec;
}

std::string kexBinFile::ReadString()
{
	std::string str;
	char c = 0;

	while (1)
	{
		if (!(c = Read8()))
		{
			break;
		}

		str += c;
	}

	return str;
}

void kexBinFile::Write8(const byte val)
{
	buffer[bufferOffset] = val;
	bufferOffset++;
}

void kexBinFile::Write16(const short val)
{
	Write8(val & 0xff);
	Write8((val >> 8) & 0xff);
}

void kexBinFile::Write32(const int val)
{
	Write8(val & 0xff);
	Write8((val >> 8) & 0xff);
	Write8((val >> 16) & 0xff);
	Write8((val >> 24) & 0xff);
}

void kexBinFile::WriteFloat(const float val)
{
	fint_t fi;
	fi.f = val;
	Write32(fi.i);
}

void kexBinFile::WriteVector(const kexVec3 &val)
{
	WriteFloat(val.x);
	WriteFloat(val.y);
	WriteFloat(val.z);
}

void kexBinFile::WriteString(const std::string &val)
{
	const char *c = val.c_str();

	for (size_t i = 0; i < val.size(); i++)
	{
		Write8(c[i]);
	}

	Write8(0);
}

int kexBinFile::GetOffsetValue(int id)
{
	return *(int*)(buffer + (id << 2));
}

byte *kexBinFile::GetOffset(int id, byte *subdata, int *count)
{
	byte *data = (subdata == nullptr) ? buffer : subdata;

	bufferOffset = GetOffsetValue(id);
	byte *dataOffs = &data[bufferOffset];

	if (count)
	{
		*count = *(int*)(dataOffs);
	}

	return dataOffs;
}
