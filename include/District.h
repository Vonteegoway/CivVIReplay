#pragma once
#include "Tile.h"
#include "PlayerInfo.h"
#include "Asset.h"
#include "Improvement.h"

#include "cereal/types/map.hpp"

struct District
{
	int x;
	int y;
	unsigned int type;
	int globalId;
	int id;
	int damage;
	int wallDamage;
	int wall;
	int cost;
	int built;
	int cityId;
	bool pillage;

	std::map<std::string, std::pair<int,float>> proxi; //description / count / value
    int totalProxi;
	int yieldType;
	bool bRiver;

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(x, y, id, type, globalId, damage, wallDamage, wall, cost, built, cityId, pillage);
    }

	District()
	{
		pillage = false;
		yieldType = 0;
		bRiver = false;
        wall = 0;
        wallDamage = 0;
        damage = 0;
	}

	void addProxi(float addProxi, std::string type)
	{
        if (proxi.find(type) != proxi.end())
        {
            proxi[type].first++;
        }
        else
        {
            proxi[type] = std::make_pair(1, addProxi);
        }
	}

    static const Tile* getDirectNeighbours(int i, int x, int y, const std::vector<Tile>& tiles, int width)//without me 
    {
        std::vector<int> offsets = y % 2 == 0 ? std::vector<int>{-1, -1, -1, 0, 0, -1, 0, +1, +1, -1, +1, 0} : std::vector<int>{ -1, 0, -1, +1, 0, -1, 0, +1, +1, 0, +1, +1 };

        int heigth = tiles.size() / width;
        int nextX = x + offsets[i * 2 + 1];
        int nextY = y + offsets[i * 2];

        if (nextX >= 0 && nextX < width && nextY >= 0 && nextY < heigth)
        {
            return &tiles[nextX + nextY * width];
        }

        return NULL;
    }

    static bool isNaturalWonder(unsigned int type, const Assets& assets)
    {
        if (type == 0xFFFFFFFF)
            return false;

        if (assets.assets.find(type) == assets.assets.end())
            return false;

        for (unsigned int iValue = 0; iValue < assets[type].dbValue->Childs->Count; ++iValue) {
            TABLE::INSTANCE& Instance = *assets[type].dbValue->Childs[iValue];
            if (!strcmp(Instance.pTable->Name, "Features"))
            {
                for (unsigned int iValue = 0; iValue < Instance.Values->Count; ++iValue) {

                    const TABLE::DESCRIPTOR& Descriptor = *Instance.pTable->Descriptors[iValue];
                    TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];
                    if (!strcmp(Descriptor.Name, "NaturalWonder"))
                    {
                        return Value.bValue;
                    }
                }
                break;
            }
        }

        return false;
    }

    static int specificDistrict(int type, PlayerInfo& pInfo)
    {

        if (type == 0x71d9cd60) // campus
        {
            if (pInfo.leader == 0xcea3b6d4)
                return 0x4d77d2bc; //observatory
            if (pInfo.leader == 0xa6cca53a)
                return 0xfbe22137; //seowon
        }
        else if (type == 0x32b15d10) //holy site
        {
            if (pInfo.leader == 0xca547f73)
                return 0xd28784a8; //lavra
        }
        else if (type == 0xbe8c19bb)  //commercial hub
        {
            if (pInfo.leader == 0xca547f73)
                return 0x7b5c15e5; //suguba
        }
        else if (type == 0xcbecc739)  //harbor
        {
            if (pInfo.leader == 0xa7b274b9 || pInfo.leader == 0xcf9f21e)
                return 0x57490d08; //royal navy
            if (pInfo.leader == 0xa0fad691)
                return 0x22e60fde; //cothon
        }
        else if (type == 0x9ac880cb)  //ZI
        {
            if (pInfo.leader == 0x36fe8dfe)
                return 0x2c29e1e1; //hansa
            if (pInfo.leader == 0xb98fc669)
                return 0xb45b1b15; //oppidum
        }
        else if (type == 0x885fdf1e)  //theatre
        {
            if (pInfo.leader == 0xd37275c3 || pInfo.leader == 0x5eae1c66)
                return 0xbe3f9c41; //acropolis
        }
        else if (type == 0x66db35b9)  //encampment
        {
            if (pInfo.leader == 0xf42613d0)
                return 0x79dd476d; //thanh
        }
        return type;
    }

    static void computeNeighborProxi(int x, int y, std::vector<Tile> tiles, int width, PlayerInfo& pInfo, const Assets& assets)
    {
        const Tile& current = tiles[x + y * width];

        for (int iNeighbor = 0; iNeighbor < 6; iNeighbor++)
        {
            const Tile* neighbor = getDirectNeighbours(iNeighbor, x, y, tiles, width);
            if (!neighbor)
                continue;
            District* pDistrict = tiles[neighbor->iTile].pDistrict;
            if (pDistrict)
                pDistrict->computeProxi(tiles, width, pInfo, assets);
        }
    }

	void computeProxi(std::vector<Tile> tiles, int width, PlayerInfo& pInfo, const Assets& assets)
    {
        District& d = *this;
        d.totalProxi = 0;
        d.proxi.clear();

        const Tile& current = tiles[d.x + d.y * width];

        if (d.type == 0xfbe22137) //seowon initial
            d.addProxi(2.f, "Initial Seowon value");
        if (pInfo.leader == 0xa10d0331 && (current.appeal == 2 || current.appeal == 3)) // australia
            d.addProxi(1.f, "Charming appeal");
        if (pInfo.leader == 0xa10d0331 && (current.appeal >= 4))
            d.addProxi(3.f, "Breathtaking appeal");

        if (d.type == 0x71d9cd60 || d.type == 0x4d77d2bc || d.type == 0xfbe22137) // campus
            d.yieldType = 0x3a81f8ab;
        else if (d.type == 0x32b15d10 || d.type == 0xd28784a8)  //holy site / lavra
            d.yieldType = 0x7f0f8e76;
        else if (d.type == 0xbe8c19bb || d.type == 0x782329e8)  //commercial hub / suguba
            d.yieldType = 0xb761634a;
        else if (d.type == 0x885fdf1e || d.type == 0xbe3f9c41)  //theatre
            d.yieldType = 0xeb3f55e8;
        else if (d.type == 0xcbecc739 || d.type == 0x57490d08 || d.type == 0x22e60fde)  //harbor // royal navy // cothon
            d.yieldType = 0xb761634a;
        else if (d.type == 0x9ac880cb || d.type == 0x2c29e1e1 || d.type == 0xb45b1b15)  //ZI
            d.yieldType = 0xbb19ce0a;
        else if (d.type == 0x79dd476d) //thanh
            d.yieldType = 0xeb3f55e8;
        else
            d.yieldType = 0;
        
        if(d.yieldType != 0)
        {
            for (int iNeighbor = 0; iNeighbor < 6; iNeighbor++)
            {
                const Tile* neighbor = getDirectNeighbours(iNeighbor, d.x, d.y, tiles, width);
                if (!neighbor)
                    continue;

                District* pDistrict = tiles[neighbor->iTile].pDistrict;
                if (d.type == 0x71d9cd60)  //campus
                {
                    if (neighbor->feature == 0xeb13f3d7 || neighbor->feature == 0x5cb047c1) // reef / geothermal
                        d.addProxi(2.f, "Adjacent geothermal / reef");
                    if (neighbor->terrain == 0x5f83b433 || neighbor->terrain == 0xa3b9a8f0 || neighbor->terrain == 0x5490bef9 || neighbor->terrain == 0xdf49ddbd || neighbor->terrain == 0x29b146f4) // mountain
                        d.addProxi(1.f, "Adjacent mountain");
                    if (neighbor->feature == 0xe9115439) // jungle
                        d.addProxi(0.5f, "Adjacent jungle");
                    if (pInfo.leader == 0x4111e7e3 && neighbor->feature == 0xe9115439) //brazil
                        d.addProxi(1.f, "Adjacent jungle - brazilian bonus");
                    if (pInfo.leader == 0xa3797d87 && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //indonesia
                        d.addProxi(0.5f, "Adjacent water - indonesian bonus");
                }
                if (d.type == 0x4d77d2bc)  //OBSERVATORY
                {
                    if (neighbor->improvment == 0x5ad657ab) // plantation
                        d.addProxi(2.f, "Adjacent plantation");
                    if (neighbor->improvment == 0xa0929b1) // farm
                        d.addProxi(0.5f, "Adjacent farm");
                }
                if (d.type == 0xfbe22137)  //SEOWON
                {
                    if (neighbor->improvment == 0x3bb72a67) // mine
                        d.addProxi(0.5f, "Adjacent mine");
                }
                else if (d.type == 0x32b15d10 || d.type == 0xd28784a8)  //holy site / lavra
                {
                    if (neighbor->terrain == 0x5f83b433 || neighbor->terrain == 0xa3b9a8f0 || neighbor->terrain == 0x5490bef9 || neighbor->terrain == 0xdf49ddbd || neighbor->terrain == 0x29b146f4) // mountain
                        d.addProxi(1.f, "Adjacent mountain");
                    if (isNaturalWonder(neighbor->feature, assets))
                        d.addProxi(1.f, "Adjacent natural wonder");
                    if (neighbor->feature == 0xa220944) // forest
                        d.addProxi(0.5f, "Adjacent forest");
                    if (pInfo.leader == 0x4111e7e3 && neighbor->feature == 0xe9115439) //brazil
                        d.addProxi(1.f, "Adjacent jungle - brazilian bonus");
                    if (pInfo.leader == 0xf7f035fa && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //norwegia
                        d.addProxi(1.f, "Adjacent water - norwegian bonus");
                    if (pInfo.leader == 0xa3797d87 && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //indonesia
                        d.addProxi(0.5f, "Adjacent water - indonesian bonus");
                    //todo pantheon !
                }
                else if (d.type == 0xbe8c19bb || d.type == 0x782329e8)  //commercial hub / suguba
                {
                    if (neighbor->river != 0 && !d.bRiver)
                    {
                        d.addProxi(2.f, "Adjacent to a river");
                        d.bRiver = true;
                    }
                    if (pInfo.leader == 0x4111e7e3 && neighbor->feature == 0xe9115439) //brazil
                        d.addProxi(1.f, "Adjacent jungle - brazilian bonus");
                }
                else if (d.type == 0x885fdf1e || d.type == 0xbe3f9c41)  //theatre
                {
                    if (neighbor->wonder != 0xffffffff)
                        d.addProxi(2.f, "Adjacent wonder");
                    if (pInfo.leader == 0x4111e7e3 && neighbor->feature == 0xe9115439) //brazil
                        d.addProxi(1.f, "Adjacent jungle - brazilian bonus");
                    if (pInfo.leader == 0xa3797d87 && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //indonesia
                        d.addProxi(0.5f, "Adjacent water - indonesian bonus");
                }
                else if (d.type == 0xcbecc739 || d.type == 0x57490d08 || d.type == 0x22e60fde)  //harbor // royal navy // cothon
                {
                    if (neighbor->resource != 0xFFFFFFFF && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //sea resource
                        d.addProxi(1.f, "Adjacent sea resource");
                }
                else if (d.type == 0x9ac880cb)  //ZI
                {
                    if (neighbor->improvment == 0x3bb72a67) //mine
                        d.addProxi(0.5f, "Adjacent mine");
                    if (neighbor->improvment == 0x7a1ae4c0) //lumber mills
                        d.addProxi(0.5f, "Adjacent lumber mills");
                    if (neighbor->improvment == 0xfb33c447) //quarry
                        d.addProxi(1.f, "Adjacent quarry");
                    if (pInfo.leader == 0xa3797d87 && (neighbor->terrain == 0x4a707a11 || neighbor->terrain == 0x47c909dd)) //indonesia
                        d.addProxi(0.5f, "Adjacent water - indonesian bonus");
                    //TODO discover tech
                    if (neighbor->hasStrategicResource())
                        d.addProxi(1.f, "Adjacent strategic resource");
                }
                else if (d.type == 0x2c29e1e1) //hansa
                {
                    if (neighbor->resource != 0xFFFFFFFF) //resource
                        d.addProxi(1.f, "Adjacent resource");
                }
                else if (d.type == 0xb45b1b15) //oppidum
                {
                    //TODO discover tech
                    if (neighbor->hasStrategicResource())
                        d.addProxi(2.f, "Adjacent strategic resource");
                    if (neighbor->improvment == 0xfb33c447) //quarry
                        d.addProxi(2.f, "Adjacent quarry");

                }
                //if neighbour is a district (and not a wonder)
                if(pDistrict && pDistrict->type != 0x62f1b509)
                {
                    if (d.type == 0xbe8c19bb)  //commercial hub
                    {
                        if (pDistrict->type == 0xcbecc739) //harbor
                            d.addProxi(2.f, "Adjacent to a harbor");
                    }
                    else if (d.type == 0x782329e8)  //suguba
                    {
                        if (pDistrict->type == 0x32b15d10) //holy site
                            d.addProxi(2.f, "Adjacent to a holy site");
                    }
                    else if (d.type == 0x885fdf1e || d.type == 0xbe3f9c41)  //theatre
                    {
                        if (pDistrict->type == 0x762430e5 || pDistrict->type == 0xe317d371 || pDistrict->type == 0x5ef18d30 || pDistrict->type == 0xf855fcb5 || pDistrict->type == 0x58015315) //ENTERTAINMENT_COMPLEX
                            d.addProxi(2.f, "Adjacent to an entertainment complex");
                    }
                    else if (d.type == 0xcbecc739 || d.type == 0x57490d08 || d.type == 0x22e60fde)  //harbor // royal navy // cothon
                    {
                        if (pDistrict->type == 0x8a32007d) //citycenter
                            d.addProxi(2.f, "Adjacent to a city center");
                        if (pInfo.leader == 0xf7f035fa && (pDistrict->type == 0x32b15d10)) //norwegia
                            d.addProxi(2.f, "Adjacent to a holy site");
                    }
                    else if (d.type == 0x9ac880cb || d.type == 0x2c29e1e1)  //ZI / hansa
                    {
                        if (pDistrict->type == 0xb92c62a7 || pDistrict->type == 0xa58ab9ac || pDistrict->type == 0x5dbcdc9e || pDistrict->type == 0xe0ea131d) //aqueduct / dam / bath / canal
                            d.addProxi(2.f, "Adjacent to an aqueduc / dam / bath / canal");
                        if (d.type == 0x2c29e1e1) //hansa
                        {
                            if (pDistrict->type == 0xbe8c19bb) //commercial hub
                                d.addProxi(2.f, "Adjacent to a commercial hub");
                        }
                    }

                    if (pDistrict->type == 0x98117336) //government
                    {
                        d.addProxi(1.f, "Adjacent to a government plaza");
                    }

                    if (d.type == 0xfbe22137)//seowon
                    {
                        if (pDistrict->type != 0x885fdf1e)
                            d.addProxi(-1.f, "Adjacent to a district");
                    }
                    else if (d.type == 0x79dd476d) //thanh
                    {
                        d.addProxi(2.f, "Adjacent to a district");
                    }
                    else if (d.type == 0xbe3f9c41) //acropolis
                    {
                        d.addProxi(1.f, "Adjacent to a district");
                        if (pDistrict->type == 0x8a32007d)
                            d.addProxi(1.f, "Adjacent to a city center");

                    }
                    else
                    {
                        if (pInfo.leader == 0x70223815)//japan
                            d.addProxi(1.f, "Adjacent to a district");
                        else
                            d.addProxi(0.5f, "Adjacent to a district");
                    }

                }
            }

            for (std::map<std::string, std::pair<int, float>>::iterator it = proxi.begin(); it != proxi.end(); it++)
            {
                totalProxi += (int)(it->second.first * it->second.second);
            }
        }
	}
};