/*
	The main glue for ZDRay.
	Copyright (C) 2002-2006 Randy Heit

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32

// Need windows.h for QueryPerformanceCounter
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define HAVE_TIMING 1
#define START_COUNTER(s,e,f) \
	LARGE_INTEGER s, e, f; QueryPerformanceCounter (&s);
#define END_COUNTER(s,e,f,l) \
	QueryPerformanceCounter (&e); QueryPerformanceFrequency (&f); \
	if (!NoTiming) printf (l, double(e.QuadPart - s.QuadPart) / double(f.QuadPart));

#else

#include <time.h>
#define HAVE_TIMING 1
#define START_COUNTER(s,e,f) \
	clock_t s, e; s = clock();
#define END_COUNTER(s,e,f,l) \
	e = clock(); \
	if (!NoTiming) printf (l, double(e - s) / CLOCKS_PER_SEC);

// Need these to check if input/output are the same file
#include <sys/types.h>
#include <sys/stat.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <thread>

#include "framework/zdray.h"
#include "wad/wad.h"
#include "level/level.h"
#include "commandline/getopt.h"

// MACROS ------------------------------------------------------------------

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ParseArgs(int argc, char **argv);
static void ShowUsage();
static void ShowVersion();
static bool CheckInOutNames();

#ifndef DISABLE_SSE
static void CheckSSE();
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern "C" int optind;
extern "C" char *optarg;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

const char		*Map = NULL;
const char		*InName;
const char		*OutName = "tmp.wad";
bool			 BuildNodes = true;
bool			 BuildGLNodes = true;// false;
bool			 ConformNodes = false;
bool			 NoPrune = false;
EBlockmapMode	 BlockmapMode = EBM_Rebuild;
ERejectMode		 RejectMode = ERM_DontTouch;
bool			 WriteComments = false;
int				 MaxSegs = 64;
int				 SplitCost = 8;
int				 AAPreference = 16;
bool			 CheckPolyobjs = true;
bool			 ShowMap = false;
bool			 ShowWarnings = false;
bool			 NoTiming = false;
bool			 CompressNodes = true;// false;
bool			 CompressGLNodes = true;// false;
bool			 ForceCompression = true;// false;
bool			 GLOnly = true;// false;
bool			 V5GLNodes = false;
bool			 HaveSSE1, HaveSSE2;
int				 SSELevel;
int				 NumThreads = 0;
int				 LMDims = LIGHTMAP_MAX_SIZE;
int				 Samples = 8;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static option long_opts[] =
{
	{"help",			no_argument,		0,	1000},
	{"version",			no_argument,		0,	'V'},
	{"view",			no_argument,		0,	'v'},
	{"warn",			no_argument,		0,	'w'},
	{"map",				required_argument,	0,	'm'},
	{"output",			required_argument,	0,	'o'},
	{"output-file",		required_argument,	0,	'o'},
	{"file",			required_argument,	0,	'f'},
	{"no-nodes",		no_argument,		0,	'N'},
	{"gl",				no_argument,		0,	'g'},
	{"gl-matching",		no_argument,		0,	'G'},
	{"empty-blockmap",	no_argument,		0,	'b'},
	{"empty-reject",	no_argument,		0,	'r'},
	{"zero-reject",		no_argument,		0,	'R'},
	{"full-reject",		no_argument,		0,	'e'},
	{"no-reject",		no_argument,		0,	'E'},
	{"partition",		required_argument,	0,	'p'},
	{"split-cost",		required_argument,	0,	's'},
	{"diagonal-cost",	required_argument,	0,	'd'},
	{"no-polyobjs",		no_argument,		0,	'P'},
	{"no-prune",		no_argument,		0,	'q'},
	{"no-timing",		no_argument,		0,	't'},
	{"compress",		no_argument,		0,	'z'},
	{"compress-normal",	no_argument,		0,	'Z'},
	{"extended",		no_argument,		0,	'X'},
	{"gl-only",			no_argument,		0,	'x'},
	{"gl-v5",			no_argument,		0,	'5'},
	{"no-sse",			no_argument,		0,  1002},
	{"no-sse2",			no_argument,		0,  1003},
	{"comments",		no_argument,		0,	'c'},
	{"threads",			required_argument,	0,	'j'},
	{"samples",			required_argument,	0,	'Q'},
	{"size",			required_argument,	0,	'S'},
	{0,0,0,0}
};

static const char short_opts[] = "wVgGvbNrReEm:o:f:p:s:d:PqtzZXx5c";

// CODE --------------------------------------------------------------------

int main(int argc, char **argv)
{
	bool fixSame = false;

#ifdef DISABLE_SSE
	HaveSSE1 = HaveSSE2 = false;
#else
	HaveSSE1 = HaveSSE2 = true;
#endif

	ParseArgs(argc, argv);

	if (InName == NULL)
	{
		if (optind >= argc || optind < argc - 1)
		{ // Source file is unspecified or followed by junk
			ShowUsage();
			return 0;
		}

		InName = argv[optind];
	}

#ifndef DISABLE_SSE
	CheckSSE();
#endif

	try
	{
		START_COUNTER(t1a, t1b, t1c)

		if (CheckInOutNames())
		{
			// When the input and output files are the same, output will go to
			// a temporary file. After everything is done, the input file is
			// deleted and the output file is renamed to match the input file.

			char *out = new char[strlen(OutName) + 3], *dot;

			if (out == NULL)
			{
				throw std::runtime_error("Could not create temporary file name.");
			}

			strcpy(out, OutName);
			dot = strrchr(out, '.');
			if (dot && (dot[1] == 'w' || dot[1] == 'W')
				&& (dot[2] == 'a' || dot[2] == 'A')
				&& (dot[3] == 'd' || dot[3] == 'D')
				&& dot[4] == 0)
			{
				// *.wad becomes *.daw
				dot[1] = 'd';
				dot[3] = 'w';
			}
			else
			{
				// * becomes *.x
				strcat(out, ".x");
			}
			OutName = out;
			fixSame = true;
		}

		{
			FWadReader inwad(InName);
			FWadWriter outwad(OutName, inwad.IsIWAD());

			int lump = 0;
			int max = inwad.NumLumps();

			while (lump < max)
			{
				if (inwad.IsMap(lump) && (!Map || stricmp(inwad.LumpName(lump), Map) == 0))
				{
					START_COUNTER(t2a, t2b, t2c)
					FProcessor builder(inwad, lump);
					builder.BuildNodes();
					builder.BuildLightmaps("lightconfig.txt");
					builder.Write(outwad);
					END_COUNTER(t2a, t2b, t2c, "   %.3f seconds.\n")

					lump = inwad.LumpAfterMap(lump);
				}
				else if (inwad.IsGLNodes(lump))
				{
					// Ignore GL nodes from the input for any maps we process.
					if (BuildNodes && (Map == NULL || stricmp(inwad.LumpName(lump) + 3, Map) == 0))
					{
						lump = inwad.SkipGLNodes(lump);
					}
					else
					{
						outwad.CopyLump(inwad, lump);
						++lump;
					}
				}
				else
				{
					//printf ("copy %s\n", inwad.LumpName (lump));
					outwad.CopyLump(inwad, lump);
					++lump;
				}
			}

			outwad.Close();
		}

		if (fixSame)
		{
			remove(InName);
			if (0 != rename(OutName, InName))
			{
				printf("The output file could not be renamed to %s.\nYou can find it as %s.\n",
					InName, OutName);
			}
		}

		END_COUNTER(t1a, t1b, t1c, "\nTotal time: %.3f seconds.\n")
	}
	catch (std::runtime_error msg)
	{
		printf("%s\n", msg.what());
		return 20;
	}
	catch (std::bad_alloc)
	{
		printf("Out of memory\n");
		return 20;
	}
	catch (std::exception msg)
	{
		printf("%s\n", msg.what());
		return 20;
	}
#ifndef _DEBUG
	catch (...)
	{
		printf("Unhandled exception. ZDRay cannot continue.\n");
		return 20;
	}
#endif

	return 0;
}

//==========================================================================
//
// ParseArgs
//
//==========================================================================

static void ParseArgs(int argc, char **argv)
{
	int ch;

	while ((ch = getopt_long(argc, argv, short_opts, long_opts, NULL)) != EOF)
	{
		switch (ch)
		{
		case 0:
			break;

		case 'v':
			ShowMap = true;
			break;
		case 'w':
			ShowWarnings = true;
			break;
		case 'm':
			Map = optarg;
			break;
		case 'o':
			OutName = optarg;
			break;
		case 'f':
			InName = optarg;
			break;
		case 'N':
			BuildNodes = false;
			break;
		case 'b':
			BlockmapMode = EBM_Create0;
			break;
		case 'r':
			RejectMode = ERM_Create0;
			break;
		case 'R':
			RejectMode = ERM_CreateZeroes;
			break;
		case 'e':
			RejectMode = ERM_Rebuild;
			break;
		case 'E':
			RejectMode = ERM_DontTouch;
			break;
		case 'p':
			MaxSegs = atoi(optarg);
			if (MaxSegs < 3)
			{ // Don't be too unreasonable
				MaxSegs = 3;
			}
			break;
		case 's':
			SplitCost = atoi(optarg);
			if (SplitCost < 1)
			{ // 1 means to add no extra weight at all
				SplitCost = 1;
			}
			break;
		case 'd':
			AAPreference = atoi(optarg);
			if (AAPreference < 1)
			{
				AAPreference = 1;
			}
			break;
		case 'P':
			CheckPolyobjs = false;
			break;
		case 'g':
			BuildGLNodes = true;
			ConformNodes = false;
			break;
		case 'G':
			BuildGLNodes = true;
			ConformNodes = true;
			break;
		case 'X':
			CompressNodes = true;
			CompressGLNodes = true;
			ForceCompression = false;
			break;
		case 'z':
			CompressNodes = true;
			CompressGLNodes = true;
			ForceCompression = true;
			break;
		case 'Z':
			CompressNodes = true;
			CompressGLNodes = false;
			ForceCompression = true;
			break;
		case 'x':
			GLOnly = true;
			BuildGLNodes = true;
			ConformNodes = false;
			break;
		case '5':
			V5GLNodes = true;
			break;
		case 'q':
			NoPrune = true;
			break;
		case 't':
			NoTiming = true;
			break;
		case 'c':
			WriteComments = true;
			break;
		case 'V':
			ShowVersion();
			exit(0);
			break;
		case 1002:		// Disable SSE/SSE2 ClassifyLine routine
			HaveSSE1 = false;
			HaveSSE2 = false;
			break;
		case 1003:		// Disable only SSE2 ClassifyLine routine
			HaveSSE2 = false;
			break;
		case 'j':
			NumThreads = atoi(optarg);
			break;
		case 'Q':
			Samples = atoi(optarg);
			if (Samples <= 0) Samples = 1;
			if (Samples > 128) Samples = 128;
			Samples = kexMath::RoundPowerOfTwo(Samples);
			break;
		case 'S':
			LMDims = atoi(optarg);
			if (LMDims <= 0) LMDims = 1;
			if (LMDims > LIGHTMAP_MAX_SIZE) LMDims = LIGHTMAP_MAX_SIZE;
			LMDims = kexMath::RoundPowerOfTwo(LMDims);
			break;
		case 1000:
			ShowUsage();
			exit(0);
		default:
			printf("Try `zdray --help' for more information.\n");
			exit(0);
		}
	}
}

//==========================================================================
//
// ShowUsage
//
//==========================================================================

static void ShowUsage()
{
	printf(
		"Usage: zdray [options] sourcefile.wad\n"
		"  -m, --map=MAP            Only affect the specified map\n"
		"  -o, --output=FILE        Write output to FILE instead of tmp.wad\n"
		"  -q, --no-prune           Keep unused sidedefs and sectors\n"
		"  -N, --no-nodes           Do not rebuild nodes\n"
		"  -g, --gl                 Build GL-friendly nodes\n"
		"  -G, --gl-matching        Build GL-friendly nodes that match normal nodes\n"
		"  -x, --gl-only            Only build GL-friendly nodes\n"
		"  -5, --gl-v5              Create v5 GL-friendly nodes (overriden by -z and -X)\n"
		"  -X, --extended           Create extended nodes (including GL nodes, if built)\n"
		"  -z, --compress           Compress the nodes (including GL nodes, if built)\n"
		"  -Z, --compress-normal    Compress normal nodes but not GL nodes\n"
		"  -b, --empty-blockmap     Create an empty blockmap\n"
		"  -r, --empty-reject       Create an empty reject table\n"
		"  -R, --zero-reject        Create a reject table of all zeroes\n"
		//"  -e, --full-reject        Rebuild reject table (unsupported)\n"
		"  -E, --no-reject          Leave reject table untouched\n"
		"  -p, --partition=NNN      Maximum segs to consider at each node (default %d)\n"
		"  -s, --split-cost=NNN     Cost for splitting segs (default %d)\n"
		"  -d, --diagonal-cost=NNN  Cost for avoiding diagonal splitters (default %d)\n"
		"  -P, --no-polyobjs        Do not check for polyobject subsector splits\n"
		"  -j, --threads=NNN        Number of threads used for raytracing (default %d)\n"
		"  -Q, --samples=NNN        Set texel sampling size (lowest = higher quaility but\n"
		"                           slow compile time) must be in powers of two\n"
		"  -S, --size=NNN           lightmap texture dimensions for width and height\n"
		"                           must be in powers of two (1, 2, 4, 8, 16, etc)\n"
#ifdef _WIN32
		"  -v, --view               View the nodes\n"
#endif
		"  -w, --warn               Show warning messages\n"
#if HAVE_TIMING
		"  -t, --no-timing          Suppress timing information\n"
#endif
		"  -V, --version            Display version information\n"
		"      --help               Display this usage information"
#ifndef _WIN32
		"\n"
#endif
		, MaxSegs /* Partition size */
		, SplitCost
		, AAPreference
		, (int)std::thread::hardware_concurrency()
	);
}

