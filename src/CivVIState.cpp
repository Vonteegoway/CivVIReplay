#include "CivVIState.h"
#include "CivVIGame.h"
#include <algorithm>
#include <fstream>
#include <iostream>


void prettifyName(char* name)
{
    char* c = name;
    bool upper = true;
    while (*c) {
        if (*c == '_')
        {
            *c = ' ';
            upper = true;
        }
        else
        {
            if (!upper)
            {
                *c = (char)tolower(*c);
            }
            upper = false;
        }
        c++;
    }
}

void CivVIState::ComputeDistrictProxi(const CivVIGame& Game, Assets& assets)
{
    for (int iPlayer = 0; iPlayer < players.size(); iPlayer++)
    {
        Player& p = players[iPlayer];
        if (p.iPlayer >= players.size())
            continue;
        PlayerInfo& pInfo = pGame->playersInfo[iPlayer];

        for (int iDistrict = 0; iDistrict < p.districts.size(); iDistrict++)
        {
            District& d = p.districts[iDistrict];
            d.computeProxi(tiles, width, pInfo, assets);

        }
    }
}

void CivVIState::UpdatePointer(const CivVIGame& Game)
{
    //--------------------------
    for (int iPlayer = 0; iPlayer < players.size(); iPlayer++)
    {
        Player& p = players[iPlayer];
        if (p.iPlayer >= players.size())
            continue;
        PlayerInfo& pInfo = pGame->playersInfo[iPlayer];

        for (int iCity = 0; iCity < p.cities.size(); iCity++)
        {
            City& c = p.cities[iCity];


            while (p.pCities.size() <= c.id)
                p.pCities.push_back(NULL);
            p.pCities[c.id] = &c;

            if (c.x + c.y * width > tiles.size())
                continue;
            tiles[c.x + c.y * width].pCity = &c;

            for (std::map<unsigned int, float>::iterator it = c.yields.begin(); it != c.yields.end(); it++)
            {
                p.yields[it->first] += it->second;
            }
            if (pInfo.typeCiv == CITY_STATE)
            {
                const char* cityName = pInfo.locCivName; //LOC_CIVILIZATION_MITLA_NAME
                const char* begin = cityName;
                if (!strncmp(cityName, "LOC_CIVILIZATION_", 17))
                    begin += 17;
                size_t len = strlen(begin);
                strcpy(c.pretty_name, begin); 
                c.pretty_name[len - 5] = 0;
            }
            else
            {
                const char* cityName = c.name.c_str();
                const char* begin = cityName;
                if (!strncmp(cityName, "LOC_CITY_NAME", 13))
                    begin += 14;
                else if (!strncmp(cityName, "LOC_CITY", 8))
                    begin += 9;
                strcpy(c.pretty_name, begin);
            }
            prettifyName(c.pretty_name);

            p.totalPop += c.population;

            c.player = &p;

        }
        for (int iDistrict = 0; iDistrict < p.districts.size(); iDistrict++)
        {
            District& d = p.districts[iDistrict];
            tiles[d.x + d.y * width].pDistrict = &d;
        }
        p.militaryStrength = 0;
        p.unitCount = 0;
        for (int iUnit = 0; iUnit < p.units.size(); iUnit++)
        {
            Unit& u = p.units[iUnit];
            u.player = &p;
            if(u.x != 0xffffd8f1)
                tiles[u.x + u.y * width].units.push_back(&u);

            UnitInfo& ui = p.unitsInfo[u.type];
            if (ui.health)
            {
                p.militaryStrength += ui.attack * (ui.health - u.damage) / ui.health;
                p.unitCount++;
            }
        }
        /* there is not all improvement here ....
        for (int iImprovement = 0; iImprovement < p.improvements.size(); iImprovement++)
        {
            Improvement& improvement = p.improvements[iImprovement];
            tiles[improvement.x + improvement.y * width].pImprovement = &improvement;
        }*/
        for (int iGovernor = 0; iGovernor < p.governors.size(); iGovernor++)
        {
            Governor& governor = p.governors[iGovernor];
            if (governor.player != 0xffff)
            {
                Player& otherPlayer = players[governor.player];
                for (int iCity = 0; iCity < otherPlayer.cities.size(); iCity++)
                {
                    if (otherPlayer.cities[iCity].id == governor.city)
                    {
                        otherPlayer.cities[iCity].governor = &governor;
                        break;
                    }
                }
            }
        }
        for (int iEmissary = 0; iEmissary < p.emissaries.size(); iEmissary++)
        {
            p.maxEmissary = std::max(p.maxEmissary, p.emissaries[iEmissary]);
        }
        //--
        for (int k = 0; k < 2; k++)
        {
            p.techDogmeInfo[k].researchCount = 0;
            for (std::map<unsigned int, bool>::iterator it = p.techDogmeInfo[k].found.begin(); it != p.techDogmeInfo[k].found.end(); it++)
            {
                if (it->second)
                    p.techDogmeInfo[k].researchCount++;
            }

        }
        //--
        for (int iOtherPlayer = 0; iOtherPlayer < players.size(); iOtherPlayer++)
        {
            Player& otherP = players[iOtherPlayer];
            PlayerInfo& pOtherInfo = pGame->playersInfo[iOtherPlayer];
            if (otherP.iPlayer >= players.size())
                continue;
            if (iOtherPlayer != iPlayer && pInfo.team != pOtherInfo.team && otherP.internalTourist > p.counterTourist)
                p.counterTourist = otherP.internalTourist;
        }
        //--
        if (pInfo.team != -1)
        {
            while (pInfo.team >= teams.size())
            {
                teams.push_back(Team());
            }
            Team& t = teams[pInfo.team];
            if (t.inSightCount.size())
            {
                for (int iTile = 0; iTile < t.inSightCount.size(); iTile++)
                {
                    t.inSightCount[iTile] += p.inSightCount[iTile];
                    t.visible[iTile] += p.visible[iTile];
                }
            }
            else
            {
                t.inSightCount = p.inSightCount;
                t.visible = p.visible;
            }
        }
    }
    //-------------------------
    for (int iTile = 0; iTile < tiles.size(); iTile++)
    {
        const Tile& t = tiles[iTile];
        if (t.iPlayer < players.size())
        {
            if(!t.pDistrict && !t.pCity)
                players[t.iPlayer].freeTile++;
            if (t.improvment != 0xffffffff)
            {
                players[t.iPlayer].tileWorked++;
            }
        }
    }
    //-------------------------
}

