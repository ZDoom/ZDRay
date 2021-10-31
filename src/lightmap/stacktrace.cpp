
#include "stacktrace.h"

#ifdef WIN32
#include <Windows.h>
#include <DbgHelp.h>
#else
#include <execinfo.h>
#include <cxxabi.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#endif

#ifdef WIN32
#pragma comment(lib, "dbghelp.lib")
class NativeSymbolResolver
{
public:
	NativeSymbolResolver() { SymInitialize(GetCurrentProcess(), nullptr, TRUE); }
	~NativeSymbolResolver() { SymCleanup(GetCurrentProcess()); }

	std::string GetName(void* frame)
	{
		std::string s;

		unsigned char buffer[sizeof(IMAGEHLP_SYMBOL64) + 128];
		IMAGEHLP_SYMBOL64* symbol64 = reinterpret_cast<IMAGEHLP_SYMBOL64*>(buffer);
		memset(symbol64, 0, sizeof(IMAGEHLP_SYMBOL64) + 128);
		symbol64->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		symbol64->MaxNameLength = 128;

		DWORD64 displacement = 0;
		BOOL result = SymGetSymFromAddr64(GetCurrentProcess(), (DWORD64)frame, &displacement, symbol64);
		if (result)
		{
			IMAGEHLP_LINE64 line64;
			DWORD displacement = 0;
			memset(&line64, 0, sizeof(IMAGEHLP_LINE64));
			line64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			result = SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)frame, &displacement, &line64);
			if (result)
			{
				s = std::string("Called from ") + symbol64->Name + " at " + line64.FileName + ", line " + std::to_string(line64.LineNumber) + "\n", symbol64->Name;
			}
			else
			{
				s = std::string("Called from ") + symbol64->Name + "\n";
			}
		}

		return s;
	}
};
#else
class NativeSymbolResolver
{
public:
	std::string GetName(void* frame)
	{
		std::string s;
		char** strings;
		void* frames[1] = { frame };
		strings = backtrace_symbols(frames, 1);

		// Decode the strings
		char* ptr = strings[0];
		char* filename = ptr;
		const char* function = "";

		// Find function name
		while (*ptr)
		{
			if (*ptr == '(')	// Found function name
			{
				*(ptr++) = 0;
				function = ptr;
				break;
			}
			ptr++;
		}

		// Find offset
		if (function[0])	// Only if function was found
		{
			while (*ptr)
			{
				if (*ptr == '+')	// Found function offset
				{
					*(ptr++) = 0;
					break;
				}
				if (*ptr == ')')	// Not found function offset, but found, end of function
				{
					*(ptr++) = 0;
					break;
				}
				ptr++;
			}
		}

		int status;
		char* new_function = abi::__cxa_demangle(function, nullptr, nullptr, &status);
		if (new_function)	// Was correctly decoded
		{
			function = new_function;
		}

		s = std::string("Called from ") + function + " at " + filename + "\n";

		if (new_function)
		{
			free(new_function);
		}

		free(strings);
		return s;
	}
};
#endif

static int CaptureStackTrace(int max_frames, void** out_frames)
{
	memset(out_frames, 0, sizeof(void*) * max_frames);

#ifdef _WIN64
	// RtlCaptureStackBackTrace doesn't support RtlAddFunctionTable..

	CONTEXT context;
	RtlCaptureContext(&context);

	UNWIND_HISTORY_TABLE history;
	memset(&history, 0, sizeof(UNWIND_HISTORY_TABLE));

	ULONG64 establisherframe = 0;
	PVOID handlerdata = nullptr;

	int frame;
	for (frame = 0; frame < max_frames; frame++)
	{
		ULONG64 imagebase;
		PRUNTIME_FUNCTION rtfunc = RtlLookupFunctionEntry(context.Rip, &imagebase, &history);

		KNONVOLATILE_CONTEXT_POINTERS nvcontext;
		memset(&nvcontext, 0, sizeof(KNONVOLATILE_CONTEXT_POINTERS));
		if (!rtfunc)
		{
			// Leaf function
			context.Rip = (ULONG64)(*(PULONG64)context.Rsp);
			context.Rsp += 8;
		}
		else
		{
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, imagebase, context.Rip, rtfunc, &context, &handlerdata, &establisherframe, &nvcontext);
		}

		if (!context.Rip)
			break;

		out_frames[frame] = (void*)context.Rip;
	}
	return frame;

#elif defined(WIN32)
	return 0;//return RtlCaptureStackBackTrace(0, MIN(max_frames, 32), out_frames, nullptr);
#else
	return backtrace(out_frames, max_frames);
#endif
}

std::string CaptureStackTraceText(int framesToSkip)
{
	void* frames[32];
	int numframes = CaptureStackTrace(32, frames);

	NativeSymbolResolver nativeSymbols;

	std::string s;
	for (int i = framesToSkip + 1; i < numframes; i++)
	{
		s += nativeSymbols.GetName(frames[i]);
	}
	return s;
}
