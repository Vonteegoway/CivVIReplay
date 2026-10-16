#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include "Viewer.h"
#include <thread>
#include <cstring>
#include <cstdlib>
#include "Asset.h"
#include "Platform.h"
#include "SelfTest.h"

SaveLoader loader;

// Headless diagnostic: parse saves and print a summary, without opening a window.
static int dumpSaves(int count, char** files)
{
    loader.waitForCivPaths = false; // parsing only needs the gameplay database
    loader.init();
    if (!loader.isValidCivPath())
        fprintf(stderr, "note: Civilization VI install not found\n");
    for (int i = 0; i < count; i++)
        loader.paths.push(files[i]);
    try
    {
        loader.LoadSave(true);
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    if (loader.hasFatalError)
        return 1; // message already printed

    printf("database: %s\n", loader.assets.database.path.u8string().c_str());
    printf("image assets: %zu / %zu database types\n", loader.assets.imageAssets.size(), loader.assets.assets.size());
    for (int iState = 0; iState < loader.loadedStateCount; iState++)
    {
        const CivVIState& State = loader.States[iState];
        const CivVIGame& Game = *State.pGame;
        printf("\n[%d] turn %d, seed %d, timestamp %llu, width %d, %s %s %s, players %d%s%s\n",
            iState, State.turn, Game.seed, (unsigned long long)State.timestamp, State.width,
            Game.gamespeed, Game.mapSize, Game.map, Game.nbPlayer,
            State.error.empty() ? "" : ", error: ", State.error.c_str());
        for (size_t team = 0; team < Game.playersByTeams.size(); team++)
        {
            for (int iPlayer : Game.playersByTeams[team])
            {
                const PlayerInfo& info = Game.playersInfo[iPlayer];
                const Player& player = State.players[iPlayer];
                printf("  #%d %-24s %-30s %-24s cities %zu units %zu\n", iPlayer, info.civStr, info.leaderStr,
                    info.pseudo, player.cities.size(), player.units.size());
            }
        }
    }
    return loader.loadedStateCount == 0;
}

static int run()
{
    Platform::installCrashHandler();

    loader.init();

    std::thread first([]() {
        try
        {
            loader.LoadSave(false);
        }
        catch (const std::exception& e)
        {
            loader.setFatalError(std::string("Unexpected error while loading: ") + e.what());
        }
        catch (...)
        {
            loader.setFatalError("Unexpected error while loading");
        }
    });
    Viewer v(1280, 720);
    loader.stop = true;
    first.join();
    return 0;
}

#ifdef _WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd) {
    const bool commandLine = __argc > 1 && (!strcmp(__argv[1], "--dump") || !strcmp(__argv[1], "--selftest"));
    if (commandLine && AttachConsole(ATTACH_PARENT_PROCESS)) // /SUBSYSTEM:WINDOWS has no console of its own
    {
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }
    if (__argc > 2 && !strcmp(__argv[1], "--dump"))
        return dumpSaves(__argc - 2, __argv + 2);
    if (__argc > 1 && !strcmp(__argv[1], "--selftest"))
        return runSelfTest(__argc - 2, __argv + 2);
    return run();
}
#else
int main(int argc, char** argv) {
    if (argc > 2 && !strcmp(argv[1], "--dump"))
        return dumpSaves(argc - 2, argv + 2);
    if (argc > 1 && !strcmp(argv[1], "--selftest"))
        return runSelfTest(argc - 2, argv + 2);
    return run();
}
#endif
