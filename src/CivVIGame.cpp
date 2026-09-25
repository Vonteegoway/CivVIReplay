#include "CivVIGame.h"
#include "CivVIState.h"
#include <algorithm>

CivVIGame::CivVIGame()
{
    nbPlayer = 0;
    begin = 0;
    seed = 0;
    timestamp = 0;
    multiPlayer = false;
    gamespeed[0] = 0;
    mapSize[0] = 0;
    map[0] = 0;
}

void CivVIGame::UpdateMapStats(CivVIState& firstState)
{
    for (int iTile = 0; iTile < firstState.tiles.size(); iTile++)
    {
        Tile& tile = firstState.tiles[iTile];
        terrainStats[tile.terrain]++;
        resourceStats[tile.resource]++;
        featureStats[tile.feature]++;
    }
}

void CivVIGame::UpdateStats(CivVIState& previousState, CivVIState& state, Assets& assets)
{
    if (state.players.size() == 0 || previousState.players.size() == 0)
        return;

    if (state.turn - previousState.turn != 1)
        return;

    for (int iPlayer = 0; iPlayer < previousState.players.size(); iPlayer++)
    {
        Player& pp = previousState.players[iPlayer];
        Player& p = state.players[iPlayer];
        const PlayerInfo& playerInfo = playersInfo[iPlayer];

        if (pp.iPlayer == -1)
            continue;

        if (playerInfo.typeCiv != FULL_CIV)
            continue;

        p.builderBuilt = pp.builderBuilt;
        p.settlerBuilt = pp.settlerBuilt;

        for (std::map<unsigned int, unsigned int>::iterator it = pp.goodyHuts.begin(); it != pp.goodyHuts.end(); it++)
        {
            if (it->second != p.goodyHuts[it->first])
            {
                huts.push_back(GoodyHut(it->first, iPlayer, previousState.turn));
            }
        }

        for (std::map<unsigned int, bool>::iterator it = pp.techDogmeInfo[TECH].found.begin(); it != pp.techDogmeInfo[TECH].found.end(); it++)
        {
            if (it->second != p.techDogmeInfo[TECH].found[it->first] && !p.techDogmeInfo[TECH].boost[it->first])
            {
                hardTechs.push_back(HardTech(it->first, iPlayer, previousState.turn));
            }
        }

        for (std::map<unsigned int, bool>::iterator it = pp.techDogmeInfo[DOGME].found.begin(); it != pp.techDogmeInfo[DOGME].found.end(); it++)
        {
            if (it->second != p.techDogmeInfo[DOGME].found[it->first] && !pp.techDogmeInfo[DOGME].boost[it->first])
            {
                hardDogmes.push_back(HardTech(it->first, iPlayer, previousState.turn));
            }
        }

        for (std::map<unsigned int, float>::iterator it = p.yields.begin(); it != p.yields.end(); it++)
        {
            p.totalYields[it->first] = pp.totalYields[it->first] + it->second;
        }

        while (districtStats.size() <= iPlayer)
            districtStats.push_back(std::vector<DistrictStat>());

        std::vector<DistrictStat>& st = districtStats[iPlayer];

        for (int iDistrict = 0; iDistrict < p.districts.size(); iDistrict++)
        {
            District& d = p.districts[iDistrict];
            if (d.type == 0x8a32007d || d.type == 1660007689)
                continue;

            while (st.size() <= d.globalId)
                st.push_back(DistrictStat());

            if(st[d.globalId].turnBegin == -1)
                st[d.globalId] = DistrictStat(d.type, iPlayer, previousState.turn, d.cost, d.globalId, d.cityId, state.players[iPlayer].pCities[d.cityId]->name.c_str());

            if (d.built && st[d.globalId].turnEnd == -1)
                st[d.globalId].turnEnd = previousState.turn;

        }

        std::map<unsigned int, int> unitByTypeDiff;
        std::map<unsigned int, unsigned int> starByIds;

        for (int iUnit = 0; iUnit < p.units.size(); iUnit++)
        {
            Unit& u = p.units[iUnit];
            UnitInfo& ui = p.unitsInfo[u.type];
            if (u.x == 0xffffd8f1)
               continue;
            if (u.army>3)
                continue;
            unitByTypeDiff[u.type]++; //create or get
            starByIds[u.id] = u.army;
            if (ui.type == 0x11d57532) //builder
            {
                bool bFound = false;
                for (int iPreviousUnit = 0; iPreviousUnit < pp.units.size(); iPreviousUnit++)
                {
                    Unit& pu = pp.units[iPreviousUnit];
                    if (pu.type == u.type && pu.id == u.id)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                    p.builderBuilt++;
            }
            if (ui.type == 0xd0649d28) //settler
            {
                bool bFound = false;
                for (int iPreviousUnit = 0; iPreviousUnit < pp.units.size(); iPreviousUnit++)
                {
                    Unit& pu = pp.units[iPreviousUnit];
                    if (pu.type == u.type && pu.id == u.id)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                    p.settlerBuilt++;
            }
        }
        int stars[3] = { 0, 0, 0 };

        for (int iPreviousUnit = 0; iPreviousUnit < pp.units.size(); iPreviousUnit++)
        {
            Unit& u = pp.units[iPreviousUnit];
            if (u.x == 0xffffd8f1)
                continue;
            //UnitInfo& ui = p.unitsInfo[u.type];
            unitByTypeDiff[u.type]--; //create or get

            if (starByIds.find(u.id) != starByIds.end())
            {
                if (starByIds[u.id] != u.army && u.army != 0xffffffff)
                {
                    stars[starByIds[u.id]]++;
                    stars[u.army]--;
                }
            }
        }


        p.unitLost = pp.unitLost - stars[1] - stars[2]; // for 3stars we assume here we always get them from 2stars... easiest but incorrect 
        p.unitUpgraded = pp.unitUpgraded;
        for (std::map<unsigned int, int>::iterator it = unitByTypeDiff.begin(); it != unitByTypeDiff.end(); it++)
        {
            unsigned int currentUnit = it->first;
            UnitInfo& ui = p.unitsInfo[currentUnit];
            if (it->second < 0)
            {
                if (ui.attack == 0)
                    continue;
                int unitLost = -it->second;
                int unitUpgrade = 0;
                while (assets.unitUpgrade.find(currentUnit) != assets.unitUpgrade.end())
                {
                    unsigned int upgrade = assets.unitUpgrade[currentUnit];

                    if (unitByTypeDiff.find(upgrade) != unitByTypeDiff.end() && unitByTypeDiff[upgrade] > 0)
                    {
                        unitUpgrade += std::min(-it->second, unitByTypeDiff[upgrade]);
                        unitLost -= unitUpgrade;
                        break;
                    }

                    std::vector<unsigned int> units = assets.unitReplace[upgrade];
                    bool found = false;
                    for (unsigned int unit : units)
                    {
                        if (unitByTypeDiff.find(unit) != unitByTypeDiff.end() && unitByTypeDiff[unit] > 0)
                        {
                            unitUpgrade += std::min(-it->second, unitByTypeDiff[unit]);
                            unitLost -= unitUpgrade;
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                    currentUnit = upgrade;
                }
                p.unitLost += unitLost;
                p.unitUpgraded += unitUpgrade;
            }
        }


        while (state.missingGolds.size() <= playerInfo.team)
            state.missingGolds.push_back(0);
        state.missingGolds[playerInfo.team] += p.gold - pp.gold - p.yields[0xB761634A];

    }

}
