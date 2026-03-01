#include <Windows.h>
#include "Viewer.h"
#include <thread>
#include <Dbghelp.h>
#include <tchar.h>
#include "Asset.h"

SaveLoader loader;

typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

void create_minidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
    HMODULE mhLib = ::LoadLibrary(_T("dbghelp.dll"));
    MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");

    HANDLE  hFile = ::CreateFile(_T("core.dmp"), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);

    _MINIDUMP_EXCEPTION_INFORMATION ExInfo;
    ExInfo.ThreadId = ::GetCurrentThreadId();
    ExInfo.ExceptionPointers = apExceptionInfo;
    ExInfo.ClientPointers = FALSE;

    const DWORD Flags = MiniDumpNormal | 
        MiniDumpWithFullMemoryInfo |
        MiniDumpWithHandleData |
        MiniDumpWithUnloadedModules |
        MiniDumpWithThreadInfo;

    pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)Flags, &ExInfo, NULL, NULL);
    ::CloseHandle(hFile);
}

LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
    create_minidump(apExceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd) {

    char Buffer[MAX_PATH];
    DWORD Length = GetTempPathA(MAX_PATH, Buffer);
    SetUnhandledExceptionFilter(unhandled_handler);

    loader.init();

	std::thread first(&SaveLoader::LoadSave, &loader, false);
    Viewer v(hInstance, 1280, 720);
    loader.stop = true;
	first.join();
	return 0;
}
