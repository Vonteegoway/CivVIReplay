#include "SaveLoader.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include "Platform.h"

static std::string extractValueFromValveFile(const std::string& line, const std::string& key)
{
    size_t pathKeyPos = line.find(key);
    if (pathKeyPos != std::string::npos)
    {
        size_t pathBeginPos = line.find("\"", pathKeyPos + key.size() + 1);
        size_t pathEndPos = line.find("\"", pathBeginPos + 1);
        if (pathBeginPos != std::string::npos && pathEndPos != std::string::npos)
        {
            return line.substr(pathBeginPos + 1, pathEndPos - pathBeginPos - 1);
        }
    }

    return "";
}

static fs::path getInstalledCivSteam()
{
    const std::string civSteamId = "289070";

    for (const fs::path& library : Platform::steamLibraries())
    {
        fs::path steamapps = library / "steamapps";
        std::ifstream manifest(steamapps / ("appmanifest_" + civSteamId + ".acf"));
        std::string line;
        while (std::getline(manifest, line))
        {
            std::string installdir = extractValueFromValveFile(line, "\"installdir\"");
            if (!installdir.empty())
            {
                fs::path civ = steamapps / "common" / installdir;
                std::error_code ec;
                if (fs::is_directory(civ, ec))
                    return civ / ""; // trailing separator: isValidCivPath inspects parent_path()
            }
        }
    }
    return fs::path();
}

static fs::path getInstalledCivEpic()
{
#ifdef _WIN32
    const std::string civEpicId = "Kinglet";

    const char* programData = std::getenv("PROGRAMDATA");
    if (!programData)
        return fs::path();
    fs::path ManifestsPath = fs::path(programData) / "Epic" / "EpicGamesLauncher" / "Data" / "Manifests";

    std::error_code ec;
    if (!fs::exists(ManifestsPath, ec))
        return fs::path();

    for (const auto& entry : fs::directory_iterator(ManifestsPath, ec))
    {
        bool isRightGame = false;
        std::string installLocation;
        if (entry.path().extension() == ".item")
        {
            std::string line;
            std::ifstream myfile(entry.path(), std::ios::in);
            while (std::getline(myfile, line))
            {
                std::string epicId = extractValueFromValveFile(line, "\"MainGameAppName\"");
                if (!epicId.empty())
                {
                    if (epicId == civEpicId)
                        isRightGame = true;
                    else
                        break;
                }
                if (installLocation.empty())
                    installLocation = extractValueFromValveFile(line, "\"InstallLocation\"");
            }
            myfile.close();
            if (isRightGame)
            {
                //LogA("Find civ path : %s", installLocation.c_str());
                return fs::path(installLocation) / "";
            }
        }
    }
#endif
    return fs::path();
}

void SaveLoader::init()
{
    SetupTreesManually();
    States.reserve(1000);
    loadedStateCount = 0;
    stop = false;
    endLoadCivDb = false;
    //LogA("Find Steam or Epic civ install");


    fs::path path = getInstalledCivSteam();
    if (!path.empty())
    {
        //LogA("Civ is installed on Steam : %s", civPath.c_str());
        civPath = path;
        steamInstall = true;
    }
    else
    {
        //LogA("Civ is installed on Epic or not installed");
        civPath = getInstalledCivEpic();
        steamInstall = false;
    }

    bbgPath = Platform::civUserDir() / "Mods" / "BetterBalancedGame";
    //LogA("BBG path (default on Epic) : %s", bbgPath.c_str());
}

void SaveLoader::Close()
{
    // Drop queued saves, then wait for the loader thread to finish the one it is parsing
    mtx.lock();
    while (paths.size())
        paths.pop();
    while (busy)
    {
        mtx.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        mtx.lock();
    }
    States.clear();
    Games.clear();
    loadedStateCount = 0;
    mtx.unlock();
}

