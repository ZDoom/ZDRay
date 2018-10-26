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

#include "lightmap/kexlib/math/mathlib.h"

class kexBinFile
{
public:
    kexBinFile();
    ~kexBinFile();

    bool                Open(const char *file, kexHeapBlock &heapBlock = hb_static);
    bool                Create(const char *file);
    void                Close();
    bool                Exists(const char *file);
    int                 Length();
    void                Duplicate(const char *newFileName);

    byte                Read8();
    short               Read16();
    int                 Read32();
    float               ReadFloat();
    kexVec3             ReadVector();
    kexStr              ReadString();

    void                Write8(const byte val);
    void                Write16(const short val);
    void                Write32(const int val);
    void                WriteFloat(const float val);
    void                WriteVector(const kexVec3 &val);
    void                WriteString(const kexStr &val);

    int                 GetOffsetValue(int id);
    byte                *GetOffset(int id,
                                   byte *subdata = NULL,
                                   int *count = NULL);

    FILE                *Handle() const { return handle; }
    byte                *Buffer() const { return buffer; }
    void                SetBuffer(byte *ptr) { buffer = ptr; }
    byte                *BufferAt() const { return &buffer[bufferOffset]; }
    bool                Opened() const { return bOpened; }
    void                SetOffset(const int offset) { bufferOffset = offset; }

private:
    FILE                *handle;
    byte                *buffer;
    unsigned int        bufferOffset;
    bool                bOpened;
};
