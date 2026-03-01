#pragma once
#include <vector>
#include <string>
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"

struct Religion
{
	int iPlayer;
	int iSymbol;
	std::vector<unsigned int> beliefs;
	std::vector<unsigned int> buildings;
	std::vector<unsigned int> units;
	std::string name;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(iPlayer, iSymbol, CEREAL_NVP(beliefs), CEREAL_NVP(buildings), CEREAL_NVP(units), name);
	}
};