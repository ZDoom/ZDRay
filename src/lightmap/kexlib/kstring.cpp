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
// DESCRIPTION: String Class
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include "kstring.h"

void kexStr::Init()
{
	length = 0;
	bufferLength = STRING_DEFAULT_SIZE;
	charPtr = defaultBuffer;
	charPtr[0] = '\0';
}

void kexStr::CheckSize(int size, bool bKeepString)
{
	if (size <= bufferLength)
	{
		return;
	}

	Resize(size, bKeepString);
}

void kexStr::CopyNew(const char *string, int len)
{
	CheckSize(len + 1, false);
	strcpy(charPtr, string);
	length = len;
}

kexStr::kexStr()
{
	Init();
}

kexStr::kexStr(const char *string)
{
	Init();

	if (string == NULL)
	{
		return;
	}

	CopyNew(string, strlen(string));
}

kexStr::kexStr(const char *string, const int length)
{
	Init();

	if (string == NULL)
	{
		return;
	}

	CopyNew(string, length);
}

kexStr::kexStr(const kexStr &string)
{
	Init();

	if (string.charPtr == NULL)
	{
		return;
	}

	CopyNew(string.charPtr, string.Length());
}

kexStr::~kexStr()
{
	if (charPtr != defaultBuffer)
	{
		delete[] charPtr;
		charPtr = defaultBuffer;
	}

	charPtr[0] = '\0';
	length = 0;
}

kexStr &kexStr::Concat(const char *string)
{
	return Concat(string, strlen(string));
}

kexStr &kexStr::Concat(const char c)
{
	CheckSize((length + 1) + 1, true);
	charPtr[length++] = c;
	charPtr[length] = '\0';
	return *this;
}

kexStr &kexStr::Concat(const char *string, int len)
{
	CheckSize((length + len) + 1, true);

	for (int i = 0; i < len; i++)
	{
		charPtr[length + i] = string[i];
	}

	length += len;
	charPtr[length] = '\0';

	return *this;
}

kexStr &kexStr::Copy(const kexStr &src, int len)
{
	int i = 0;
	const char *p = src;
	CheckSize((length + len) + 1, true);

	while ((len--) >= 0)
	{
		charPtr[i] = p[i];
		i++;
	}

	return *this;
}

kexStr &kexStr::Copy(const kexStr &src)
{
	return Copy(src, src.Length());
}

kexStr &kexStr::operator=(const kexStr &str)
{
	int len = str.Length();

	CheckSize(len + 1, false);
	strncpy(charPtr, str.charPtr, len);
	length = len;
	charPtr[length] = '\0';

	return *this;
}

kexStr &kexStr::operator=(const char *str)
{
	int len = strlen(str);

	CheckSize(len + 1, false);
	strncpy(charPtr, str, len);
	length = len;
	charPtr[length] = '\0';

	return *this;
}

kexStr &kexStr::operator=(const bool b)
{
	const char *str = b ? "true" : "false";
	int len = strlen(str);

	CheckSize(len + 1, false);
	strncpy(charPtr, str, len);
	length = len;
	charPtr[length] = '\0';

	return *this;
}

kexStr kexStr::operator+(const kexStr &str)
{
	kexStr out(*this);

	return out.Concat(str.c_str());
}

kexStr kexStr::operator+(const char *str)
{
	kexStr out(*this);

	return out.Concat(str);
}

kexStr kexStr::operator+(const bool b)
{
	kexStr out(*this);

	return out.Concat(b ? "true" : "false");
}

kexStr kexStr::operator+(const int i)
{
	kexStr out(*this);

	char tmp[64];
	sprintf(tmp, "%i", i);

	return out.Concat(tmp);
}

kexStr kexStr::operator+(const float f)
{
	kexStr out(*this);

	char tmp[64];
	sprintf(tmp, "%f", f);

	return out.Concat(tmp);
}

kexStr &kexStr::operator+=(const kexStr &str)
{
	return Concat(str.c_str());
}

kexStr &kexStr::operator+=(const char *str)
{
	return Concat(str);
}

kexStr &kexStr::operator+=(const char c)
{
	return Concat(c);
}

kexStr &kexStr::operator+=(const bool b)
{
	return Concat(b ? "true" : "false");
}

const char kexStr::operator[](int index) const
{
	assert(index >= 0 && index < length);
	return charPtr[index];
}

void kexStr::Resize(int size, bool bKeepString)
{

	if (size <= 0)
	{
		return;
	}

	int newsize = size + ((32 - (size & 31)) & 31);
	char *newbuffer = new char[newsize];

	if (bKeepString)
	{
		strncpy(newbuffer, charPtr, length);
	}

	if (charPtr != defaultBuffer)
	{
		delete[] charPtr;
	}

	charPtr = newbuffer;
	bufferLength = newsize;
}

int kexStr::IndexOf(const char *pattern) const
{
	int patlen = strlen(pattern);
	int i = 0;
	int j = 0;

	while (i + j < Length())
	{
		if (charPtr[i + j] == pattern[j])
		{
			if (++j == patlen)
			{
				return i;
			}
		}
		else
		{
			i++;
			j = 0;
		}
	}

	return -1;
}

