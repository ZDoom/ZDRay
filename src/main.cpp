
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "commandline/getopt.h"
#include "framework/zdray.h"
#include "wad/wad.h"
#include "level/level.h"
#include "lightmap/lightmap.h"

static void ParseArgs(int argc, char **argv);
static void ShowUsage();
static void ShowVersion();

const char *Map = nullptr;
const char *InName = nullptr;
const char *OutName = "zdray.bin";
bool ShowMap = false;
bool ShowWarnings = false;

// Constants that used to be args in zdbsp
bool NoPrune = false;
EBlockmapMode BlockmapMode = EBM_Rebuild;
ERejectMode RejectMode = ERM_DontTouch;
int MaxSegs = 64;
int SplitCost = 8;
int AAPreference = 16;
bool CheckPolyobjs = true;

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
	{0,0,0,0}
};

static const char short_opts[] = "wVvm:o:f:";

int main(int argc, char **argv)
{
	ParseArgs(argc, argv);

	if (InName == nullptr)
	{
		if (optind >= argc || optind < argc - 1)
		{
			ShowUsage();
			return 0;
		}

		InName = argv[optind];
	}

	try
	{
		FWadReader inwad(InName);

		int lump = 0;
		int max = inwad.NumLumps();
		while (lump < max)
		{
			if (inwad.IsMap(lump) && (!Map || stricmp(inwad.LumpName(lump), Map) == 0))
			{
				FLevelLoader loader(inwad, lump);
				loader.BuildNodes();

				loader.Level.ParseConfigFile("lightconfig.txt");
				loader.Level.SetupDlight();
				Surface_AllocateFromMap(loader.Level);
				loader.Level.CreateLights();

				kexLightmapBuilder builder;
				builder.CreateLightmaps(loader.Level);
				builder.WriteTexturesToTGA();

				builder.WriteMeshToOBJ();

				loader.Level.CleanupThingLights();

				lump = inwad.LumpAfterMap(lump);
			}
			else
			{
				++lump;
			}
		}
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

static void ParseArgs(int argc, char **argv)
{
	int ch;

	while ((ch = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != EOF)
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
		case 'V':
			ShowVersion();
			exit(0);
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
		"  -o, --output=FILE        Write output to FILE instead of zdray.bin\n"
#ifdef _WIN32
		"  -v, --view               View progress\n"
#endif
		"  -w, --warn               Show warning messages\n"
		"  -V, --version            Display version information\n"
		"      --help               Display this usage information"
#ifndef _WIN32
		"\n"
#else
		"\r\n"
#endif
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