static unsigned int getRevealStrategicTech(unsigned int resource)
{
    //return resource == 0xe3e70273 || resource == 0x64ec0edd || resource == 0x9353a548 || resource == 0x6f229238 || resource == 0x6bb73300 || resource == 0xa6573f0a || resource == 0x13cb7793;

    if (resource == 0x9353a548) // horse
        return 0x94e99382;
    if (resource == 0xe3e70273) // aluminium
        return 0xc4919d63;
    if (resource == 0x64ec0edd) // coal
        return 0xc6a9793a;
    if (resource == 0x6f229238) // iron
        return 0x134cbbf0;
    if (resource == 0x6bb73300) // niter
        return 0x3813af2d;
    if (resource == 0xa6573f0a) // oil
        return 0xf144c0b2;
    if (resource == 0x13cb7793) // uranium
        return 0x95484501;
}

void CivVIState::ComputeTileYields(Assets& assets)
{
    const CivVIState& State = *this;
    std::vector<Tile>& tiles = this->tiles;
    for (int i = 0; i < tiles.size(); i++)
    {
        Tile& tile = tiles[i];
        for (std::map<unsigned int, unsigned int>::iterator it = tile.disaster.begin(); it != tile.disaster.end(); it++)
        {
            if(it->second)
                tile.yields[it->first] += it->second;
        }

        for (int iModifier = 0; iModifier < assets.staticYieldModifiers.size(); iModifier++)
        {
            const StaticModifier& modifier = assets.staticYieldModifiers[iModifier];
            if (!modifier.count)
                continue;
            if (modifier.type == tile.terrain)
                tile.yields[modifier.yield] += modifier.count;
            if (modifier.type == tile.feature)
                tile.yields[modifier.yield] += modifier.count;
            if (modifier.type == tile.resource)
            {
                if (modifier.layerType == RESOURCE && tile.hasStrategicResource())
                {
                    if (tile.iPlayer < State.players.size())
                    {
                        const Player& p = State.players[tile.iPlayer];
                        unsigned int tech = getRevealStrategicTech(tile.resource);
                        if(p.techDogmeInfo[0].found.at(tech))
                            tile.yields[modifier.yield] += modifier.count;

                    }
                }
                else
                    tile.yields[modifier.yield] += modifier.count;
            }
            if (modifier.type == tile.improvment)
            {

                if (modifier.reqTech != 0)
                {
                    if (tile.iPlayer < State.players.size())
                    {
                        const Player& p = State.players[tile.iPlayer];
                        if (p.techDogmeInfo[0].found.at(modifier.reqTech))
                            tile.yields[modifier.yield] += modifier.count;
                    }
                }
                else if (modifier.reqCivic != 0)
                {
                    if (tile.iPlayer < State.players.size())
                    {
                        const Player& p = State.players[tile.iPlayer];
                        if (p.techDogmeInfo[1].found.at(modifier.reqCivic))
                            tile.yields[modifier.yield] += modifier.count;
                    }
                }
                else
                    tile.yields[modifier.yield] += modifier.count;

            }
        }
    }
}



