
#include "CivVILoader.h"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <XML.h>
#include <string.h>
#include <Logger.h>

//struct DATABASE{
//	struct IDatabaseMemoryAllocator {
//
//	};
//
//	struct IDatabaseConnection1 {
//
//	};
//
//	struct IDatabaseLogger {
//
//	};
//
//	typedef bool (DATABASE::* __cdecl INITIALIZE)();
//	typedef bool (DATABASE::* __cdecl SHUTDOWN)();
//
//	IDatabaseConnection1* __ptr64 __cdecl CreateConnection(char const* __ptr64 pName, int, IDatabaseLogger* __ptr64 pLogger) {
//
//	}
//
//	bool __cdecl Initialize(IDatabaseMemoryAllocator* __ptr64 pAllocator, void(__cdecl*)(void* __ptr64)) {
//
//	}
//
//	bool __cdecl Initialize() {
//		return true;
//	}
//
//	bool __cdecl Shutdown() {
//
//	}
//};

// DLC: C:\Program Files (x86)\Steam\SteamApps\common\Sid Meier's Civilization VI\DLC\%Name%\Data

struct FILE_BUFFER {
	char* pFile;
	size_t Size;
	//--
	FILE_BUFFER(const wchar_t* FileName) {
		FILE* File = _wfopen(FileName, L"r");
		if (File) {
			fseek(File, 0, SEEK_END);
			Size = ftell(File);
			fseek(File, 0, SEEK_SET);
			pFile = new char[Size + 1];
			pFile[fread(pFile, 1, Size, File)] = '\0';
			fclose(File);
		} else {
			pFile = NULL;
			Size = 0;
		}
	}
	//--
	~FILE_BUFFER() {
		delete[] pFile;
	}
};
//--
//const wchar_t RootFolder[] = L"C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\";
//const wchar_t WorkshopFolder[] = L"C:\\Program Files (x86)\\Steam\\SteamApps\\workshop\\content\\289070\\";
//--
const wchar_t ExpansionNumber[] = L"2";
const size_t ExpansionNumberLength = sizeof(ExpansionNumber) / sizeof(wchar_t) - 1;
//--
void LoadSchema(const wchar_t* RootFolder) {
	wchar_t BaseDir[512];
	{
		_swprintf(BaseDir, L"%sBase\\Assets\\Gameplay\\Data\\Schema\\01_GameplaySchema.sql", RootFolder);
		//--
		FILE_BUFFER File(BaseDir);
		//--
		const char* pCur = File.pFile;
		ParseSchema(pCur);
	}
	//--
	{
		_swprintf(BaseDir, L"%sDLC\\Expansion%s\\Data\\Expansion%s_Schema.sql", RootFolder, ExpansionNumber, ExpansionNumber);
		//--
		FILE_BUFFER File(BaseDir);
		//--
		const char* pCur = File.pFile;
		ParseSchema(pCur);
	}
}

void CheckCurrent(XML_NODE& RootNode);

enum RULESET {
	RULESET_STANDARD,
	RULESET_EXPANSION_1,
	RULESET_EXPANSION_2,
};

struct STR_MODINFO {
	const char* pName;
	size_t NameLength;
};

const STR_MODINFO RULESETS[] = {
	{ "RULESET_STANDARD", ARRAYSIZE("RULESET_STANDARD") - 1 },
	{ "RULESET_EXPANSION_1", ARRAYSIZE("RULESET_EXPANSION_1") - 1 },
	{ "RULESET_EXPANSION_2", ARRAYSIZE("RULESET_EXPANSION_2") - 1 },
};
const STR_MODINFO LEADER_PLAYABLE_EXTENSION[] = {
	{ "Players:StandardPlayers::", ARRAYSIZE("Players:StandardPlayers::") - 1 },
	{ "Players:Expansion1_Players::", ARRAYSIZE("Players:Expansion1_Players::") - 1 },
	{ "Players:Expansion2_Players::", ARRAYSIZE("Players:Expansion2_Players::") - 1 },
};

