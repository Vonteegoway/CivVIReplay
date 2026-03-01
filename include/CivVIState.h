#pragma once
#include "CivVIParser.h"
#include "CivVIGame.h"
#include "Religion.h"
#include "GreatPeople.h"
#include "cereal/types/vector.hpp"

struct CivVIState
{
public:
	CivVIState()
	{
		width = 44;
		turn = 0;
	}
	std::vector<Tile> tiles;
	std::vector<Player> players;
	std::vector<Team> teams;
	std::vector<Religion> religions;

	std::vector<GreatPeople> greatPeople;
	std::vector<GreatPeople> greatPeopleRecruted;

	std::vector<int> missingGolds;//by team


	int turn;
	int width;
	CivVIGame* pGame;
	void Parse(CivVIParser& Parser, const CivVIGame& Game, Assets& assets);
	void UpdatePointer(const CivVIGame& Game);
	void ComputeDistrictProxi(const CivVIGame& Game, Assets& assets);
	void ComputeTileYields(Assets& assets);
	void ComputeCityLine();

	uint64_t timestamp;
	std::string error;

	template<class Archive>
	void save(Archive& archive) const
	{
		archive(width, turn, pGame->seed, CEREAL_NVP(tiles), CEREAL_NVP(players), CEREAL_NVP(religions), CEREAL_NVP(greatPeople), CEREAL_NVP(greatPeopleRecruted));
	}

	template<class Archive>
	void load(Archive& archive)
	{
		int iGame;
		archive(width, turn, iGame, CEREAL_NVP(tiles), CEREAL_NVP(players), CEREAL_NVP(religions), CEREAL_NVP(greatPeople), CEREAL_NVP(greatPeopleRecruted));
		pGame = (CivVIGame*)iGame;
	}
};