void CivVIState::ComputeCityLine()
{
    std::vector<Tile>& tiles = this->tiles;

    for (int i = 0; i < tiles.size(); i++)
    {
        Tile& tile = tiles[i];
        if (tile.iPlayer >= 64)
            continue;

        tile.iLine += 63; // 2^6 -1

        std::pair<int, int> offsets[] = { { -1, 0 }, { 0, +1 }, { +1, 0 }, { +1, -1 }, { 0, -1 }, {-1, -1}};
        int x = i % width;
        int y = i / width;
        for (int k = 0; k < 6; k++)
        {
            int newY = y + offsets[k].first;
            int newX = x + offsets[k].second;
            if (offsets[k].first != 0 && y % 2 != 0)
                newX++;
            if (newY < 0 || newY >= tiles.size() / width)
                continue;
            if (newX < 0)
                newX += width;
            if (newX >= width)
                newX -= width;
            Tile& neighbour = tiles[newX + newY * width];
            if (neighbour.iPlayer == tile.iPlayer)
                tile.iLine -= pow(2, k);
        }
    }
}
#if 0
void WriteTrees(CivVIGame* pGame)
{
    std::ofstream outfile("techtree", std::ios::out);
    for (int i = 0; i < pGame->techTreeByColumn.size(); i++)
    {
        for (int j = 0; j < pGame->techTreeByColumn[i].size(); j++)
        {
            Node& n = *pGame->techTreeByColumn[i][j];
            outfile << "0x" << std::hex << n.id << "," << std::dec << n.cost << "," << i << "," << j << "," << n.parents.size();

            for (int iParent = 0; iParent < n.parents.size(); iParent++)
            {
                outfile <<",0x" << std::hex << n.parents[iParent];
            }
            outfile << std::endl;
        }
    }
    outfile << std::endl;
    outfile << std::endl;
    for (int i = 0; i < pGame->civicTreeByColumn.size(); i++)
    {
        for (int j = 0; j < pGame->civicTreeByColumn[i].size(); j++)
        {
            Node& n = *pGame->civicTreeByColumn[i][j];
            outfile << "0x" << std::hex << n.id << "," << std::dec << n.cost << "," << i << "," << j << "," << n.parents.size();
            for (int iParent = 0; iParent < n.parents.size(); iParent++)
            {
                outfile << ",0x" << std::hex << n.parents[iParent];
            }
            outfile << std::endl;
        }
    }
}

void SetupTrees(CivVIGame* pGame)
{
    {
        std::vector<int> cost;
        for (std::vector<Node>::iterator it = pGame->techTree.begin(); it != pGame->techTree.end(); it++)
        {
            std::vector<int>::iterator f = std::find(cost.begin(), cost.end(), it->cost);
            if (f == cost.end())
            {
                cost.push_back(it->cost);
                pGame->techTreeByColumn.push_back(std::vector<Node*>());
                pGame->techTreeByColumn.back().push_back(&(*it));
            }
            else {
                int column = f - cost.begin();
                pGame->techTreeByColumn[column].push_back(&(*it));
            }

        }
    }
    {
        std::vector<int> cost;
        for (std::vector<Node>::iterator it = pGame->civicTree.begin(); it != pGame->civicTree.end(); it++)
        {
            std::vector<int>::iterator f = std::find(cost.begin(), cost.end(), it->cost);
            if (f == cost.end())
            {
                cost.push_back(it->cost);
                pGame->civicTreeByColumn.push_back(std::vector<Node*>());
                pGame->civicTreeByColumn.back().push_back(&(*it));
            }
            else {
                int column = f - cost.begin();
                pGame->civicTreeByColumn[column].push_back(&(*it));
            }

        }
    }
}
#endif

void CivVIState::Parse(CivVIParser& Parser, const CivVIGame& Game, Assets& assets)
{

    std::vector<byte>& data = Parser.compressedData.back();

    Parser.ParseTurn(data, turn);
    Parser.ParseMap(data, width, tiles);
    Parser.ParseDisaster(data, width, tiles);


    try
    {
        Parser.ParsePlayers(data, this);
    }
    catch (...)
    {
        error = "Error while parsing";
    }

    Parser.ParseVisibility(data, players);
    UpdatePointer(Game);
    if (assets.assets.size())
    {
        ComputeDistrictProxi(Game, assets);
        ComputeCityLine();
        ComputeTileYields(assets);
    }
    return;
}