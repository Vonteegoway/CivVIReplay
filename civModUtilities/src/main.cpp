#include <Windows.h>
#include "CivVILoader.h"

void LaunchViewer(HINSTANCE hInstance);

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd) {

#if 0
	const wchar_t RootFolder[] = L"D:\\Games\\Steam\\steamapps\\common\\Sid Meier's Civilization VI\\";
	const wchar_t WorkshopFolder[] = L"D:\\Games\\Steam\\steamapps\\workshop\\content\\289070\\2312050357\\";
#else
	const wchar_t RootFolder[] = L"C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\";
//	const wchar_t WorkshopFolder[] = L"C:\\Program Files (x86)\\Steam\\SteamApps\\workshop\\content\\289070\\2312050357\\";
	const wchar_t WorkshopFolder[] = L"C:\\Users\\Voda\\Documents\\My Games\\Sid Meier's Civilization VI\\Mods\\BBG\\";
#endif
	//--
	MOD_UTILITIES::Load(RootFolder, WorkshopFolder);
	LaunchViewer(hInstance);
	//--
	return 0;
}

#if 0
//int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd) {
int __cdecl main() {
	char Buffer[512];
//?CreateConnection@Database@@YAPEAVIDatabaseConnection1@1@PEBDHPEAVIDatabaseLogger@1@@Z
//?Initialize@Database@@YA_NPEAVIDatabaseMemoryAllocator@1@P6AXPEAX@Z@Z
//?Initialize@Database@@YA_NXZ
//?Shutdown@Database@@YA_NXZ

	// class Database::IDatabaseConnection1 * __ptr64 __cdecl Database::CreateConnection(char const * __ptr64,int,class Database::IDatabaseLogger * __ptr64)
	// bool __cdecl Database::Initialize(class Database::IDatabaseMemoryAllocator * __ptr64,void (__cdecl*)(void * __ptr64))
	// bool __cdecl Database::Initialize(void)
	// bool __cdecl Database::Shutdown(void)
//	HMODULE hMod = LoadLibraryA("DatabaseDLL_FinalRelease.dll");
//	DATABASE Database;
//	
//	FARPROC pmInitialize = GetProcAddress(hMod, "?Initialize@Database@@YA_NXZ");
//	(Database.*(DATABASE::INITIALIZE&)pmInitialize)();
//
//	FARPROC pmShutdown = GetProcAddress(hMod, "?Shutdown@Database@@YA_NXZ");
//	(Database.*(DATABASE::SHUTDOWN&)pmShutdown)();

	return 0;
}
#endif