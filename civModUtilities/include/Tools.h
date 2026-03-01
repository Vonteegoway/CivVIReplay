#pragma once

#pragma comment(linker, "/nodefaultlib:msvcrt.lib")
#pragma comment(linker, "/nodefaultlib:msvcrtd.lib")
#pragma comment(linker, "/nodefaultlib:msvcprt.lib")
#pragma comment(linker, "/nodefaultlib:msvcprtd.lib")

#include <malloc.h>
#include <intrin.h>
#include <new.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include <ctype.h>

#define OUT
#define IN_OUT

#define NULL 0

#pragma warning(disable:26495)
#pragma warning(disable:26451)
#pragma warning(disable:26812)

#ifdef _DEBUG
#define VkBreak() __debugbreak()
#define VkAssert(c) (void)((!!(c)) || (VkBreak(), 0))
#else
#pragma warning(disable:4390)
#define VkBreak()
#define VkAssert(c) 
#endif

#define CT_ASSERT(x) extern int __dummy[(int)x]

template <typename TYPE, size_t SIZE> constexpr __forceinline unsigned int ArraySize(TYPE (&Array)[SIZE]) {
	return (unsigned int)SIZE;
}

inline void* malloc_s(size_t Size) {
#ifdef _DEBUG
	void* pAlloc = malloc(Size);
	VkAssert(pAlloc);
	return pAlloc;
#else
	return malloc(Size);
#endif
}

inline void* realloc_s(void* _pAlloc, size_t Size) {
#ifdef _DEBUG
	VkAssert(_pAlloc);
	void* pAlloc = realloc(_pAlloc, Size);
	VkAssert(pAlloc);
	return pAlloc;
#else
	return realloc(_pAlloc, Size);
#endif
}

inline void free_s(void* _pAlloc) {
#ifdef _DEBUG
	VkAssert(_pAlloc);
	free(_pAlloc);
#else
	free(_pAlloc);
#endif
}

template<typename TYPE, bool bAutomaticConstructionDestruction> inline void _Create(IN_OUT TYPE*& Datas, IN_OUT unsigned int& Size, const unsigned int InitialSize) {
	VkAssert(Size == 0);
	Size += InitialSize;
	Datas = (TYPE*)malloc_s(Size * sizeof(TYPE));
	//--
	if (bAutomaticConstructionDestruction) {
		new(&Datas[0]) TYPE[Size];
	}
}

template<typename TYPE, unsigned int GROW_FACTOR_N, unsigned int GROW_FACTOR_D, bool bAutomaticConstructionDestruction> inline void _Grow(IN_OUT TYPE*& Datas, IN_OUT unsigned int& Size) {
	VkAssert(Size != 0);
	//--
	int PreviousSize = Size;
	//--
	Size = (Size * GROW_FACTOR_N) / GROW_FACTOR_D + 1;
	Datas = (TYPE*)realloc_s(Datas, Size * sizeof(TYPE));
	//--
	if (bAutomaticConstructionDestruction) {
		new(&Datas[PreviousSize]) TYPE[size_t(Size) - PreviousSize];
	}
}

template<typename TYPE, bool bAutomaticConstructionDestruction> inline void _Resize(IN_OUT TYPE*& Datas, const unsigned int PreviousSize, const unsigned int& Size) {
	VkAssert(Size != 0);
	//--
	if (PreviousSize < Size) {
		Datas = (TYPE*)realloc_s(Datas, Size * sizeof(TYPE));
		//--
		if (bAutomaticConstructionDestruction) {
			new(&Datas[PreviousSize]) TYPE[Size - PreviousSize];
		}
	}
	else {
		if (bAutomaticConstructionDestruction) {
			delete[] (Datas, &Datas[Size]);
		}
		//--
		Datas = (TYPE*)realloc_s(Datas, Size * sizeof(TYPE));
	}
}

template<typename TYPE, bool bAutomaticConstructionDestruction> inline void _Destroy(IN_OUT TYPE*& Datas, IN_OUT unsigned int& Size) {
	if (bAutomaticConstructionDestruction) {
		delete[] (Datas, &Datas[0]);
	}
	//--
	free_s(Datas);
	Size = 0;
}

struct NON_COPYABLE {
	NON_COPYABLE() {}
private:
	NON_COPYABLE(const NON_COPYABLE& Other) { VkBreak(); }
	NON_COPYABLE& operator =(NON_COPYABLE& Other) { VkBreak(); return *this; }
};

template <typename TYPE> TYPE Abs(TYPE Value) {
	return Value < 0 ? -Value : Value;
}

template <typename TYPE> void Swap(TYPE& One, TYPE& Two) {
	TYPE _ = Two;
	Two = One;
	One = _;
}

template <typename T_CHAR, size_t T_SIZE> inline size_t saprintf(T_CHAR(&OutBuffer)[T_SIZE], const T_CHAR* Format, ...) {
	va_list va;
	va_start(va, Format);
	VkBreak();
	va_end(va);
	return 0;
}

template <size_t T_SIZE> inline size_t saprintf(char(&OutBuffer)[T_SIZE], const char* Format, ...) {
	va_list va;
	va_start(va, Format);
	size_t Len = vsnprintf(OutBuffer, T_SIZE, Format, va);
	va_end(va);
	return Len;
}

template <size_t T_SIZE> inline size_t saprintf(wchar_t(&OutBuffer)[T_SIZE], const wchar_t* Format, ...) {
	va_list va;
	va_start(va, Format);
	size_t Len = vswprintf(OutBuffer, T_SIZE, Format, va);
	va_end(va);
	return Len;
}

template <typename T_CHAR> inline size_t seprintf(T_CHAR* OutBuffer, const T_CHAR* OutBufferEnd, const T_CHAR* Format, ...) {
	va_list va;
	va_start(va, Format);
	VkBreak();
	va_end(va);
	return 0;
}

template <> inline size_t seprintf(char* OutBuffer, const char* OutBufferEnd, const char* Format, ...) {
	va_list va;
	va_start(va, Format);
	size_t Len = vsnprintf(OutBuffer, OutBufferEnd - OutBuffer, Format, va);
	va_end(va);
	return Len;
}

template <> inline size_t seprintf(wchar_t* OutBuffer, const wchar_t* OutBufferEnd, const wchar_t* Format, ...) {
	va_list va;
	va_start(va, Format);
	size_t Len = vswprintf(OutBuffer, OutBufferEnd - OutBuffer, Format, va);
	va_end(va);
	return Len;
}

inline size_t ToUpper(const char* szInput, OUT char* szOutput) {
	int Length = 0;
	while (szInput[Length]) {
		szOutput[Length] = toupper(szInput[Length]);
		++Length;
	}
	szOutput[Length] = '\0';
	return Length;
}