int kexStr::IndexOf(const char *string, const char *pattern)
{
	int patlen = strlen(pattern);
	int i = 0;
	int j = 0;

	while (i + j < (int)strlen(string))
	{
		if (string[i + j] == pattern[j])
		{
			if (++j == patlen)
			{
				return i;
			}
		}
		else
		{
			i++;
			j = 0;
		}
	}

	return -1;
}

int kexStr::IndexOf(const kexStr &pattern) const
{
	return IndexOf(pattern.c_str());
}

kexStr &kexStr::NormalizeSlashes()
{
	for (int i = 0; i < length; i++)
	{
		if ((charPtr[i] == '/' || charPtr[i] == '\\') && charPtr[i] != DIR_SEPARATOR)
		{
			charPtr[i] = DIR_SEPARATOR;
		}
	}

	return *this;
}

kexStr &kexStr::StripPath()
{
	int pos = 0;
	int i = 0;

	pos = length;

	for (i = length - 1; charPtr[i] != '\\' && charPtr[i] != '/'; i--, pos--)
	{
		if (pos <= 0)
		{
			return *this;
		}
	}
	length = length - pos;
	for (i = 0; i < length; i++)
	{
		charPtr[i] = charPtr[pos + i];
	}

	CheckSize(length, true);
	charPtr[length] = '\0';
	return *this;
}

kexStr &kexStr::StripExtension()
{
	int pos = IndexOf(".");

	if (pos == -1)
	{
		return *this;
	}

	length = pos;
	CheckSize(length, true);
	charPtr[length] = '\0';

	return *this;
}

kexStr &kexStr::StripFile()
{
	int pos = 0;
	int i = 0;

	if (IndexOf(".") == -1)
	{
		return *this;
	}

	pos = length;

	for (i = length - 1; charPtr[i] != '\\' && charPtr[i] != '/'; i--, pos--)
	{
		if (pos <= 0)
		{
			return *this;
		}
	}

	length = pos;
	CheckSize(length, true);
	charPtr[length] = '\0';

	return *this;
}

int kexStr::Hash()
{
	unsigned int hash = 0;
	char *str = (char*)charPtr;
	char c;

	while ((c = *str++))
	{
		hash = c + (hash << 6) + (hash << 16) - hash;
	}

	return hash & (MAX_HASH - 1);
}

int kexStr::Hash(const char *s)
{
	unsigned int hash = 0;
	char *str = (char*)s;
	char c;

	while ((c = *str++))
	{
		hash = c + (hash << 6) + (hash << 16) - hash;
	}

	return hash & (MAX_HASH - 1);
}

char *kexStr::Format(const char *str, ...)
{
	va_list v;
	static char vastr[1024];

	va_start(v, str);
	vsprintf(vastr, str, v);
	va_end(v);

	return vastr;
}

kexStr kexStr::Substr(int start, int len) const
{
	kexStr str;
	int l = Length();

	if (l <= 0 || start >= l)
	{
		return str;
	}

	if (start + len >= l)
	{
		len = l - start;
	}

	return str.Concat((const char*)&charPtr[start], len);
}
/*
void kexStr::Split(kexStrList &list, const char seperator)
{
	int splitLen = 0;
	int startOffs = 0;
	for (int i = 0; i < length; i++)
	{
		if (charPtr[i] == seperator)
		{
			if (splitLen == 0)
			{
				continue;
			}

			list.Push(kexStr(&charPtr[startOffs], splitLen));
			startOffs += (splitLen + 1);
			splitLen = 0;
			continue;
		}

		splitLen++;
	}

	if (splitLen != 0 && startOffs != 0)
	{
		list.Push(kexStr(&charPtr[startOffs], splitLen));
	}
}
*/
int kexStr::Atoi()
{
	return atoi(charPtr);
}

float kexStr::Atof()
{
	return (float)atof(charPtr);
}

void kexStr::WriteToFile(const char *file)
{
	if (length <= 0)
	{
		return;
	}

	FILE *f = fopen(file, "w");
	fprintf(f, "%s", charPtr);
	fclose(f);
}

kexStr &kexStr::ToUpper()
{
	char c;
	for (int i = 0; i < length; i++)
	{
		c = charPtr[i];
		if (c >= 'a' && c <= 'z')
		{
			c -= 'a' - 'A';
		}
		charPtr[i] = c;
	}

	return *this;
}

kexStr &kexStr::ToLower()
{
	char c;
	for (int i = 0; i < length; i++)
	{
		c = charPtr[i];
		if (c >= 'A' && c <= 'Z')
		{
			c += 32;
		}
		charPtr[i] = c;
	}

	return *this;
}

bool kexStr::CompareCase(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (*s1 != *s2)
		{
			return (*s2 - *s1) != 0;
		}
		s1++;
		s2++;
	}
	if (*s1 != *s2)
	{
		return (*s2 - *s1) != 0;
	}

	return false;
}

bool kexStr::CompareCase(const kexStr &a, const kexStr &b)
{
	return CompareCase(a.c_str(), b.c_str());
}

bool kexStr::Compare(const char *s1, const char *s2)
{
	const byte *us1 = (const byte*)s1;
	const byte *us2 = (const byte*)s2;

	while (tolower(*us1) == tolower(*us2))
	{
		if (*us1++ == '\0')
		{
			return false;
		}

		us2++;
	}

	return (tolower(*us1) - tolower(*us2)) != 0;
}

bool kexStr::Compare(const kexStr &a, const kexStr &b)
{
	return Compare(a.c_str(), b.c_str());
}
