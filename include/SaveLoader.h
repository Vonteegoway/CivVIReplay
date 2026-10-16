#pragma once
#include <atomic>
#include <cstdio>
#include <mutex>
#include <cstdint>
#include <queue>
#include "CivVIState.h"
#include "CivicTech.h"
#include "Asset.h"
#include <filesystem>
namespace fs = std::filesystem;
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"

struct SaveLoader {
	template<class Archive>
	void save(Archive& archive) const
	{
		archive(loadedStateCount, CEREAL_NVP(States), CEREAL_NVP(Games));
	}

	template<class Archive>
	void load(Archive& archive)
	{
		archive(loadedStateCount, CEREAL_NVP(States), CEREAL_NVP(Games));

		for (int iState = 0; iState < States.size(); iState++)
		{
			CivVIGame& Game = Games[(int)(intptr_t)States[iState].pGame];
			CivVIState& State = States[iState];

			State.pGame = &Game;
			State.UpdatePointer(Game);
			State.ComputeDistrictProxi(Game, assets);
			State.ComputeCityLine();
			State.ComputeTileYields(assets);
		}
		for (int iState = 1; iState < States.size(); iState++)
		{
			States[iState].pGame->UpdateStats(States[iState - 1], States[iState], assets);
		}
	}

	void init();

	int loadedStateCount;
	bool stop;
	bool endLoadCivDb;
	bool busy = false; // a save is being parsed (guarded by mtx)

	const std::string defaultCivPath;
	const std::string defaultBBGPath;
	fs::path civPath = "C:/Program Files (x86)/Steam/SteamApps/common/Sid Meier's Civilization VI/";
	fs::path bbgPath = "C:/Program Files (x86)/Steam/SteamApps/workshop/content/289070/";
	bool steamInstall = false;
	bool waitForCivPaths = true; // false: parse saves even when the Civ VI install / BBG folders are unknown
	bool printPaths = true;      // print each save path as it is parsed (the self-test prints its own)
	// Set when the loader thread cannot continue; shown by the UI. Written once by the loader
	// thread before hasFatalError is raised, so readers check the flag first.
	std::string fatalError;
	std::atomic<bool> hasFatalError{ false };
	void setFatalError(const std::string& message)
	{
		fatalError = message;
		fprintf(stderr, "%s\n", message.c_str());
		hasFatalError = true;
	}
	bool isValidBBGPath();
	bool isValidCivPath();
	fs::path RootFolder;
	fs::path WorkshopFolder;

	std::mutex mtx;
	std::queue<std::string> paths;

	std::vector<CivVIState> States;
	std::map<int, CivVIGame> Games;

	void SetupTreesManually();
	std::map<unsigned int, CivicTech> techTree2;
	std::map<unsigned int, CivicTech> civicTree2;

	Assets assets;

	void LoadSave_WithoutAsset(bool untilEnd);
	void LoadSave(bool untilEnd = false);
	void Close();

	void findCivPath();


#ifdef _DEBUG
	CivVIParser ParserCpy;
#endif
};