#pragma once
#include "Unit.h"
#include "City.h"
#include "District.h"
#include "Improvement.h"
#include "Governor.h"
#include "StrategicResource.h"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"

enum AgeState
{
	DARK,
	NORMAL,
	GOLDEN,
	HEROIC
};

enum TechDogmeType
{
	TECH = 0,
	DOGME
};

struct TechDogmeInfo
{
	std::map<unsigned int, unsigned int> turnTo;
	std::map<unsigned int, float> research;
	std::map<unsigned int, bool>  found;
	std::map<unsigned int, bool>  boost;
	std::vector<unsigned int>	  current;
	int researchCount;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(CEREAL_NVP(turnTo), CEREAL_NVP(research), CEREAL_NVP(found), CEREAL_NVP(boost), CEREAL_NVP(current));
	}
};

struct Player
{
	std::vector<Unit> units;
	std::map<unsigned int, UnitInfo> unitsInfo;
	std::map<unsigned int, unsigned int> unitsCount;
	std::vector<City> cities;
	std::vector<City*> pCities;
	std::vector<District> districts;
	std::vector<Improvement> improvements;
	std::vector<Governor> governors;

	std::map<unsigned int, float> yields;
	std::map<unsigned int, float> totalYields;

	TechDogmeInfo techDogmeInfo[2]; //0 tech / 1 civic

	int totalTourismByPlayer[64];
	int touristUnknown[64];
	int touristByPlayer[64];
	int externalTourist;
	int internalTourist;
	int counterTourist;

	int availableRoads;
	int unknownRoads;

	std::map<unsigned int, float> greatPeopleProd;
	std::map<unsigned int, float> greatPeopleCurrent;
	std::map<unsigned int, float> greatPeopleUnknown;

	std::vector<Resource> strats;

	int gold;
	float maintenance;
	int faith;
	int diploFavor;
	int diploPoint;

	unsigned int pantheon;
	unsigned int totalPop;
	unsigned int militaryStrength;

	unsigned int government;
	unsigned int lastTurnChangeGovernment;
	std::vector<unsigned int>	  policies[4];

	int iPlayer;

	std::vector<unsigned int>	  visible;
	std::vector<unsigned int>	  inSightCount;
	std::vector<unsigned int>	  improvment;
	std::vector<unsigned int>	  resource;
	std::vector<unsigned int>	  e;
	std::vector<unsigned int>	  inSightUnitCount;


	std::vector<unsigned int>	  emissaries; // for city states
	unsigned int				  maxEmissary;

	AgeState					  ageState;
	unsigned int				  eraPoint;
	unsigned int				  eraPointInitEra;

	unsigned int				  eraPointNormal;
	unsigned int				  eraPointHeroic;
	int							  eraPointModifiers;
	std::vector<unsigned int>	  commemorations;

	std::map<unsigned int, unsigned int> goodyHuts;

	unsigned int				  unitCount;
	unsigned int				  unitLost;
	unsigned int				  unitUpgraded;
	unsigned int				  unitStar;
	
	unsigned int				  builderBuilt;
	unsigned int				  settlerBuilt;
	unsigned int				  tileWorked;
	unsigned int				  freeTile;

	Player()
	{
		maxEmissary = 0;
		totalPop = 0;
		militaryStrength = 0;
		iPlayer = -1;
		ageState = NORMAL;
		externalTourist = 0;
		counterTourist = 0;
		unitLost = 0;
		unitUpgraded = 0;
		builderBuilt = 0;
		settlerBuilt = 0;
		tileWorked = 0;
		freeTile = 0;
	}

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(totalTourismByPlayer, touristUnknown, touristByPlayer, externalTourist, internalTourist, counterTourist, CEREAL_NVP(greatPeopleProd), CEREAL_NVP(greatPeopleCurrent), CEREAL_NVP(greatPeopleUnknown), CEREAL_NVP(strats), techDogmeInfo[0], techDogmeInfo[1],
			availableRoads, unknownRoads, CEREAL_NVP(units), CEREAL_NVP(unitsInfo), CEREAL_NVP(unitsCount), CEREAL_NVP(cities), CEREAL_NVP(districts), CEREAL_NVP(improvements), CEREAL_NVP(governors), gold, maintenance, faith, diploFavor, diploPoint, 
			pantheon, government, lastTurnChangeGovernment, policies, iPlayer, CEREAL_NVP(visible), CEREAL_NVP(inSightCount), CEREAL_NVP(improvment), CEREAL_NVP(resource), CEREAL_NVP(e), CEREAL_NVP(inSightUnitCount), CEREAL_NVP(emissaries), ageState, eraPoint, eraPointInitEra, eraPointNormal,
			eraPointHeroic, eraPointModifiers, CEREAL_NVP(commemorations), CEREAL_NVP(goodyHuts));
	}
};

struct Team
{
	std::vector<unsigned int>	  visible;
	std::vector<unsigned int>	  inSightCount;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(CEREAL_NVP(visible), CEREAL_NVP(inSightCount));
	}
};