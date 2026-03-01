#pragma once
#include "cereal/types/map.hpp"
struct Terrain;
struct City;
struct Improvement; 
struct Unit;
struct District;

struct Tile
{
	Tile()
	{
		pCity = NULL;
		pDistrict = NULL;
		iPlayer = -1;
		iLine = 0;
		wonder = 0xffffffff;
	}
	unsigned int iTile;
	unsigned int landmass;
	unsigned int terrain;
	unsigned int feature;
	unsigned int resource;
	unsigned int resourceCount;
	unsigned int improvment;
	unsigned int road;
	unsigned int roadLvl;
	unsigned int continent;
	int16_t appeal;
	unsigned int river;
	unsigned int riverCount;
	unsigned int cliffbitmap;
	unsigned int bOverlay;
	unsigned int group[2];
	unsigned int wonder;
	unsigned int pillage;
	//--
	City* pCity;
	std::vector<Unit*> units;
	District* pDistrict;
	//--
	//BELONGING
	unsigned short iPlayer;
	unsigned short iCity;
	unsigned short iDistrict;
	//--
	std::map<unsigned int, unsigned int> disaster;
	//COMPUTED
	std::map<unsigned int, unsigned int> yields;
	unsigned short iLine;

	bool hasStrategicResource() const
	{
		return resource == 0xe3e70273 || resource == 0x64ec0edd || resource == 0x9353a548 || resource == 0x6f229238 || resource == 0x6bb73300 || resource == 0xa6573f0a || resource == 0x13cb7793;
	}


	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(iTile, landmass, terrain, feature, resource, resourceCount, improvment, road, roadLvl, continent, appeal, river, riverCount, cliffbitmap, bOverlay, group[2], wonder, pillage, iPlayer, iCity, iDistrict, CEREAL_NVP(disaster));
	}

};