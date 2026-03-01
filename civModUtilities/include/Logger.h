#pragma once

#define ENABLE_LOGGING() 1

#ifdef __cplusplus
extern "C" {
#endif

#if ENABLE_LOGGING()
	void __cdecl TimedLogA(const char* Format, ...);
	void __cdecl TimedLogW(const wchar_t* Format, ...);
	void __cdecl LogA(const char* Format, ...);
	void __cdecl LogW(const wchar_t* Format, ...);
#else
	inline void TimedLogA(const char* Format, ...) {}
	inline void TimedLogW(const wchar_t* Format, ...) {}
	void __cdecl LogA(const char* Format, ...) {}
	void __cdecl LogW(const wchar_t* Format, ...) {}
#endif

#ifdef __cplusplus
}
#endif