#pragma once
#include <vector>
#include "PlayerInfo.h"
#include "GoodyHut.h"
#include "DistrictStat.h"
#include "HardTech.h"
#include <mutex>
#include <queue>
#include "Asset.h"
#include "cereal/types/vector.hpp"

struct CivVIState;
struct CivVIGame
{
	CivVIGame();

	PlayerInfo playersInfo[64];
	int begin; // avoid first spectators
	int seed;
	int nbPlayer;
	std::vector<std::vector<int>> playersByTeams;

	//GameInfo
	char gamespeed[256];
	char mapSize[256];
	char map[4096];
	uint64_t timestamp;
	bool multiPlayer;

	//Stats
	std::vector<GoodyHut> huts;
	std::vector<HardTech> hardTechs;
	std::vector<HardTech> hardDogmes;
	std::vector<std::vector<DistrictStat>> districtStats;
	std::map<unsigned int, unsigned int> terrainStats;
	std::map<unsigned int, unsigned int> featureStats;
	std::map<unsigned int, unsigned int> resourceStats;

	void UpdateStats(CivVIState& previousState, CivVIState& state, Assets& assets);
	void UpdateMapStats(CivVIState& firstState);

	template<class Archive>
	void save(Archive& archive) const
	{
		archive(seed, playersInfo, begin, nbPlayer, gamespeed, mapSize, map, timestamp, multiPlayer);
		archive((int)playersByTeams.size());
		for (int i = 0; i < playersByTeams.size(); i++)
		{
			archive(playersByTeams[i]);
		}
	}

	template<class Archive>
	void load(Archive& archive)
	{
		archive(seed, playersInfo, begin, nbPlayer, gamespeed, mapSize, map, timestamp, multiPlayer);
		int size;
		archive(size);
		playersByTeams.resize(size);
		for (int i = 0; i < playersByTeams.size(); i++)
		{
			archive(playersByTeams[i]);
		}
	}
};

