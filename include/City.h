#pragma once
#include <string>
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/string.hpp"
struct Governor;
enum ProductionType
{
	PROD_UNIT = 0,
	PROD_BUILDING,
	PROD_DISTRICT,
	PROD_WONDER,
	PROD_PROJECT
};

struct City
{
	int x;
	int y;
	int id;
	int population;
	unsigned int religion;

	std::map<unsigned int, float> yields;
	std::map<unsigned int, float> yields2;

	std::string name;
	char pretty_name[256];
	std::vector<unsigned int> currentProds;
	ProductionType currentProdType;
	std::map<unsigned int, unsigned int> prod;
	std::map<unsigned int, unsigned int> built;
	
	Governor* governor;
	Player* player;

	City()
	{
		governor = NULL;
	}


	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(x, y, id, population, religion, CEREAL_NVP(yields), CEREAL_NVP(yields2), name, CEREAL_NVP(currentProds), currentProdType, CEREAL_NVP(prod), CEREAL_NVP(built));
	}


};