bool SaveLoader::isValidCivPath()
{
    if (civPath.empty())
        return false;
    if (!fs::is_directory(civPath))
        return false;
    fs::path p = civPath.parent_path().filename();
    if (p == "Steam")
        return true;
    if (p == "SteamApps")
        return true;
    if (p == "Sid Meier's Civilization VI")
        return true;

    for (const auto& entry : fs::directory_iterator(civPath))
    {
        const fs::path& filename = entry.path().filename();
        if (filename == "Base") //Base
            return true;
    }

    return false;
}

bool SaveLoader::isValidBBGPath()
{
    if (steamInstall)
        return true; //mod is in workshop

    if (bbgPath.empty())
        return false;
    if (!fs::is_directory(bbgPath))
        return false;
    for (const auto& entry : fs::directory_iterator(bbgPath))
    {
        const fs::path& filename = entry.path().filename();
        if (filename == "BetterBalancedGame.modinfo") //BBG
            return true;
    }
    return false;
}

void SaveLoader::LoadSave_WithoutAsset(bool untilEnd)
{
    bool sendOnceToDatabase = false;
    while (!stop)
    {
        if (paths.empty())
        {
            if (untilEnd)
                return;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }


        std::string path;
        mtx.lock();
        if (paths.empty()) // emptied by Close() since the check above
        {
            mtx.unlock();
            continue;
        }
        path = paths.front();
        paths.pop();
        busy = true;
        mtx.unlock();

        //LogA("Begin LoadSave_WithoutAsset %s \n", path.c_str());

        States.push_back(CivVIState());
        CivVIState& State = States[loadedStateCount];

        CivVIParser Parser;

        Parser.ParseAll(path.c_str());
        if (printPaths)
            std::cout << path << std::endl;
#ifdef _DEBUG
        char filename[1000];
        strcpy(filename, path.c_str());
        int len = strlen(path.c_str());
        filename[len - 8] = 'b';
        filename[len - 7] = 'i';
        filename[len - 6] = 'n';
        filename[len - 5] = 0;
        Parser.Write(filename, Parser.compressedData.back());
#endif

        if (stop)
            return;

        int seed = Parser.GetGameSeed();
        bool newSeed = false;
        if (Games.find(seed) == Games.end())
        {
            try
            {
                Games[seed] = CivVIGame();
                Games[seed].seed = seed;
                Parser.ParsePlayersInfo(Games.at(seed));
                newSeed = true;

            }
            catch (...)
            {
                State.error = "Error while parsing";
            }

        }
        CivVIGame& Game = Games.at(seed);
        State.pGame = &Game;
        State.timestamp = Parser.timestamp;

        if (stop)
            return;

        try
        {
            State.Parse(Parser, Game, assets);
        }
        catch (const std::exception& e)
        {
            // One unreadable save must not take the whole viewer down
            State.error = std::string("Error while parsing: ") + e.what();
        }

        if (stop)
            return;

        if (newSeed)
        {
            Game.timestamp = Parser.timestamp;
            Parser.SetGameInfo(Game);
            for (int i = 0; i < 64; i++)
            {
                PlayerInfo& playerInfo = Game.playersInfo[i];
                const Player& p = State.players[i];
                if (!p.cities.size() && !p.units.size())
                    continue;
                else if (playerInfo.typeCiv == FULL_CIV)
                {
                    if (!Game.nbPlayer)
                        Game.begin = i;

                    int team = playerInfo.team;
                    while (Game.playersByTeams.size() <= team)
                        Game.playersByTeams.push_back(std::vector<int>());

                    Game.playersByTeams[team].push_back(i);
                    Game.nbPlayer++;
                }
                else if (playerInfo.typeCiv == CITY_STATE)
                    continue;
                else
                    break;

            }
            Game.UpdateMapStats(State);

#ifdef _DEBUG
            ParserCpy = Parser;
#endif
        }


        if (loadedStateCount != 0 && States[loadedStateCount - 1].pGame == State.pGame)
            Games[seed].UpdateStats(States[loadedStateCount - 1], State, assets);

        if (stop)
            return;
        mtx.lock();
        loadedStateCount++;
        mtx.unlock();

        if (!sendOnceToDatabase)
        {
            std::string players = "";;
            for (int i = 0; i < State.pGame->playersByTeams.size(); i++)
            {
                for (int j = 0; j < State.pGame->playersByTeams[i].size(); j++)
                {
                    int iPlayer = State.pGame->playersByTeams[i][j];
                    PlayerInfo& playerInfo = State.pGame->playersInfo[iPlayer];
                    players+= playerInfo.pseudo;
                    players += " (";
                    players += playerInfo.pseudoWithSteamId;
                    players += ") / ";
                }
            }
            /*CURL* curl = curl_easy_init();
            if (curl) {
                Json::Value root;
                Json::StreamWriterBuilder StreamWriterBuilder;

                root["seed"] = seed;
                root["first_timestamp"] = State.timestamp;
                root["user_timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                root["number_of_turn"] = paths.size() + 1;
                root["map"] = Games[seed].map;
                root["players"] = players;

                std::string string = Json::writeString(StreamWriterBuilder, root);
                curl_easy_setopt(curl, CURLOPT_URL, "https://civgg.com/api/usage.php");
                curl_easy_setopt(curl, CURLOPT_POST, 1);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, string.c_str());

                struct curl_slist* slist1;
                slist1 = NULL;
                slist1 = curl_slist_append(slist1, "Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist1);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
            }*/
            sendOnceToDatabase = true;
        }

        mtx.lock();
        busy = false;
        mtx.unlock();
    }
    return;

}