const unsigned int RULESET_LENGTH = sizeof(RULESET) - 1;
const char GAMECORE[] = "Expansion2";
const unsigned int GAMECORE_LENGTH = sizeof(GAMECORE) - 1;

const char* GAMEMODEs[] = {
//	"GAMEMODE_APOCALYPSE",
	"",
};

const wchar_t* DLCs[] = {
	L"Expansion2",
	L"Expansion1",
	L"Australia",
	L"Aztec_Montezuma",
	L"GranColombia_Maya",
	L"Babylon",
	L"Byzantium_Gaul",
	L"CatherineDeMedici",
	L"Ethiopia",
	L"Indonesia_Khmer",
	L"KublaiKhan_Vietnam",
	L"Macedonia_Persia",
	L"Nubia_Amanitore",
	L"Poland_Jadwiga",
	L"Portugal",
	L"TeddyRoosevelt",
	L"VikingsLandmarks",
	L"JuliusCaesar",
	L"GreatNegotiators",
	L"GreatWarlords",
	L"RulersOfChina",
	L"RulersOfTheSahara",
	L"GreatBuilders",
	L"RulersOfEngland",
};

wchar_t* pCurrentDLC;
unsigned int DLCNameOffset;
DATA_ARRAY<CRC> CriteriaIDs;
DATA_ARRAY<CRC> LoadedMods;