//==========================================================================
//
// ShowVersion
//
//==========================================================================

static void ShowVersion()
{
	printf("ZDRay " ZDRAY_VERSION " ("
#if defined(__GNUC__)

		"GCC"
#if defined(__i386__)
		"-x86"
#elif defined(__amd64__)
		"-amd64"
#elif defined(__ppc__)
		"-ppc"
#endif

#elif defined(_MSC_VER)

		"VC"
#if defined(_M_IX86)
		"-x86"
#if _M_IX86_FP > 1
		"-SSE2"
#endif
#elif defined(_M_X64)
		"-x64"
#endif

#endif

		" : " __DATE__ ")\n");
}

//==========================================================================
//
// CheckInOutNames
//
// Returns true if InName and OutName refer to the same file. This needs
// to be implemented different under Windows than Unix because the inode
// information returned by stat is always 0, so it cannot be used to
// determine duplicate files.
//
//==========================================================================

static bool CheckInOutNames()
{
#ifndef _WIN32
	struct stat info;
	dev_t outdev;
	ino_t outinode;

	if (0 != stat(OutName, &info))
	{ // If out doesn't exist, it can't be duplicated
		return false;
	}
	outdev = info.st_dev;
	outinode = info.st_ino;
	if (0 != stat(InName, &info))
	{
		return false;
	}
	return outinode == info.st_ino && outdev == info.st_dev;
#else
	HANDLE inFile, outFile;

	outFile = CreateFile(OutName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (outFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	inFile = CreateFile(InName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (inFile == INVALID_HANDLE_VALUE)
	{
		CloseHandle(outFile);
		return false;
	}

	BY_HANDLE_FILE_INFORMATION inInfo, outInfo;
	bool same = false;

	if (GetFileInformationByHandle(inFile, &inInfo) &&
		GetFileInformationByHandle(outFile, &outInfo))
	{
		same = inInfo.dwVolumeSerialNumber == outInfo.dwVolumeSerialNumber &&
			inInfo.nFileIndexLow == outInfo.nFileIndexLow &&
			inInfo.nFileIndexHigh == outInfo.nFileIndexHigh;
	}

	CloseHandle(inFile);
	CloseHandle(outFile);

	return same;
#endif
}

//==========================================================================
//
// CheckSSE
//
// Checks if the processor supports SSE or SSE2.
//
//==========================================================================

#ifndef DISABLE_SSE
static void CheckSSE()
{
#ifdef __SSE2__
	// If we compiled with SSE2 support enabled for everything, then
	// obviously it's available, or the program won't get very far.
	return;
#endif

	if (!HaveSSE2 && !HaveSSE1)
	{
		return;
	}

	bool forcenosse1 = !HaveSSE1;
	bool forcenosse2 = !HaveSSE2;

	HaveSSE1 = false;
	HaveSSE2 = false;
#if defined(_MSC_VER) && !defined(__clang__)

#ifdef _M_X64
	// Processors implementing AMD64 are required to support SSE2.
	return;
#else
	__asm
	{
		pushfd				// save EFLAGS
		pop eax				// store EFLAGS in EAX
		mov edx, eax			// save in EDX for later testing
		xor eax, 0x00200000	// toggle bit 21
		push eax			// put to stack
		popfd				// save changed EAX to EFLAGS
		pushfd				// push EFLAGS to TOS
		pop eax				// store EFLAGS in EAX
		cmp eax, edx			// see if bit 21 has changed
		jz noid				// if no change, then no CPUID

		// Check the feature flag for SSE/SSE2
		mov eax, 1
		cpuid
		test edx, (1 << 25)	// Check for SSE
		setnz HaveSSE1
		test edx, (1 << 26)	// Check for SSE2
		setnz HaveSSE2
		noid :
	}
#endif

#elif defined(__GNUC__) || defined(__clang__)

	// Same as above, but for GCC
	asm volatile
		("pushfl\n\t"
			"popl %%eax\n\t"
			"movl %%eax,%%edx\n\t"
			"xorl $0x200000,%%eax\n\t"
			"pushl %%eax\n\t"
			"popfl\n\t"
			"pushfl\n\t"
			"popl %%eax\n\t"
			"cmp %%edx,%%eax\n\t"
			"jz noid\n\t"
			"mov $1,%%eax\n\t"
			"cpuid\n\t"
			"test $(1<<25),%%edx\n\t"
			"setneb %0\n"
			"test $(1<<26),%%edx\n\t"
			"setneb %1\n"
			"noid:"
			:"=m" (HaveSSE1), "=m" (HaveSSE2)::"eax", "ebx", "ecx", "edx");

#endif

	if (forcenosse1)
	{
		HaveSSE1 = false;
	}
	if (forcenosse2)
	{
		HaveSSE2 = false;
	}
}
#endif

//==========================================================================

void Warn(const char *format, ...)
{
	va_list marker;

	if (!ShowWarnings)
	{
		return;
	}

	va_start(marker, format);
	vprintf(format, marker);
	va_end(marker);
}

void Error(const char *error, ...)
{
	va_list argptr;

	va_start(argptr, error);
	vprintf(error, argptr);
	va_end(argptr);
	printf("\n");
	exit(1);
}

char *Va(const char *str, ...)
{
	va_list v;
	static char vastr[1024];

	va_start(v, str);
	vsprintf(vastr, str, v);
	va_end(v);

	return vastr;
}

void Delay(int ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

const int64_t GetSeconds()
{
	return time(0);
}

#if 0 // Old dlight main function

int main(int argc, char **argv)
{
	kexWadFile wadFile;
	kexWadFile outWadFile;
	FLevel doomMap;
	lump_t *lmLump;
	kexArray<int> ignoreLumps;
	kexLightmapBuilder builder;
	kexStr configFile("strife_sve.cfg");
	bool bWriteTGA;
	int map = 1;
	int arg = 1;

	printf("DLight (c) 2013-2014 Samuel Villarreal\n\n");

	if (argc < 1 || argv[1] == NULL)
	{
		printf("Usage: dlight [options] [wadfile]\n");
		printf("Use -help for list of options\n");
		return 0;
	}

	basePath = argv[0];
	basePath.StripFile();

	bWriteTGA = false;

	while (1)
	{
		if (!strcmp(argv[arg], "-help"))
		{
			printf("Options:\n");
			printf("-help:              displays all known options\n");
			printf("-map:               process lightmap for MAP##\n");
			printf("-samples:           set texel sampling size (lowest = higher quaility but\n");
			printf("                    slow compile time) must be in powers of two\n");
			printf("-ambience:          set global ambience value for lightmaps (0.0 - 1.0)\n");
			printf("-size:              lightmap texture dimentions for width and height\n");
			printf("                    must be in powers of two (1, 2, 4, 8, 16, etc)\n");
			printf("-threads:           set total number of threads (1 min, 128 max)\n");
			printf("-config:            specify a config file to parse (default: strife_sve.cfg)\n");
			printf("-writetga:          dumps lightmaps to targa (.TGA) files\n");
			arg++;
			return 0;
		}
		else if (!strcmp(argv[arg], "-map"))
		{
			if (argv[arg + 1] == NULL)
			{
				Error("Specify map number for -map\n");
				return 1;
			}

			map = atoi(argv[++arg]);
			arg++;
		}
		else if (!strcmp(argv[arg], "-samples"))
		{
			if (argv[arg + 1] == NULL)
			{
				Error("Specify value for -samples\n");
				return 1;
			}

			builder.samples = atoi(argv[++arg]);
			if (builder.samples <= 0)
			{
				builder.samples = 1;
			}
			if (builder.samples > 128)
			{
				builder.samples = 128;
			}

			builder.samples = kexMath::RoundPowerOfTwo(builder.samples);
			arg++;
		}
		else if (!strcmp(argv[arg], "-ambience"))
		{
			if (argv[arg + 1] == NULL)
			{
				Error("Specify value for -ambience\n");
				return 1;
			}

			builder.ambience = (float)atof(argv[++arg]);
			if (builder.ambience < 0)
			{
				builder.ambience = 0;
			}
			if (builder.ambience > 1)
			{
				builder.ambience = 1;
			}
		}
		else if (!strcmp(argv[arg], "-size"))
		{
			int lmDims;

			if (argv[arg + 1] == NULL)
			{
				Error("Specify value for -size\n");
				return 1;
			}

			lmDims = atoi(argv[++arg]);
			if (lmDims <= 0)
			{
				lmDims = 1;
			}
			if (lmDims > LIGHTMAP_MAX_SIZE)
			{
				lmDims = LIGHTMAP_MAX_SIZE;
			}

			lmDims = kexMath::RoundPowerOfTwo(lmDims);

			builder.textureWidth = lmDims;
			builder.textureHeight = lmDims;
			arg++;
		}
		else if (!strcmp(argv[arg], "-threads"))
		{
			kexWorker::maxWorkThreads = atoi(argv[++arg]);
			kexMath::Clamp(kexWorker::maxWorkThreads, 1, MAX_THREADS);
			arg++;
		}
		else if (!strcmp(argv[arg], "-config"))
		{
			configFile = argv[++arg];
			arg++;
		}
		else if (!strcmp(argv[arg], "-writetga"))
		{
			bWriteTGA = true;
			arg++;
		}
		else
		{
			break;
		}
	}

	if (argv[arg] == NULL)
	{
		printf("Usage: dlight [options] [wadfile]\n");
		return 0;
	}

	if (!wadFile.Open(argv[arg]))
	{
		return 1;
	}

	// concat the base path to light def file if there is none
	if (configFile.IndexOf("\\") == -1 && configFile.IndexOf("/") == -1)
	{
		configFile = basePath + configFile;
	}

	int starttime = (int)GetSeconds();

	printf("---------------- Parsing config file ----------------\n\n");
	doomMap.ParseConfigFile(configFile.c_str());

	printf("------------- Building level structures -------------\n\n");
	wadFile.SetCurrentMap(map);
	doomMap.BuildMapFromWad(wadFile);

	printf("----------- Allocating surfaces from level ----------\n\n");
	Surface_AllocateFromMap(doomMap);

	printf("---------------- Allocating lights ----------------\n\n");
	doomMap.CreateLights();

	printf("---------------- Creating lightmaps ---------------\n\n");
	builder.CreateLightmaps(doomMap);
	doomMap.CleanupThingLights();

	if (bWriteTGA)
	{
		builder.WriteTexturesToTGA();
	}

	printf("------------------ Rebuilding wad ----------------\n\n");
	wadFile.CreateBackup();

	lmLump = wadFile.GetLumpFromName(Va("LM_MAP%02d", wadFile.currentmap));

	if (lmLump)
	{
		int lumpnum = lmLump - wadFile.lumps;

		ignoreLumps.Push(lumpnum + ML_LM_LABEL);
		ignoreLumps.Push(lumpnum + ML_LM_CELLS);
		ignoreLumps.Push(lumpnum + ML_LM_SUN);
		ignoreLumps.Push(lumpnum + ML_LM_SURFS);
		ignoreLumps.Push(lumpnum + ML_LM_TXCRD);
		ignoreLumps.Push(lumpnum + ML_LM_LMAPS);
	}

	outWadFile.InitForWrite();
	outWadFile.CopyLumpsFromWadFile(wadFile, ignoreLumps);
	outWadFile.AddLump(Va("LM_MAP%02d", wadFile.currentmap), 0, NULL);

	builder.AddLightGridLump(outWadFile);
	builder.AddLightmapLumps(outWadFile);

	printf("------------- Writing %s -------------\n\n", wadFile.wadName.c_str());
	outWadFile.Write(wadFile.wadName);
	outWadFile.Close();
	wadFile.Close();

	printf("----------------- Shutting down -----------------\n\n");
	Mem_Purge(hb_static);

	int proctime = (int)GetSeconds() - starttime;
	printf("\nBuild time: %d:%02d:%02d\n",
		proctime / 3600, (proctime / 60) % 60, proctime % 60);

	return 0;
}

#endif
