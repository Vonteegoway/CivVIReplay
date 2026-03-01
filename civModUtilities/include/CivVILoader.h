#pragma once
#include "Types.h"

void ParseBBG(IN_OUT const char*& pCur);
void ParseSchema(IN_OUT const char*& pCur);
void PreConnectGameplay();
void UpdateInstances();
void ConnectGameplay();
void ParseBlp(IN_OUT const char*& pCur);

namespace MOD_UTILITIES {
	extern "C" __declspec(dllexport) void Load(const wchar_t* RootFolder, const wchar_t* WorkshopFolder);
}