void SaveLoader::LoadSave(bool untilEnd)
{
    while (waitForCivPaths && !isValidCivPath())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (stop)
            return;
        continue;
    }
    while (waitForCivPaths && !isValidBBGPath())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (stop)
            return;
        continue;
    }

    fs::path p = civPath.parent_path().filename();
    //[] = L"C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\";
    //[] = L"C:\\Program Files(x86)\\Steam\\SteamApps\\workshop\\content\\289070\\";
    if (steamInstall)
    {
        if (p == "Steam")
        {
            RootFolder = civPath / "SteamApps" / "common" / "Sid Meier's Civilization VI" / "";
            WorkshopFolder = civPath / "SteamApps" / "workshop" / "content" / "289070" / "";
        }
        else if (p == "SteamApps")
        {
            RootFolder = civPath / "common" / "Sid Meier's Civilization VI" / "";
            WorkshopFolder = civPath / "workshop" / "content" / "289070" / "";

        }
        else if (p == "Sid Meier's Civilization VI")
        {
            RootFolder = civPath;
            WorkshopFolder = civPath.parent_path().parent_path().parent_path() / "workshop" / "content" / "289070" / "";
        }

    }
    else //EPIC version can be random
    {
        RootFolder = civPath;
    }

    if (!steamInstall)
    {
        RootFolder = civPath;
        WorkshopFolder = bbgPath;
    }

    //LogA("Trying to load Asset with : %s / %s", RootFolder.c_str(), WorkshopFolder.c_str());

    try
    {
        assets.InitAll(RootFolder, WorkshopFolder);
    }
    catch (const std::exception& e)
    {
        setFatalError(std::string("Could not load the gameplay database: ") + e.what());
        endLoadCivDb = true;
        return;
    }
    endLoadCivDb = true;

    LoadSave_WithoutAsset(untilEnd);
}

