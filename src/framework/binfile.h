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

#pragma once

#include "math/mathlib.h"
#include <string>

class kexBinFile
{
public:
	uint8_t Read8();
	short Read16();
	int Read32();
	float ReadFloat();
	kexVec3 ReadVector();
	std::string ReadString();

	void Write8(const uint8_t val);
	void Write16(const short val);
	void Write32(const int val);
	void WriteFloat(const float val);
	void WriteVector(const kexVec3 &val);
	void WriteString(const std::string &val);

	int GetOffsetValue(int id);
	uint8_t *GetOffset(int id, uint8_t *subdata = nullptr, int *count = nullptr);

	uint8_t *Buffer() const { return buffer; }
	void SetBuffer(uint8_t *ptr) { buffer = ptr; }
	uint8_t *BufferAt() const { return &buffer[bufferOffset]; }
	void SetOffset(const int offset) { bufferOffset = offset; }

private:
	uint8_t *buffer = nullptr;
	unsigned int bufferOffset = 0;
};
