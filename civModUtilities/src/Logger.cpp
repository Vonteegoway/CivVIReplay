#include "Logger.h"
#include <Windows.h>
#include <stdio.h>

#if ENABLE_LOGGING()
struct LOGGER_INSTANCE {
	FILE* hFile;

	LOGGER_INSTANCE() {
		wchar_t Buffer[MAX_PATH];
		DWORD Length = GetTempPathW(MAX_PATH, Buffer);
		//--
		_snwprintf(Buffer + Length, MAX_PATH - Length, L"EngineLogger.log");
		//--
		hFile = _wfopen(Buffer, L"wt+");
	}

	~LOGGER_INSTANCE() {
		fclose(hFile);
	}
};

namespace GLOBALS {
	LOGGER_INSTANCE Logger;
}

void DisplayTime() {
	unsigned __int64 UnbiasedInterruptTime;
	QueryUnbiasedInterruptTime(&UnbiasedInterruptTime);
	unsigned int Microseconds = (UnbiasedInterruptTime /= 10) % 1000;
	unsigned int Miliseconds = (UnbiasedInterruptTime /= 1000) % 1000;
	unsigned int Seconds = (UnbiasedInterruptTime /= 1000) % 60;
	unsigned int Minutes = (UnbiasedInterruptTime /= 60) % 60;
	fprintf(GLOBALS::Logger.hFile, "[%02u:%02u.%03u.%03u]", Minutes, Seconds, Miliseconds, Microseconds);
}

#ifdef __cplusplus
extern "C" {
#endif

void __cdecl TimedLogA(const char* Format, ...) {
	DisplayTime();
	va_list va;
	va_start(va, Format);
	vfprintf(GLOBALS::Logger.hFile, Format, va);
	va_end(va);
	fflush(GLOBALS::Logger.hFile);
}

void __cdecl TimedLogW(const wchar_t* Format, ...) {
	DisplayTime();
	va_list va;
	va_start(va, Format);
	vfwprintf(GLOBALS::Logger.hFile, Format, va);
	va_end(va);
	fflush(GLOBALS::Logger.hFile);
}


void __cdecl LogA(const char* Format, ...) {
	va_list va;
	va_start(va, Format);
	vfprintf(GLOBALS::Logger.hFile, Format, va);
	va_end(va);
	fflush(GLOBALS::Logger.hFile);
}

void __cdecl LogW(const wchar_t* Format, ...) {
	va_list va;
	va_start(va, Format);
	vfwprintf(GLOBALS::Logger.hFile, Format, va);
	va_end(va);
	fflush(GLOBALS::Logger.hFile);
}


#ifdef __cplusplus
}
#endif

#endif