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

#include <assert.h>

template<class type>
class kexArray
{
public:
    kexArray();
    ~kexArray();

    typedef int         compare_t(const type*, const type*);

    void                Push(type o);
    void                Pop();
    void                Empty();
    void                Init();
    void                Resize(unsigned int size);
    type                IndexOf(unsigned int index) const;
    bool                Contains(const type check) const;
    void                Splice(const unsigned int start, unsigned int len);
    void                Sort(compare_t *function);
    void                Sort(compare_t *function, unsigned int count);

    const unsigned int  Length() const { return length; }
    type                GetData(const int index) { return data[index]; }

    type                &operator[](unsigned int index);
    kexArray<type>      &operator=(const kexArray<type> &arr);

protected:
    type                *data;
    unsigned int        length;
    unsigned int        aidx;
};

//
// kexArray::kexArray
//
template<class type>
kexArray<type>::kexArray()
{
    Init();
}

//
// kexArray::~kexArray
//
template<class type>
kexArray<type>::~kexArray()
{
    Empty();
}

//
// kexArray::Init
//
template<class type>
void kexArray<type>::Init()
{
    data = NULL;
    length = 0;
    aidx = 0;
}

//
// kexArray::Resize
//
template<class type>
void kexArray<type>::Resize(unsigned int size)
{
    type *tmp;

    if(size == length)
    {
        return;
    }

    if(size <= 0 && length != 0)
    {
        delete[] data;
        data = NULL;
        length = 0;
        return;
    }

    if(length == 0)
    {
        data = new type[size];
        length = size;
        return;
    }

    tmp = data;
    data = new type[size];

    for(unsigned int i = 0; i < length; i++)
    {
        data[i] = tmp[i];
    }

    length = size;
    delete[] tmp;
}

//
// kexArray::Push
//
template<class type>
void kexArray<type>::Push(type o)
{
    Resize(length+1);
    data[aidx++] = o;
}

//
// kexArray::Pop
//
template<class type>
void kexArray<type>::Pop()
{
    if(length == 0)
    {
        return;
    }

    Resize(length-1);
    aidx--;
}

//
// kexArray::Empty
//
template<class type>
void kexArray<type>::Empty()
{
    if(data && length > 0)
    {
        delete[] data;
        data = NULL;
        length = 0;
        aidx = 0;
    }
}

//
// kexArray::IndexOf
//
template<class type>
type kexArray<type>::IndexOf(unsigned int index) const
{
    if(index >= length)
    {
        index = length-1;
    }

    return data[index];
}

//
// kexArray::Contains
//
template<class type>
bool kexArray<type>::Contains(const type check) const
{
    for(unsigned int i = 0; i < length; ++i)
    {
        if(data[i] == check)
        {
            return true;
        }
    }

    return false;
}

//
// kexArray::Splice
//
template<class type>
void kexArray<type>::Splice(const unsigned int start, unsigned int len)
{
    if(length == 0 || len == 0)
    {
        return;
    }

    if(len >= length)
    {
        len = length;
    }

    type *tmp = new type[len];

    for(unsigned int i = 0; i < len; i++)
    {
        tmp[i] = data[start+i];
    }

    delete[] data;
    data = tmp;
    length = length - len;
    aidx = length-1;
}

//
// kexArray::Sort
//
// Note that data will be shuffled around, so this could invalidate any
// pointers that relies on the array/data
//
template<class type>
void kexArray<type>::Sort(compare_t *function)
{
    if(data == NULL)
    {
        return;
    }

    typedef int compareCast(const void*, const void*);
    compareCast *cmpFunc = (compareCast*)function;

    qsort((void*)data, length, sizeof(type), cmpFunc);
}

//
// kexArray::Sort
//
// Note that data will be shuffled around, so this could invalidate any
// pointers that relies on the array/data
//
template<class type>
void kexArray<type>::Sort(compare_t *function, unsigned int count)
{
    if(data == NULL)
    {
        return;
    }

    typedef int compareCast(const void*, const void*);
    compareCast *cmpFunc = (compareCast*)function;

    qsort((void*)data, count, sizeof(type), cmpFunc);
}

//
// kexArray::operator[]
//
template <class type>
type &kexArray<type>::operator[](unsigned int index)
{
    assert(index < length);
    return data[index];
}

//
// kexArray::operator=
//
template <class type>
kexArray<type> &kexArray<type>::operator=(const kexArray<type> &arr)
{
    if(data)
    {
        delete[] data;
    }

    data = NULL;
    length = arr.length;
    aidx = arr.aidx;

    if(arr.length > 0)
    {
        data = new type[arr.length];

        for(unsigned int i = 0; i < arr.length; i++)
        {
            data[i] = arr.data[i];
        }
    }

    return *this;
}