void ExploreModInfo(XML_NODE& RootNode, const RULESET Ruleset) {

	XML_NODE* pMod = RootNode.GetNode("Mod"_crc32_low);
	if (pMod) {
		XML_ATTRIBUTE* pModID = pMod->Get("id"_crc32_low);
		VkAssert(pModID);
		LoadedMods.Add() = Crc32_Low(pModID->pStartContent, pModID->ContentLength);
		//--
		for (unsigned int iNode = 0; iNode < pMod->Nodes->Count; iNode++) {
			XML_NODE& ChildNode = pMod->Nodes[iNode];
			if (ChildNode.ElementID == "ActionCriteria"_crc32_low) {
				for (unsigned int iCriteria = 0; iCriteria < ChildNode.Nodes->Count; ++iCriteria) {
					XML_NODE& CriteriaNode = ChildNode.Nodes[iCriteria];
					if (CriteriaNode.ElementID == "Criteria"_crc32_low) {
						XML_ATTRIBUTE* pCriteriaID = CriteriaNode.Get("id"_crc32_low);
						VkAssert(pCriteriaID);
						//--
						bool bKeep = false;
						bool bAnyOne;
						CriteriaNode.GetAsBool("any"_crc32_low, OUT bAnyOne, false);
						//--
						for (unsigned int iCriteriaCondition = 0; iCriteriaCondition < CriteriaNode.Nodes->Count; ++iCriteriaCondition) {
							XML_NODE& CriteriaCondition = CriteriaNode.Nodes[iCriteriaCondition];
							switch (CriteriaCondition.ElementID) {
								case "RuleSetInUse"_crc32_low: {
									const char* pRuleSets = CriteriaCondition.pStartContent;
									//--
									while (true) {
										if (!_strnicmp(RULESETS[Ruleset].pName, pRuleSets, RULESETS[Ruleset].NameLength)) {
											bKeep = true;
											break;
										}
										while (*pRuleSets != ',' && *pRuleSets != '<') {
											++pRuleSets;
										}
										if (*pRuleSets == '<')
											break;
										++pRuleSets;
									}
									break;
								}
								case "GameCoreInUse"_crc32_low: {
									if (CriteriaCondition.ContentLength == GAMECORE_LENGTH && !_strnicmp(CriteriaCondition.pStartContent, GAMECORE, GAMECORE_LENGTH)) {
										bKeep = true;
									}
									break;
								}
								case "ModIsEnabled"_crc32_low: {
									CRC ModIsEnabled = Crc32_Low(CriteriaCondition.pStartContent, CriteriaCondition.ContentLength);
									if (LoadedMods.Count && LoadedMods.FindIndex(ModIsEnabled) != -1) {
										bKeep = true;
									}
									break;
								}
								case "ModInUse"_crc32_low: {
									CRC ModIsUse = Crc32_Low(CriteriaCondition.pStartContent, CriteriaCondition.ContentLength);
									if (LoadedMods.Count && LoadedMods.FindIndex(ModIsUse) != -1) {
										bKeep = true;
									}
									break;
								}
								case "ConfigurationValueMatches"_crc32_low: {
									for (unsigned int iConfiguration = 0; iConfiguration < CriteriaCondition.Nodes->Count && bKeep; ++iConfiguration) {
										XML_ATTRIBUTE& Configuration = CriteriaCondition.Nodes[iConfiguration];
										switch (Configuration.ElementID) {
											case "Group"_crc32_low: {
												CRC GroupID = Crc32_Low(Configuration.pStartContent, Configuration.ContentLength);
												if (GroupID != "Game"_crc32_low) {
													bKeep = false;
												}
												break;
											}
											case "ConfigurationId"_crc32_low: {
												bool bFound = false;
												for (int iMod = 0; iMod < (sizeof(GAMEMODEs) / sizeof(*GAMEMODEs)); ++iMod) {
													if (!_strnicmp(Configuration.pStartContent, GAMEMODEs[iMod], Configuration.ContentLength)) {
														bKeep = true;
														bFound = true;
														break;
													}
												}
												//--
												if (!bFound)
													bKeep = false;
												//--
												break;
											}
											case "Value"_crc32_low: {
												int Value = atoi(Configuration.pStartContent);
												if (!Value)
													bKeep = false;
												break;
											}
										}
										//--
										if (!bKeep)
											break;
									}
									break;
								}
								case "LeaderPlayable"_crc32_low: {
									const char* pLeaderPlayable = CriteriaCondition.pStartContent;
									//--
									while (true) {
										if (!_strnicmp(LEADER_PLAYABLE_EXTENSION[Ruleset].pName, pLeaderPlayable, LEADER_PLAYABLE_EXTENSION[Ruleset].NameLength)) {
											bKeep = true;
											break;
										}
										while (*pLeaderPlayable != ',' && *pLeaderPlayable != '<') {
											++pLeaderPlayable;
										}
										if (*pLeaderPlayable == '<')
											break;
										++pLeaderPlayable;
									}
									break;
								}
								default: {
									continue;
									break;
								}
							}
							//--
							if (!bKeep && !bAnyOne || bKeep && bAnyOne)
								break;
						}
						//--
						if (bKeep) {
							CriteriaIDs.Add() = Crc32_Low(pCriteriaID->pStartContent, pCriteriaID->ContentLength);
						}
					}
				}
			} else if (ChildNode.ElementID == "InGameActions"_crc32_low) {
				for (unsigned int iAction = 0; iAction < ChildNode.Nodes->Count; ++iAction) {
					XML_NODE& ActionNode = ChildNode.Nodes[iAction];
					switch (ActionNode.ElementID) {
						case "UpdateDatabase"_crc32_low: {
							XML_ATTRIBUTE* pCriteria = ActionNode.Get("criteria"_crc32_low);
							//--
							if (!pCriteria || CriteriaIDs.FindIndex(Crc32_Low(pCriteria->pStartContent, pCriteria->ContentLength)) != -1) {
								for (unsigned int iFile = 0; iFile < ActionNode.Nodes->Count; ++iFile) {
									XML_NODE& FileNode = ActionNode.Nodes[iFile];
									if (FileNode.ElementID == "File"_crc32_low) {
										if (!_strnicmp(FileNode.pStartContent + FileNode.ContentLength - 4, ".xml", 3)) {
											_swprintf(pCurrentDLC + DLCNameOffset, L"%.*S", FileNode.ContentLength, FileNode.pStartContent);
											//--
											LogW(L"[Civ6ModUtilities]	Parsing %.*S\n", FileNode.ContentLength, FileNode.pStartContent);
											//--
											FILE_BUFFER File(pCurrentDLC);
											//--
											if (File.Size) {
												const char* pCur = File.pFile;
												XML_NODE RootNode;
												StartParseXml(RootNode, pCur);
												CheckCurrent(RootNode);
											}
										} else if (!_strnicmp(FileNode.pStartContent + FileNode.ContentLength - 4, ".sql", 3)) {
											_swprintf(pCurrentDLC + DLCNameOffset, L"%.*S", FileNode.ContentLength, FileNode.pStartContent);
											//--
											LogW(L"[Civ6ModUtilities]	Parsing %.*S\n", FileNode.ContentLength, FileNode.pStartContent);
											//--
											FILE_BUFFER File(pCurrentDLC);
											//--
											if (File.Size) {
												const char* pCur = File.pFile;
												ParseBBG(pCur);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void LoadCivBaseState(const wchar_t* RootFolder) {
	wchar_t BaseDir[512];
	//--
	{
		LogW(L"[Civ6ModUtilities]Exploring BaseGame\n");
		//--
		int Offset = _swprintf(BaseDir, L"%sBase\\Assets\\Gameplay\\Data\\", RootFolder);
		_swprintf(BaseDir + Offset, L"*.xml");
		//--
		WIN32_FIND_DATAW FindData;
		HANDLE hFind = FindFirstFileW(BaseDir, &FindData);
		//--
		if (hFind == INVALID_HANDLE_VALUE) {
			VkBreak();
		}
		//--
		do {
			if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				_swprintf(BaseDir + Offset, L"%s", FindData.cFileName);
				//--
				LogW(L"[Civ6ModUtilities]	Parsing %s\n", FindData.cFileName);
				//--
				FILE_BUFFER File(BaseDir);
				//--
				const char* pCur = File.pFile;
				XML_NODE RootNode;
				StartParseXml(RootNode, pCur);
				CheckCurrent(RootNode);
			}
		} while (FindNextFileW(hFind, &FindData) != 0);
		//--
		FindClose(hFind);
	}
	//--
	for (int iDLC = 0; iDLC < (sizeof(DLCs) / sizeof(*DLCs)); ++iDLC) {
		unsigned int Offset = _swprintf(BaseDir, L"%sDLC\\%s\\", RootFolder, DLCs[iDLC]);
		_swprintf(BaseDir + Offset, L"*.modinfo");
		//--
		WIN32_FIND_DATAW FindData;
		HANDLE hFind = FindFirstFileW(BaseDir, &FindData);
		//--
		if (hFind == INVALID_HANDLE_VALUE)
			continue;
		//--
		FindClose(hFind);
		//--
		_swprintf(BaseDir + Offset, L"%s", FindData.cFileName);
		//--
		LogW(L"[Civ6ModUtilities]Exploring DLC %s\n", FindData.cFileName);
		//--
		FILE_BUFFER File(BaseDir);
		//--
		pCurrentDLC = BaseDir;
		DLCNameOffset = Offset;
		CriteriaIDs.RemoveAll();
		//--
		const char* pCur = File.pFile;
		XML_NODE RootNode;
		StartParseXml(RootNode, pCur);
		ExploreModInfo(RootNode, RULESET_EXPANSION_2);
		//LoadCivBaseState_DLCFolder(FindDataDLCFolder, BaseDir, Offset);
	}
}

bool LoadMod(const wchar_t* Folder, const wchar_t* ModName) {
	wchar_t BaseDir[512];
	int Offset = _swprintf(BaseDir, L"%s", Folder);
	//--
	_swprintf(BaseDir + Offset, L"%s.modinfo", ModName);
	//--
	FILE_BUFFER File(BaseDir);
	if (!File.pFile)
		return false;
	//--
	pCurrentDLC = BaseDir;
	DLCNameOffset = Offset;
	CriteriaIDs.RemoveAll();
	//--
	const char* pCur = File.pFile;
	XML_NODE RootNode;
	StartParseXml(RootNode, pCur);
	ExploreModInfo(RootNode, RULESET_EXPANSION_2);
	return true;
}

void LoadBLPAssets(const wchar_t* RootFolder) {
	wchar_t BaseDir[512];
	//--
	{
		int Offset = _swprintf(BaseDir, L"%sBase\\Platforms\\Windows\\BLPs\\leaders\\", RootFolder);
		_swprintf(BaseDir + Offset, L"*.blp");
		//--
		WIN32_FIND_DATAW FindData;
		HANDLE hFind = FindFirstFileW(BaseDir, &FindData);
		//--
		if (hFind == INVALID_HANDLE_VALUE) {
			VkBreak();
		}
		//--
		do {
			if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				_swprintf(BaseDir + Offset, L"%s", FindData.cFileName);
				FILE_BUFFER File(BaseDir);
				//--
				const char* pCur = File.pFile;
				ParseBlp(pCur);
			}
		} while (FindNextFileW(hFind, &FindData) != 0);
		//--
		FindClose(hFind);
	}
}

#if 0
void ExportTable(TABLE& Table)
{
	std::ofstream myfile;
	myfile.open("output.txt");
	for (unsigned int iInstance = 0; iInstance < Table.Instances->Count; ++iInstance) {
		TABLE::INSTANCE& Instance = *Table.Instances[iInstance];
		for (unsigned int iValue = 0; iValue < Instance.Values->Count; ++iValue) {
			const TABLE::DESCRIPTOR& Descriptor = *Table.Descriptors[iValue];
			TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];
			switch (Descriptor.Type) {
			case TABLE::DESCRIPTOR::TEXT: {
				myfile << std::hex << ~Crc32(Value.szValue) << "; //" << Value.szValue << std::endl;
				break;
			}
			}
		}
	}
	myfile.close();
}
#endif

void LuaCall_GenerateMap(const char* FileName);

namespace MOD_UTILITIES {
	void Load(const wchar_t* RootFolder, const wchar_t* WorkshopFolder) {
		//LoadBLPAssets(RootFolder);

#if 1
		LogA("[Civ6ModUtilities]Parsing Schemas\n");
		LoadSchema(RootFolder);
		PreConnectGameplay();
		LogA("[Civ6ModUtilities]Parsing Civ BaseState\n");
		LoadCivBaseState(RootFolder);
		//--
		LogA("[Civ6ModUtilities]Parsing BBG\n");
		wchar_t BBGFolder[256] = L"";
		_swprintf(BBGFolder, L"%s\\2865001760\\", WorkshopFolder);
		bool loadedBBG = LoadMod(BBGFolder, L"BetterBalancedGame");
		wchar_t BBGExpandedFolder[256] = L"";
		_swprintf(BBGExpandedFolder, L"%s\\3533091092\\", WorkshopFolder);
		bool loadedBBGExpanded = LoadMod(BBGExpandedFolder, L"BBGExpanded");
		wchar_t BBMFolder[256] = L"";
		_swprintf(BBMFolder, L"%s\\3179425402\\", WorkshopFolder);
		bool loadedBBM = LoadMod(BBMFolder, L"BBM_137");
		if (loadedBBG && loadedBBGExpanded && loadedBBM)
		{
			LogA("[Civ6ModUtilities]Applying BBG\n");
			UpdateInstances();
		}
		//--
		LogA("[Civ6ModUtilities]Connecting Gameplay\n");
		ConnectGameplay();
#endif
		//--

		//LuaCall_GenerateMap("C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\DLC\\Byzantium_Gaul\\Maps\\Highlands_XP2.lua");
		//ExportTable(*pTableTypes);
	}
}