void SaveLoader::SetupTreesManually()
{
    techTree2[0x38c528c5] = CivicTech(0x38c528c5, 25, 0, 3, 0);
    techTree2[0x94e99382] = CivicTech(0x94e99382, 25, 0, 4, 0);
    techTree2[0x88915fe6] = CivicTech(0x88915fe6, 25, 0, 6, 0);
    techTree2[0x2b98e631] = CivicTech(0x2b98e631, 50, 1, 0, 0);
    techTree2[0x8a9cf522] = CivicTech(0x8a9cf522, 50, 1, 1, 0);
    techTree2[0x66492a1d] = CivicTech(0x66492a1d, 50, 1, 2, 1, 0x38c528c5);
    techTree2[0xcf38cd67] = CivicTech(0xcf38cd67, 50, 1, 3, 1, 0x38c528c5);
    techTree2[0x8e2b0d4f] = CivicTech(0x8e2b0d4f, 50, 1, 4, 1, 0x94e99382);
    techTree2[0x3dab57d4] = CivicTech(0x3dab57d4, 80, 2, 5, 1, 0x88915fe6);
    techTree2[0x134cbbf0] = CivicTech(0x134cbbf0, 80, 2, 6, 1, 0x88915fe6);
    techTree2[0xbda3136e] = CivicTech(0xbda3136e, 80, 2, 7, 1, 0x88915fe6);
    techTree2[0xc0b521c9] = CivicTech(0xc0b521c9, 120, 3, 1, 2, 0x2b98e631, 0x8a9cf522);
    techTree2[0x3413597d] = CivicTech(0x3413597d, 120, 3, 3, 1, 0xcf38cd67);
    techTree2[0x9b6ac79e] = CivicTech(0x9b6ac79e, 120, 3, 4, 1, 0x8e2b0d4f);
    techTree2[0x4bf844d9] = CivicTech(0x4bf844d9, 120, 3, 6, 1, 0x134cbbf0);
    techTree2[0xa959dbd4] = CivicTech(0xa959dbd4, 200, 4, 0, 1, 0x2b98e631);
    techTree2[0x9ff51dd] = CivicTech(0x9ff51dd, 200, 4, 2, 1, 0x3413597d);
    techTree2[0xeec2897e] = CivicTech(0xeec2897e, 200, 4, 5, 2, 0x3dab57d4, 0x9b6ac79e);
    techTree2[0x8a1b1084] = CivicTech(0x8a1b1084, 200, 4, 7, 1, 0xbda3136e);
    techTree2[0xca2c34d6] = CivicTech(0xca2c34d6, 300, 5, 0, 2, 0xa959dbd4, 0x9ff51dd);
    techTree2[0xd1f9acec] = CivicTech(0xd1f9acec, 300, 5, 1, 1, 0x9ff51dd);
    techTree2[0x16953d32] = CivicTech(0x16953d32, 300, 5, 3, 2, 0x3413597d, 0x9b6ac79e);
    techTree2[0x4d69ed34] = CivicTech(0x4d69ed34, 300, 5, 7, 2, 0x4bf844d9, 0x8a1b1084);
    techTree2[0xaae9f4fa] = CivicTech(0xaae9f4fa, 390, 6, 2, 2, 0x9ff51dd, 0x16953d32);
    techTree2[0xdb491a81] = CivicTech(0xdb491a81, 390, 6, 4, 2, 0x9b6ac79e, 0x16953d32);
    techTree2[0x3813af2d] = CivicTech(0x3813af2d, 390, 6, 5, 1, 0xeec2897e);
    techTree2[0xc5e45f1] = CivicTech(0xc5e45f1, 390, 6, 6, 1, 0xeec2897e);
    techTree2[0x563db9c8] = CivicTech(0x563db9c8, 600, 7, 0, 1, 0xca2c34d6);
    techTree2[0x70cdec42] = CivicTech(0x70cdec42, 600, 7, 1, 3, 0xaae9f4fa, 0xca2c34d6, 0xd1f9acec);
    techTree2[0xc5c695e2] = CivicTech(0xc5c695e2, 600, 7, 3, 2, 0xaae9f4fa, 0xdb491a81);
    techTree2[0x9f2663d1] = CivicTech(0x9f2663d1, 600, 7, 4, 3, 0x16953d32, 0xdb491a81, 0x3813af2d);
    techTree2[0x6dcabcb1] = CivicTech(0x6dcabcb1, 600, 7, 7, 1, 0x4d69ed34);
    techTree2[0x558cd07b] = CivicTech(0x558cd07b, 730, 8, 0, 1, 0x563db9c8);
    techTree2[0xda7ad523] = CivicTech(0xda7ad523, 730, 8, 2, 1, 0xaae9f4fa);
    techTree2[0xf13db16d] = CivicTech(0xf13db16d, 730, 8, 4, 1, 0x9f2663d1);
    techTree2[0x19fe1463] = CivicTech(0x19fe1463, 730, 8, 6, 1, 0xc5e45f1);
    techTree2[0xc6a9793a] = CivicTech(0xc6a9793a, 930, 9, 1, 2, 0x558cd07b, 0x70cdec42);
    techTree2[0x3eae5253] = CivicTech(0x3eae5253, 930, 9, 2, 2, 0xda7ad523, 0xc5c695e2);
    techTree2[0xdf7425a7] = CivicTech(0xdf7425a7, 930, 9, 4, 1, 0xf13db16d);
    techTree2[0xa18b8e6] = CivicTech(0xa18b8e6, 930, 9, 6, 2, 0x19fe1463, 0x6dcabcb1);
    techTree2[0x802394] = CivicTech(0x802394, 1070, 10, 0, 1, 0xc6a9793a);
    techTree2[0x6dde11d9] = CivicTech(0x6dde11d9, 1070, 10, 2, 1, 0x3eae5253);
    techTree2[0x828ea81c] = CivicTech(0x828ea81c, 1070, 10, 3, 2, 0x3eae5253, 0xf13db16d);
    techTree2[0xe3ecf839] = CivicTech(0xe3ecf839, 1070, 10, 5, 2, 0xdf7425a7, 0xa18b8e6);
    techTree2[0x6f1bd3be] = CivicTech(0x6f1bd3be, 1250, 11, 1, 2, 0xc6a9793a, 0x3eae5253);
    techTree2[0x4e7655f1] = CivicTech(0x4e7655f1, 1250, 11, 3, 1, 0x828ea81c);
    techTree2[0xaa4867ff] = CivicTech(0xaa4867ff, 1250, 11, 4, 1, 0xe3ecf839);
    techTree2[0xf144c0b2] = CivicTech(0xf144c0b2, 1250, 11, 6, 1, 0xe3ecf839);
    techTree2[0x8bd21f17] = CivicTech(0x8bd21f17, 1370, 12, 0, 1, 0x802394);
    techTree2[0xc4919d63] = CivicTech(0xc4919d63, 1370, 12, 1, 2, 0x802394, 0x6f1bd3be);
    techTree2[0x166ecf01] = CivicTech(0x166ecf01, 1370, 12, 2, 2, 0x6dde11d9, 0x4e7655f1);
    techTree2[0xdf460ffd] = CivicTech(0xdf460ffd, 1370, 12, 5, 2, 0xaa4867ff, 0xf144c0b2);
    techTree2[0x2b81b90] = CivicTech(0x2b81b90, 1480, 13, 1, 1, 0xc4919d63);
    techTree2[0xa0be8a33] = CivicTech(0xa0be8a33, 1480, 13, 2, 2, 0xc4919d63, 0x166ecf01);
    techTree2[0xb60cc89d] = CivicTech(0xb60cc89d, 1480, 13, 3, 2, 0x4e7655f1, 0xaa4867ff);
    techTree2[0x95484501] = CivicTech(0x95484501, 1480, 13, 4, 2, 0xaa4867ff, 0xdf460ffd);
    techTree2[0x817c204c] = CivicTech(0x817c204c, 1480, 13, 5, 1, 0xdf460ffd);
    techTree2[0x55fafb59] = CivicTech(0x55fafb59, 1660, 14, 0, 2, 0x8bd21f17, 0xc4919d63);
    techTree2[0x3217da5b] = CivicTech(0x3217da5b, 1660, 14, 4, 2, 0xb60cc89d, 0x95484501);
    techTree2[0x722df1cd] = CivicTech(0x722df1cd, 1660, 14, 5, 1, 0x817c204c);
    techTree2[0x6287037f] = CivicTech(0x6287037f, 1850, 15, 0, 1, 0x55fafb59);
    techTree2[0x7f74293b] = CivicTech(0x7f74293b, 1850, 15, 2, 2, 0x2b81b90, 0xa0be8a33);
    techTree2[0xbfd2e6f1] = CivicTech(0xbfd2e6f1, 1850, 15, 3, 2, 0xa0be8a33, 0xb60cc89d);
    techTree2[0x24793884] = CivicTech(0x24793884, 1850, 15, 4, 1, 0x3217da5b);
    techTree2[0xa22e98d5] = CivicTech(0xa22e98d5, 1850, 15, 5, 1, 0x722df1cd);
    techTree2[0x31c59cff] = CivicTech(0x31c59cff, 1850, 15, 6, 1, 0x722df1cd);
    techTree2[0xd1353581] = CivicTech(0xd1353581, 2155, 16, 1, 4, 0x55fafb59, 0x7f74293b, 0xbfd2e6f1, 0x24793884);
    techTree2[0x5a4a7357] = CivicTech(0x5a4a7357, 2155, 16, 4, 1, 0x24793884);
    techTree2[0x2fafc921] = CivicTech(0x2fafc921, 2155, 16, 5, 1, 0xa22e98d5);
    techTree2[0xfa42e3c3] = CivicTech(0xfa42e3c3, 2200, 17, 1, 1, 0xd1353581);
    techTree2[0x9e1ebb1c] = CivicTech(0x9e1ebb1c, 2200, 17, 2, 1, 0x5a4a7357);
    techTree2[0x3e8da7fb] = CivicTech(0x3e8da7fb, 2300, 18, 0, 1, 0xd5cd2777);
    techTree2[0xe79ab97b] = CivicTech(0xe79ab97b, 2300, 18, 3, 1, 0xfa42e3c3);
    techTree2[0xe9e8dc87] = CivicTech(0xe9e8dc87, 2300, 18, 5, 1, 0x9e1ebb1c);
    techTree2[0xd5cd2777] = CivicTech(0xd5cd2777, 2200, 18, 7, 1, 0x2fafc921);
    techTree2[0x9ce01238] = CivicTech(0x9ce01238, 2500, 19, 5, 1, 0x3e8da7fb);
    techTree2[0xf1a0eab7] = CivicTech(0xf1a0eab7, 2600, 20, 5, 1, 0x9ce01238);

    civicTree2[0x3b6076e0] = CivicTech(0x3b6076e0, 20, 0, 3, 0);
    civicTree2[0x769077e3] = CivicTech(0x769077e3, 40, 1, 1, 1, 0x3b6076e0);
    civicTree2[0xcd2f9bc5] = CivicTech(0xcd2f9bc5, 40, 1, 5, 1, 0x3b6076e0);
    civicTree2[0x27a096db] = CivicTech(0x27a096db, 50, 2, 0, 1, 0x769077e3);
    civicTree2[0xb5575ea4] = CivicTech(0xb5575ea4, 70, 2, 3, 1, 0x769077e3);
    civicTree2[0xa042647c] = CivicTech(0xa042647c, 70, 2, 4, 1, 0xcd2f9bc5);
    civicTree2[0xc531df23] = CivicTech(0xc531df23, 50, 2, 6, 1, 0xcd2f9bc5);
    civicTree2[0x7f3ffeb6] = CivicTech(0x7f3ffeb6, 110, 3, 1, 1, 0xb5575ea4);
    civicTree2[0x2cf9f4b0] = CivicTech(0x2cf9f4b0, 110, 3, 3, 2, 0xb5575ea4, 0xa042647c);
    civicTree2[0x36ac8566] = CivicTech(0x36ac8566, 110, 3, 5, 1, 0xa042647c);
    civicTree2[0x92f4a3e1] = CivicTech(0x92f4a3e1, 120, 4, 0, 2, 0x27a096db, 0x7f3ffeb6);
    civicTree2[0xd60957bb] = CivicTech(0xd60957bb, 175, 4, 2, 2, 0x7f3ffeb6, 0x2cf9f4b0);
    civicTree2[0xa1e8bcdd] = CivicTech(0xa1e8bcdd, 175, 4, 4, 2, 0x2cf9f4b0, 0x36ac8566);
    civicTree2[0xab94aa5c] = CivicTech(0xab94aa5c, 120, 4, 6, 2, 0x36ac8566, 0xc531df23);
    civicTree2[0x9ea754ee] = CivicTech(0x9ea754ee, 220, 5, 1, 1, 0xd60957bb);
    civicTree2[0x646509ac] = CivicTech(0x646509ac, 300, 5, 2, 1, 0xd60957bb);
    civicTree2[0x95250890] = CivicTech(0x95250890, 300, 5, 4, 2, 0xd60957bb, 0xa1e8bcdd);
    civicTree2[0x7fc576de] = CivicTech(0x7fc576de, 340, 6, 0, 2, 0x92f4a3e1, 0x646509ac);
    civicTree2[0x2dafaf52] = CivicTech(0x2dafaf52, 420, 6, 2, 1, 0x646509ac);
    civicTree2[0xc123f7ca] = CivicTech(0xc123f7ca, 420, 6, 4, 2, 0x646509ac, 0x95250890);
    civicTree2[0xca671949] = CivicTech(0xca671949, 340, 6, 6, 2, 0x95250890, 0xab94aa5c);
    civicTree2[0x8c66e1c8] = CivicTech(0x8c66e1c8, 440, 7, 0, 2, 0x7fc576de, 0x2dafaf52);
    civicTree2[0x4f1af82b] = CivicTech(0x4f1af82b, 600, 7, 2, 2, 0x2dafaf52, 0xc123f7ca);
    civicTree2[0x6a547c82] = CivicTech(0x6a547c82, 600, 7, 4, 1, 0xc123f7ca);
    civicTree2[0xff83774a] = CivicTech(0xff83774a, 440, 7, 6, 2, 0xc123f7ca, 0xca671949);
    civicTree2[0x97dc359f] = CivicTech(0x97dc359f, 720, 8, 2, 1, 0x4f1af82b);
    civicTree2[0x8b2eb90b] = CivicTech(0x8b2eb90b, 720, 8, 4, 2, 0x4f1af82b, 0x6a547c82);
    civicTree2[0x406f8de7] = CivicTech(0x406f8de7, 800, 9, 0, 1, 0x97dc359f);
    civicTree2[0xad59ccbb] = CivicTech(0xad59ccbb, 1010, 9, 2, 1, 0x97dc359f);
    civicTree2[0xff3722b1] = CivicTech(0xff3722b1, 1010, 9, 3, 1, 0x8b2eb90b);
    civicTree2[0x80e1edeb] = CivicTech(0x80e1edeb, 800, 9, 5, 1, 0x8b2eb90b);
    civicTree2[0xc37bd04f] = CivicTech(0xc37bd04f, 1050, 10, 0, 1, 0x406f8de7);
    civicTree2[0xef73a3bf] = CivicTech(0xef73a3bf, 1210, 10, 2, 2, 0xad59ccbb, 0xff3722b1);
    civicTree2[0xfd48ca5f] = CivicTech(0xfd48ca5f, 1210, 10, 5, 1, 0xff3722b1);
    civicTree2[0xbe4ffc0f] = CivicTech(0xbe4ffc0f, 1540, 11, 0, 1, 0xc37bd04f);
    civicTree2[0x2caa7fe2] = CivicTech(0x2caa7fe2, 1540, 11, 2, 2, 0xc37bd04f, 0xef73a3bf);
    civicTree2[0x79f81414] = CivicTech(0x79f81414, 1540, 11, 4, 2, 0xef73a3bf, 0xfd48ca5f);
    civicTree2[0x1c3efcd9] = CivicTech(0x1c3efcd9, 1580, 12, 1, 1, 0x2caa7fe2);
    civicTree2[0x8b8ae4d2] = CivicTech(0x8b8ae4d2, 1640, 12, 2, 2, 0x2caa7fe2, 0x79f81414);
    civicTree2[0xb2385bb3] = CivicTech(0xb2385bb3, 1715, 13, 1, 1, 0x8b8ae4d2);
    civicTree2[0x82851f7d] = CivicTech(0x82851f7d, 1640, 13, 3, 1, 0x8b8ae4d2);
    civicTree2[0x548bf44b] = CivicTech(0x548bf44b, 1640, 13, 5, 1, 0x8b8ae4d2);
    civicTree2[0xb9627b15] = CivicTech(0xb9627b15, 1640, 13, 6, 1, 0x8b8ae4d2);
    civicTree2[0x114a3ece] = CivicTech(0x114a3ece, 1955, 14, 0, 1, 0xbe4ffc0f);
    civicTree2[0xd983b88e] = CivicTech(0xd983b88e, 2185, 14, 2, 1, 0x8b8ae4d2);
    civicTree2[0x417cffc4] = CivicTech(0x417cffc4, 2185, 14, 5, 1, 0x8b8ae4d2);
    civicTree2[0xe2f1a1a] = CivicTech(0xe2f1a1a, 2415, 15, 2, 1, 0xd983b88e);
    civicTree2[0x79dc5652] = CivicTech(0x79dc5652, 2415, 15, 4, 1, 0xd983b88e);
    civicTree2[0xe876b07c] = CivicTech(0xe876b07c, 2880, 16, 1, 2, 0x114a3ece, 0xe2f1a1a);
    civicTree2[0x641f05a3] = CivicTech(0x641f05a3, 2880, 16, 3, 2, 0xe2f1a1a, 0x79dc5652);
    civicTree2[0x98b1eb2] = CivicTech(0x98b1eb2, 2880, 16, 5, 2, 0x79dc5652, 0x417cffc4);
    civicTree2[0xe9f1674] = CivicTech(0xe9f1674, 3100, 17, 2, 2, 0xe876b07c, 0x641f05a3);
    civicTree2[0x1529745e] = CivicTech(0x1529745e, 3000, 17, 3, 2, 0x641f05a3, 0x98b1eb2);
    civicTree2[0xf8fd9c16] = CivicTech(0xf8fd9c16, 3000, 17, 4, 2, 0x641f05a3, 0x98b1eb2);
    civicTree2[0xd7b9aa57] = CivicTech(0xd7b9aa57, 3000, 17, 5, 2, 0x641f05a3, 0x98b1eb2);
    civicTree2[0x9258c6c] = CivicTech(0x9258c6c, 3200, 18, 1, 1, 0xe9f1674);
    civicTree2[0xe957a129] = CivicTech(0xe957a129, 3300, 19, 0, 1, 0x9258c6c);
    civicTree2[0x416e76d6] = CivicTech(0x416e76d6, 3300, 19, 3, 1, 0x9258c6c);
    civicTree2[0xda6156bf] = CivicTech(0xda6156bf, 3300, 19, 4, 1, 0x9258c6c);
    civicTree2[0x65ed92fe] = CivicTech(0x65ed92fe, 3300, 19, 5, 1, 0x9258c6c);
    civicTree2[0x3d095715] = CivicTech(0x3d095715, 3500, 20, 3, 3, 0x416e76d6, 0xda6156bf, 0x65ed92fe);
}
