#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"
#include <set>
#include <cstring>
#include <vector>
#include "Viewer.h"
#include "Asset.h"
#include <assert.h>
#include <filesystem>
#include <string>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include "resource.h"
#include "cereal/archives/binary.hpp"
#include "gif.h"
#include "zip_file.h"
#include "stdio.h"
#include "Crc.h"
#include <algorithm>
#include "Platform.h"
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <tlhelp32.h>
#endif

#define HEX_WIDTH 186.f
#define HEX_HEIGHT 215.f

imgui_addons::ImGuiFileBrowser Viewer::file_dialog = imgui_addons::ImGuiFileBrowser();
imgui_addons::ImGuiFileBrowser Viewer::file_dialog_civ = imgui_addons::ImGuiFileBrowser();

Viewer::Viewer(unsigned int Width, unsigned int Height) : dSimulator(*this) {
    scroll = ImVec2(0, 0);
    zoom = 0.5f;
    loaded = false;
    iCurrentState = 0;
    iSelectedTile = -1;
    bDrawMinimap = true;
    bDrawResume = true;
    minimapSize = 3;
    bDrawEconomicStats = false;
    bDrawMilitaryStats = false;
    bDrawTech = false;
    bDrawCityState = false;
    bDrawCivic = false;
    bDrawReligions = false;
    bDrawGreatPeople = false;
    iVisibilityFromPlayer = -1;
    fogOfWarMinimap = false;
    filter = 0;
    bAutomatic = false;
    timeBeforeNext = 2.0f;
    bLimitResumeToVisibility = false;
    bDrawPolicies = false;
    bDrawHuts = false;
    bDrawHardTech = false;
    bDrawDistrict = false;
    bDrawMapStats = false;
    bDrawGoldUsage = false;
    bDrawGameProgression = false;
    std::error_code ec;
    std::filesystem::path savesPath = Platform::civUserDir() / "Saves";
    if (std::filesystem::is_directory(savesPath / "Multi", ec))
        savesPath /= "Multi";
    else if (!std::filesystem::is_directory(savesPath, ec))
        savesPath = Platform::civUserDir();
    file_dialog.initCurrentPath((savesPath / "").string());
    hsicon = NULL;
    searchFocus = false;
    bShowYieldOnTile = false;

    Start(Width, Height);

}

void getFilesList(std::string filePath, std::string extension, std::queue<std::string>& returnFileName)
{
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };
    const std::string ext = lower(extension);

    // Saves are loaded (and stats computed) in order, so sort like Windows' FindFirstFile does on NTFS
    std::vector<std::string> files;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(filePath, ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec) && lower(it->path().extension().string()) == ext)
            files.push_back(it->path().string());
    }
    std::sort(files.begin(), files.end());
    for (const std::string& file : files)
        returnFileName.push(file);
}


static void* IniDefaultBBGPath_ReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
    return (void*)&((SaveLoader*)handler->UserData)->defaultBBGPath;
}

static void IniDefaultBBGPath_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
{
    ImGuiContext& g = *ctx;
    ImGuiWindowSettings* settings = (ImGuiWindowSettings*)entry;
    SaveLoader& loader = *((SaveLoader*)handler->UserData);
    loader.bbgPath = line;
}

static void IniDefaultBBGPath_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->appendf("[%s][%s]\n", handler->TypeName, handler->TypeName);
    buf->appendf("%s", ((SaveLoader*)handler->UserData)->bbgPath.string().c_str()); // not \n because it is last
}


static void* IniDefaultCivPath_ReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
    return (void*)&((SaveLoader*)handler->UserData)->defaultCivPath;
}

static void IniDefaultCivPath_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
{
    ImGuiContext& g = *ctx;
    ImGuiWindowSettings* settings = (ImGuiWindowSettings*)entry;
    SaveLoader& loader = *((SaveLoader*)handler->UserData);
    loader.civPath = line;
}

static void IniDefaultCivPath_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->appendf("[%s][%s]\n", handler->TypeName, handler->TypeName);
    buf->appendf("%s\n", ((SaveLoader*)handler->UserData)->civPath.string().c_str()); // not \n because it is last
}


static void* IniDefaultPath_ReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
    return (void*)&((imgui_addons::ImGuiFileBrowser*)handler->UserData)->current_path;
}

static void IniDefaultPath_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
{
    ImGuiContext& g = *ctx;
    ImGuiWindowSettings* settings = (ImGuiWindowSettings*)entry;
    ((imgui_addons::ImGuiFileBrowser*)handler->UserData)->initCurrentPath(line);
}

static void IniDefaultPath_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->appendf("[%s][%s]\n", handler->TypeName, handler->TypeName);
    buf->appendf("%s\n", ((imgui_addons::ImGuiFileBrowser*)handler->UserData)->current_path.c_str());
}

void Viewer::initIniHandler()
{
    ImGuiContext& g = *GImGui;
    {
        ImGuiSettingsHandler ini_handler;
        ini_handler.TypeName = "DefaultOpenPath";
        ini_handler.TypeHash = ImHashStr("DefaultOpenPath");
        ini_handler.ReadOpenFn = IniDefaultPath_ReadOpen;
        ini_handler.ReadLineFn = IniDefaultPath_ReadLine;
        ini_handler.WriteAllFn = IniDefaultPath_WriteAll;
        ini_handler.UserData = &Viewer::file_dialog;
        g.SettingsHandlers.push_back(ini_handler);
    }
    {
        ImGuiSettingsHandler ini_handler;
        ini_handler.TypeName = "DefaultCivPath";
        ini_handler.TypeHash = ImHashStr("DefaultCivPath");
        ini_handler.ReadOpenFn = IniDefaultCivPath_ReadOpen;
        ini_handler.ReadLineFn = IniDefaultCivPath_ReadLine;
        ini_handler.WriteAllFn = IniDefaultCivPath_WriteAll;
        ini_handler.UserData = &loader;
        g.SettingsHandlers.push_back(ini_handler);
    }
    {
        ImGuiSettingsHandler ini_handler;
        ini_handler.TypeName = "DefaultBBGPath";
        ini_handler.TypeHash = ImHashStr("DefaultBBGPath");
        ini_handler.ReadOpenFn = IniDefaultBBGPath_ReadOpen;
        ini_handler.ReadLineFn = IniDefaultBBGPath_ReadLine;
        ini_handler.WriteAllFn = IniDefaultBBGPath_WriteAll;
        ini_handler.UserData = &loader;
        g.SettingsHandlers.push_back(ini_handler);
    }

}

void Viewer::DrawTooltipImage(unsigned int type, float zoom)
{
    DrawImage(type, 0.f, 0.f, zoom, ImVec4(0.f, 0.f, 1.f, 1.f));
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", loader.assets[type].name);
        ImGui::EndTooltip();
    }
}

void setPixel(uint8_t* image, int xx, int yy, int width, ImVec4 color)
{
    uint8_t red = (uint8_t)roundf(255.0f * color.x);
    uint8_t grn = (uint8_t)roundf(255.0f * color.y);
    uint8_t blu = (uint8_t)roundf(255.0f * color.z);
    uint8_t alpha = (uint8_t)roundf(255.0f * color.w);

    uint8_t* pixel = &image[(yy * width + xx) * 4];
    pixel[0] = red;
    pixel[1] = grn;
    pixel[2] = blu;
    pixel[3] = alpha;

}
void drawHexagon(uint8_t* image, int size, int width, int xPixelHex, int yPixelHex, ImVec4 color)
{
    for (int k = 0; k < size; k++)
    {
        int pixelCount = k * 2 + 1;
        int y = yPixelHex + k;
        int xOffset = size - k;
        for (int l = 0; l < pixelCount; l++)
        {
            int x = xPixelHex + xOffset + l;
            setPixel(image, x, y, width, color);
        }
    }
    for (int k = 0; k < size; k++)
    {
        int pixelCount = size * 2 + 1;
        int y = yPixelHex + k + size;
        for (int l = 0; l < pixelCount; l++)
        {
            int x = xPixelHex + l;
            setPixel(image, x, y, width, color);
        }
    }
    for (int k = 0; k < size; k++)
    {
        int pixelCount = k * 2 + 1;
        int y = yPixelHex + (size - k - 1) + 2 * size;
        int xOffset = size - k;
        for (int l = 0; l < pixelCount; l++)
        {
            int x = xPixelHex + xOffset + l;
            setPixel(image, x, y, width, color);
        }
    }

}
void CreateGif(const char* gifPath, const SaveLoader& loader, int size, int speed)
{

    const CivVIGame& game = loader.Games.begin()->second;
    const CivVIState& state0 = *loader.States.begin();

    // Create a gif
    int height = state0.tiles.size() / state0.width;
    int pixelWidth = state0.width * ((size * 2) + 2) + size + 1;
    int pixelheight = (height + 1) * 2 * size;

    uint8_t* image = new uint8_t[pixelWidth * pixelheight * 4];
    memset(image, 0, pixelWidth * pixelheight * 4);

    GifWriter writer = {};
    GifBegin(&writer, gifPath, pixelWidth, pixelheight, 2, 8, true);

    int bottompixel = height * 2 * size - size;

    for (int frame = 0; frame < loader.States.size(); ++frame)
    {
        const CivVIState& state = loader.States[frame];
        for (int i = 0; i < state.width; i++)
        {
            for (int j = 0; j < height; j++)
            {
                const Tile& tile = state.tiles[i + j * state.width];

                ImColor color;
                if (tile.iPlayer < 64)
                {
                    if (tile.pCity)
                    {
                        color = state.pGame->playersInfo[tile.iPlayer].secondaryColor;
                        color.Value.w = 1.0f;
                    }
                    else
                    {
                        color = state.pGame->playersInfo[tile.iPlayer].primaryColor;
                        color.Value.w = 1.0f;
                    }

                }
                else if (tile.terrain == 0x47c909dd) //sea
                {
                    color = ImColor(18.f / 256.f, 18.f / 256.f, 34.f / 256.f, 1.0f);
                }
                else if (tile.terrain == 0x4a707a11) //coast and lake
                {
                    color = ImColor(20.f / 256.f, 35.f / 256.f, 50.f / 256.f, 1.0f);
                }
                else if (tile.terrain == 0x29b146f4 || tile.terrain == 0xe5031280 || tile.terrain == 0x67ea840f) //snow
                {
                    color = ImColor(80.f / 256.f, 90.f / 256.f, 90.f / 256.f, 1.0f);
                }
                else if (tile.terrain == 0xdf49ddbd || tile.terrain == 0xeb62b0f6 || tile.terrain == 0x48f29eab) //tundra
                {
                    color = ImColor(66.f / 256.f, 65.f / 256.f, 52.f / 256.f, 1.0f);
                }
                else if (tile.terrain == 0x5490bef9 || tile.terrain == 0xb94134b3 || tile.terrain == 0xe5a80a2c) //desert
                {
                    color = ImColor(100.f / 256.f, 80.f / 256.f, 45.f / 256.f, 1.0f);
                }
                else if (tile.terrain == 0x5f83b433 || tile.terrain == 0x6e9d0c70 || tile.terrain == 0x83e7c630) //grass
                {
                    color = ImColor(40.f / 256.f, 50.f / 256.f, 20.f / 256.f, 1.0f);
                }
                else //plain
                {
                    color = ImColor(60.f / 256.f, 65.f / 256.f, 30.f / 256.f, 1.0f);
                }

                int xPixelHex = i * ((size * 2) + 2);
                int yPixelHex = bottompixel - j * 2 * size;
                if (j % 2)
                {
                    xPixelHex += size + 1;
                }

                drawHexagon(image, size, pixelWidth, xPixelHex, yPixelHex, color.Value);

                if (tile.units.size())
                {
                    int iUnitPlayer = tile.units[0]->player->iPlayer;
                    int xMiddleHex = xPixelHex + size - 2;
                    int yMiddleHex = yPixelHex + size;

                    ImColor pColor = state.pGame->playersInfo[iUnitPlayer].primaryColor;
                    ImColor sColor = state.pGame->playersInfo[iUnitPlayer].secondaryColor;

                    setPixel(image, xMiddleHex, yMiddleHex, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 1, yMiddleHex, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 3, yMiddleHex, pixelWidth, pColor);
                    setPixel(image, xMiddleHex, yMiddleHex + 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 3, yMiddleHex + 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex, yMiddleHex + 2, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 3, yMiddleHex + 2, pixelWidth, pColor);
                    setPixel(image, xMiddleHex, yMiddleHex + 3, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 1, yMiddleHex + 3, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex + 3, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 3, yMiddleHex + 3, pixelWidth, pColor);

                    setPixel(image, xMiddleHex + 1, yMiddleHex - 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex - 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex - 1, yMiddleHex + 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 4, yMiddleHex + 1, pixelWidth, pColor);
                    setPixel(image, xMiddleHex - 1, yMiddleHex + 2, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 4, yMiddleHex + 2, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 1, yMiddleHex + 4, pixelWidth, pColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex + 4, pixelWidth, pColor);

                    setPixel(image, xMiddleHex + 1, yMiddleHex + 1, pixelWidth, sColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex + 1, pixelWidth, sColor);
                    setPixel(image, xMiddleHex + 1, yMiddleHex + 2, pixelWidth, sColor);
                    setPixel(image, xMiddleHex + 2, yMiddleHex + 2, pixelWidth, sColor);

                }
            }
        }
        // Write the frame to the gif
        //printf("Writing frame %d...\n", frame);
        GifWriteFrame(&writer, image, pixelWidth, pixelheight, speed);
    }

    delete[] image;

    // Write EOF
    GifEnd(&writer);
}

// Now inside any function
void Viewer::closeSaves()
{
    loader.Close();
    iCurrentState = 0;
    iSelectedTile = -1;
    iTempSelectedTile = -1;
    iVisibilityFromPlayer = -1;
    bAutomatic = false;
    dSimulator.enabled = false;
    dSimulator.districts.clear();
    dSimulator.iPlayer = 0;
    dSimulator.iCurrentDistrict = 0;
}

void Viewer::showMainMenu()
{
    bool open = false, openDirectory = false;
    bool saveCivReplay = false, openCivReplay = false;
    bool saveGif = false;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            if (ImGui::MenuItem("Open", NULL))
                open = true;
            if (ImGui::MenuItem("Open directory", NULL))
                openDirectory = true;
            if (ImGui::MenuItem("Close", NULL, false, loader.loadedStateCount > 0 || !loader.paths.empty()))
                closeSaves();


            if (ImGui::MenuItem("Save civreplay", NULL))
                saveCivReplay = true;
            if (ImGui::MenuItem("Open civreplay", NULL))
                openCivReplay = true;

            if (ImGui::MenuItem("Save gif", NULL))
                saveGif= true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Option"))
        {
            ImGui::Checkbox("Minimap", &bDrawMinimap);
            ImGui::SliderInt("Minimap Size", &minimapSize, 1, 10);
            ImGui::Checkbox("Add fog of war in minmap", &fogOfWarMinimap);
            ImGui::Checkbox("Resume", &bDrawResume);
            ImGui::Checkbox("Limit Resume to Selected Player", &bLimitResumeToVisibility);
            ImGui::Checkbox("Show yield on tiles (Y) (BETA)", &bShowYieldOnTile);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Panels"))
        {
            ImGui::Checkbox("Technologies (T)", &bDrawTech);
            ImGui::Checkbox("Civics (C)", &bDrawCivic);
            ImGui::Checkbox("City State (S)", &bDrawCityState);
            ImGui::Checkbox("Religion (R)", &bDrawReligions);
            ImGui::Checkbox("Great People (G)", &bDrawGreatPeople);
            ImGui::Checkbox("Policies (P)", &bDrawPolicies);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Map Filters"))
        {
            if (ImGui::MenuItem("Religions", "1", filter == FILTER_RELIGION))
                filter = filter == FILTER_RELIGION ? FILTER_NONE : FILTER_RELIGION;
            if (ImGui::MenuItem("Continents", "2", filter == FILTER_CONTINENT))
                filter = filter == FILTER_CONTINENT ? FILTER_NONE : FILTER_CONTINENT;
            if(ImGui::MenuItem("Appeal", "3", filter == FILTER_APPEAL))
                filter = filter == FILTER_APPEAL ? FILTER_NONE : FILTER_APPEAL;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Statistics"))
        {
            /*ImGui::Checkbox("Economic", &bDrawEconomicStats);
            ImGui::Checkbox("Military", &bDrawMilitaryStats);*/
            ImGui::Checkbox("Goody huts", &bDrawHuts);
            ImGui::Checkbox("Hard Tech", &bDrawHardTech);
            ImGui::Checkbox("District", &bDrawDistrict);
            ImGui::Checkbox("Map stats", &bDrawMapStats);
#if 0
            ImGui::Checkbox("Gold usage", &bDrawGoldUsage);
#endif
            ImGui::Checkbox("Game Progression", &bDrawGameProgression);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Simulator"))
        {
            if (ImGui::Checkbox("District adjacency", &dSimulator.enabled))
            {
                if (dSimulator.enabled)
                    dSimulator.Enable();
                else
                    dSimulator.Disable();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (open) {
        ImGui::OpenPopup("Open File");
    }
    if (openDirectory)
    {
        ImGui::OpenPopup("Open Directory");
    }


    if (saveCivReplay)
    {
        ImGui::OpenPopup("Save CivReplay");
    }
    if (openCivReplay)
    {
        ImGui::OpenPopup("Load CivReplay");
    }

    if (saveGif)
    {
        ImGui::OpenPopup("Save Gif");
    }

    try
    {
        bool isOpen = false;
        if (file_dialog.showFileDialog("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, isOpen, ImVec2(700, 310), ".Civ6Save"))
        {
            //LogA("Open file %s ...\n", file_dialog.selected_path.c_str());
            loader.mtx.lock();
            loader.paths.push(file_dialog.selected_path);
            loader.mtx.unlock();
        }
        if (isOpen)
            searchFocus = true;
        if (file_dialog.showFileDialog("Open Directory", imgui_addons::ImGuiFileBrowser::DialogMode::SELECT, isOpen, ImVec2(700, 310), ".Civ6Save"))
        {
            //LogA("Open directory %s ...\n", file_dialog.selected_path.c_str());
            loader.mtx.lock();
            std::string path = file_dialog.selected_path;
            getFilesList(path, ".Civ6Save", loader.paths);
            loader.mtx.unlock();
        }
        if (isOpen)
            searchFocus = true;
        if (file_dialog.showFileDialog("Save CivReplay", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, isOpen, ImVec2(700, 310), ".Civ6Replay"))
        {
            std::string saveFile = file_dialog.selected_path;
            std::string zipFile = file_dialog.selected_path;
            if (saveFile.find(".Civ6Replay") == std::string::npos)
            {
                saveFile += ".Civ6ReplayTemp";
                zipFile += ".Civ6Replay";
            }
            else
            {
                saveFile += "Temp";
            }

            std::ofstream output(saveFile, std::ios::binary);
            cereal::BinaryOutputArchive oarchive(output); // Create an output archive
            oarchive(loader); // Write the data to the archive
            output.close();

            {
                miniz_cpp::zip_file zip;
                zip.write(saveFile, "save.Civ6ReplayTemp");
                zip.save(zipFile);
            }
            std::remove(saveFile.c_str());
        }
        if (isOpen)
            searchFocus = true;
        if (file_dialog.showFileDialog("Load CivReplay", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, isOpen, ImVec2(700, 310), ".Civ6Replay"))
        {

            std::string saveFile = file_dialog.selected_path;
            std::string unzipPath = file_dialog.selected_path;

            while (1)
            {
                if (unzipPath.back() == '/' || unzipPath.back() == '\\')
                    break;
                unzipPath.pop_back();
            }

            miniz_cpp::zip_file file(saveFile);
            file.extractall(unzipPath);

            std::string unzipFile =  unzipPath + "save.Civ6ReplayTemp";

            std::ifstream input(unzipFile, std::ios::binary);
            cereal::BinaryInputArchive iarchive(input); // Create an input archive
            iarchive(loader); // Read the data from the archive
            input.close();

            std::remove(unzipFile.c_str());
        }
        if (isOpen)
            searchFocus = true;
        if (file_dialog.showFileDialog("Save Gif", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, isOpen, ImVec2(700, 310), ".gif"))
        {
            std::string saveFile = file_dialog.selected_path;
            if (saveFile.find(".gif") == std::string::npos)
            {
                saveFile += ".gif";
            }
            CreateGif(saveFile.c_str(), loader, 5, 30);
        }
        if (isOpen)
            searchFocus = true;
    }
    catch (std::exception e)
    {
        //LogA("Error in File dialog %s ...\n", e.what());
    }
}

bool Viewer::LoadAssets()
{
    if (!loader.endLoadCivDb)
        return false;
    //--
    int count = 0;

    //LogA("Load assets ...\n");

    std::vector<unsigned int>::iterator p;
    for (p = loader.assets.imageAssets.begin(); p != loader.assets.imageAssets.end(); p++)
    {
        Asset& a = loader.assets.assets[*p];
        if (!a.bImage)
            continue;
        std::string path = Platform::findImage(a.folderName, a.imageName);
        if (a.LoadAsset(path.c_str()))
            count++;

        if (count >= 256)
        {
            return false;
        }
    }
    //LogA("End Load assets %d\n", count);
    return true;

}

bool civIsRunning()
{
#ifndef _WIN32
    return false;
#else
    PROCESSENTRY32 pe32 = { 0 };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pe32.dwSize = sizeof(PROCESSENTRY32);
    Process32First(hSnap, &pe32);     // Can throw away, never an actual app

    int iDone = 1;

    while (iDone)    // go until out of Processes
    {
        iDone = Process32Next(hSnap, &pe32);
        if (strcmp(pe32.szExeFile, "CivilizationVI.exe") == 0)    // Did we find our process?
        {
            return true;
        }
    }

    return false;
#endif
}

void Viewer::DrawGUI(){
    if (!loader.fatalError.empty())
    {
        ImGui::OpenPopup("CivVIReplay cannot start");
        if (ImGui::BeginPopupModal("CivVIReplay cannot start", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", loader.fatalError.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("Run \"CivVIReplay --selftest\" from a terminal for details. Log folder: %s", Platform::configDir().u8string().c_str());
            ImGui::EndPopup();
        }
        return;
    }
    if (!loader.isValidCivPath())
    {
        ImGui::OpenPopup("Steam or civilization path");
        try
        {
            bool isOpen;
            if (file_dialog_civ.showFileDialog("Steam or civilization path", imgui_addons::ImGuiFileBrowser::DialogMode::SELECT, isOpen, ImVec2(700, 310), ".Civ6Save"))
            {
                //LogA("Steam or civilization path %s ...\n", file_dialog_civ.selected_path.c_str());
                loader.mtx.lock();
                loader.civPath = file_dialog_civ.selected_path;
                loader.mtx.unlock();
            }
        }
        catch (std::exception e)
        {
            //LogA("Error in File dialog %s ...\n", e.what());
        }
    }
    else if(!loader.isValidBBGPath())
    {
        ImGui::OpenPopup("BBG path");

        try
        {
            bool isOpen;
            if (file_dialog_civ.showFileDialog("BBG path", imgui_addons::ImGuiFileBrowser::DialogMode::SELECT, isOpen, ImVec2(700, 310), ".Civ6Save"))
            {
                //LogA("BBG path %s ...\n", file_dialog_civ.selected_path.c_str());
                loader.mtx.lock();
                loader.bbgPath = file_dialog_civ.selected_path;
                loader.mtx.unlock();
            }
        }
        catch (std::exception e)
        {
            //LogA("Error in File dialog %s ...\n", e.what());
        }
    }
    //LogA("Begin DrawGUI Valid path %s \n", loader.civPath.string().c_str());
    searchFocus = false;
    //--
    showMainMenu();
#ifdef _DEBUG
    //LogA("Draw Demo ...\n");
    ImGui::ShowDemoWindow();
#endif
    //--
    if (!loaded)
    {
        loaded = LoadAssets();
        return;
    }
    if (!loader.loadedStateCount)
        return;

    ImGui::SetNextWindowSize(WinSize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    if (ImGui::Begin("Map", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse))
    {
        CivVIState& State = loader.States[iCurrentState];
        DrawNavigation();

        uint64_t current = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
            ).count();

        uint64_t timeFromSave = (unsigned int)(current - loader.States[iCurrentState].timestamp);
#ifndef _DEBUG
        /*if (loader.States[iCurrentState].pGame->multiPlayer && timeFromSave < 30 * 60)
        {
            ImGui::Begin("Error");
            ImGui::Text("Save is too recent. Please wait a little bit");
            ImGui::End();
        }
        else if (civIsRunning())
        {
            ImGui::Begin("Error");
            ImGui::Text("Please close Civilization before running civreplay");
            ImGui::End();
        }
        else*/
#endif
        if (loader.States[iCurrentState].error == "")
        {
            if (loaded)
            {
                DrawMap();
            }
            DrawPackets();
            DrawInfo();
            if (bDrawMinimap)
                DrawMinimap();
            CivVIState& State = loader.States[iCurrentState];
            if (State.players.size())
            {
                if (bDrawResume)
                    DrawResume();
                if (bDrawEconomicStats)
                    DrawEconomicStats();
                if (bDrawMilitaryStats)
                    DrawMilitaryStats();
                if (bDrawCivic)
                    DrawCivicPanel();
                if (bDrawReligions)
                    DrawReligions();
                if (bDrawGreatPeople)
                    DrawGreatPeople();
                if (bDrawPolicies)
                    DrawPolicies();
                if (bDrawTech)
                    DrawTechPanel();
                if (bDrawCityState)
                    DrawCityStatePanel();
                if (bDrawHuts)
                    DrawHuts();
                if (bDrawHardTech)
                    DrawHardTech();
                if (bDrawDistrict)
                    DrawDistrict();
                if (bDrawMapStats)
                    DrawMapStats();
                if (bDrawGoldUsage)
                    DrawGoldUsage();
                if (bDrawGameProgression)
                    DrawGameProgression();
                dSimulator.Draw();
            }
            //--
            if (!searchFocus)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_1, 0))
                    filter = filter == FILTER_RELIGION ? FILTER_NONE : FILTER_RELIGION;
                if (ImGui::IsKeyPressed(ImGuiKey_2, 0))
                    filter = filter == FILTER_CONTINENT ? FILTER_NONE : FILTER_CONTINENT;
                else if (ImGui::IsKeyPressed(ImGuiKey_3, 0))
                    filter = filter == FILTER_APPEAL ? FILTER_NONE : FILTER_APPEAL;
                else if (ImGui::IsKeyPressed(ImGuiKey_T, 0))
                    bDrawTech = !bDrawTech;
                else if (ImGui::IsKeyPressed(ImGuiKey_C, 0))
                    bDrawCivic = !bDrawCivic;
                else if (ImGui::IsKeyPressed(ImGuiKey_S, 0))
                    bDrawCityState = !bDrawCityState;
                else if (ImGui::IsKeyPressed(ImGuiKey_R, 0))
                    bDrawReligions = !bDrawReligions;
                else if (ImGui::IsKeyPressed(ImGuiKey_P, 0))
                    bDrawPolicies = !bDrawPolicies;
                else if (ImGui::IsKeyPressed(ImGuiKey_G, 0))
                    bDrawGreatPeople = !bDrawGreatPeople;
                else if (ImGui::IsKeyPressed(ImGuiKey_Y, 0))
                    bShowYieldOnTile = !bShowYieldOnTile;
            }
            //--
        }
    }
    ImGui::End();

}

void Viewer::getPos(const Tile& Tile, float zoom, OUT float& posX, OUT float& posY)
{
    CivVIState& State = loader.States[iCurrentState];
    int x = (Tile.iTile % State.width);
    int y = (Tile.iTile / State.width);
    int reversedY = State.tiles.size() / State.width - y;
    posX = x * HEX_WIDTH;
    posY = (reversedY / 2) * HEX_HEIGHT * 3.f / 2.f;
    if (reversedY % 2) {
        posX += HEX_WIDTH/2;
        posY += HEX_HEIGHT * 3.f / 4.f;
    }

    posX *= zoom;
    posY *= zoom;
}

void Viewer::DrawHexagon(const ImColor& color, float zoom, const ImVec2& scroll, const ImVec2& pos)
{
    float hexagonePos[12] = {
        0.f, -104.f,
        90.f, -52.f,
        90.f, 52.f,
        0.f, 104.f,
        -90.f, 52.f,
        -90.f, -52.f
    };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 SelectedTilePos[6];
    for (int i = 0; i < 6; i++)
    {
        SelectedTilePos[i] = ImVec2(pos.x - scroll.x + (HEX_WIDTH / 2 + hexagonePos[i*2]) * zoom, pos.y - scroll.y + (HEX_HEIGHT / 2 + hexagonePos[i*2+1]) * zoom);
    }
    draw_list->AddConvexPolyFilled(SelectedTilePos, 6, color);
}

void Viewer::DrawRiver(const Tile& t, float posX, float posY)
{
    if (t.river == 0)
        return;

    ImVec2 windows_size = ImGui::GetWindowSize();

    float riverPos[6][4] = {
                    {0.f, -107.f, 93.f, -52.f},
                    {93.f, -52.f, 93.f, 52.f},
                    {93.f, 52.f, 0.f, 107.f},
                    {0.f, 107.f, -93.f, 52.f},
                    {-93.f, 52.f, -93.f, -52.f},
                    {-93.f, -52.f, 0.f, -107.f}
    };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->Flags = ImDrawListFlags_None;
    for (int i = 0; i < 6; i++)
    {
        if (!(t.river & (1 << i)))
            continue;
        draw_list->AddLine(ImVec2(posX + (HEX_WIDTH / 2 + riverPos[i][0]) * zoom, posY + (HEX_HEIGHT / 2 + riverPos[i][1]) * zoom), ImVec2(posX + (HEX_WIDTH / 2 + riverPos[i][2]) * zoom, posY + (HEX_HEIGHT / 2 + riverPos[i][3]) * zoom), ImGui::GetColorU32(ImVec4(62.f / 256.f, 48.f / 256.f, 10.f / 256.f, 0.8f)), 20.f * zoom);
        draw_list->AddLine(ImVec2(posX + (HEX_WIDTH / 2 + riverPos[i][0]) * zoom, posY + (HEX_HEIGHT / 2 + riverPos[i][1]) * zoom), ImVec2(posX + (HEX_WIDTH / 2 + riverPos[i][2]) * zoom, posY + (HEX_HEIGHT / 2 + riverPos[i][3]) * zoom), ImGui::GetColorU32(ImVec4(30.f / 256.f, 90.f / 256.f, 130.f / 256.f, 0.8f)), 15.f * zoom);
    }
    draw_list->Flags = ImDrawListFlags_AntiAliasedLines;
}

void Viewer::DrawBorders(const Tile& t, float posX, float posY)
{
    CivVIState& State = loader.States[iCurrentState];
    float borderPos[6][4] = {
                    {0.f, 107.f, 93.f, 52.f},
                    {93.f, 52.f, 93.f, -52.f},
                    {93.f, -52.f, 0.f, -107.f},
                    {0.f, -107.f, -93.f, -52.f},
                    {-93.f, -52.f, -93.f, 52.f},
                    {-93.f, 52.f, 0.f, 107.f}
    };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->Flags = ImDrawListFlags_None;
//    posX += HEX_WIDTH / 2;
    //posY += HEX_HEIGHT / 2;
    for (int i = 0; i < 6; i++)
    {
        if (t.iLine & (1 << i))
        {
            draw_list->AddLine(ImVec2(posX+ (HEX_WIDTH / 2 + borderPos[i][0]) * zoom, posY + (HEX_HEIGHT / 2 + borderPos[i][1]) * zoom), ImVec2(posX+ (HEX_WIDTH / 2 + borderPos[i][2]) * zoom, posY + (HEX_HEIGHT / 2 + borderPos[i][3]) * zoom), ImGui::GetColorU32(State.pGame->playersInfo[t.iPlayer].primaryColor), 20.f * zoom);
            draw_list->AddLine(ImVec2(posX+ (HEX_WIDTH / 2 + borderPos[i][0]) * zoom, posY + (HEX_HEIGHT / 2 + borderPos[i][1]) * zoom), ImVec2(posX+ (HEX_WIDTH / 2 + borderPos[i][2]) * zoom, posY + (HEX_HEIGHT / 2 + borderPos[i][3]) * zoom), ImGui::GetColorU32(State.pGame->playersInfo[t.iPlayer].secondaryColor), 4.f * zoom);
        }
    }
    draw_list->Flags = ImDrawListFlags_AntiAliasedLines;
}

bool Viewer::DrawImage(unsigned int type, float posX, float posY, float zoom, ImVec4 defaultColor, ImVec4 tintColor /*= ImVec4(1, 1, 1, 1)*/, bool bGrey)
{
    std::map<unsigned int, Asset>::iterator iter = loader.assets.assets.find(type);
    if (bGrey && iter != loader.assets.assets.end())
    {
        char folder[512];
        sprintf(folder, "%s//GREY", iter->second.folderName);
        type = Assets::GetCrc(folder, iter->second.imageName);
        iter = loader.assets.assets.find(type);
    }
    if (iter != loader.assets.assets.end() && iter->second.loaded)
    {
        loader.assets[type].Draw(zoom, tintColor);
        return true;
    }
    else
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(ImVec2(posX+ 50.f * zoom, posY+ 60.f * zoom), ImVec2(posX+ 130.f * zoom, posY+ 140.f * zoom), ImGui::GetColorU32(defaultColor));

        return false;
    }

}

ImVec4 HSVtoRGB(float H, float S, float V) {
    ImVec4 value;
    if (H > 360 || H < 0 || S>100 || S < 0 || V>100 || V < 0) {
        return value;
    }
    float s = S / 100;
    float v = V / 100;
    float C = s * v;
    float X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
    float m = v - C;
    float r, g, b;
    if (H >= 0 && H < 60) {
        r = C, g = X, b = 0;
    }
    else if (H >= 60 && H < 120) {
        r = X, g = C, b = 0;
    }
    else if (H >= 120 && H < 180) {
        r = 0, g = C, b = X;
    }
    else if (H >= 180 && H < 240) {
        r = 0, g = X, b = C;
    }
    else if (H >= 240 && H < 300) {
        r = X, g = 0, b = C;
    }
    else {
        r = C, g = 0, b = X;
    }
    value.x = (r + m);
    value.y = (g + m);
    value.z = (b + m);
    value.w = 1.0f;
    return value;
}

VISIBILITY Viewer::IsVisible(int iTile)
{
    CivVIState& State = loader.States[iCurrentState];
    if (iVisibilityFromPlayer == -1)
        return V_TOTAL;
    int iVisibilityFromTeam = iVisibilityFromPlayer ? State.pGame->playersInfo[iVisibilityFromPlayer].team : 0;
    if (State.players[iVisibilityFromPlayer].inSightCount[iTile])
        return V_TOTAL;
    if (State.teams[iVisibilityFromTeam].inSightCount[iTile])
        return V_TOTAL;
    if (State.players[iVisibilityFromPlayer].visible[iTile])
        return V_PARTIAL;
    if (State.teams[iVisibilityFromTeam].visible[iTile])
        return V_PARTIAL;
    return V_NONE;
}

int getNeighbour(int iNeighbour, int iTile, const CivVIState& State)
{

    int x = iTile % State.width;
    int y = iTile / State.width;

    int newX = x;
    int newY = y;

    if (iNeighbour == 0)
        newX--;
    if (iNeighbour == 1)
    {
        newY++;
        newX--;
    }
    if (iNeighbour == 2)
        newY++;
    if (iNeighbour == 3)
        newX++;
    if (iNeighbour == 4)
        newY--;
    if (iNeighbour == 5)
    {
        newY--;
        newX--;
    }

    if (newY != y && y % 2 != 0)
        newX++;

    if (newY < 0 || newY >= State.tiles.size() / State.width)
        return -1;

    if (newX < 0)
        newX += State.width;
    if (newX >= State.width)
        newX -= State.width;

    return newX + newY * State.width;

}


const char* types[] = { "", "Bool", "Int", "3","4","String","UTF String","7","8","9","OBJECT","ARRAY","12","13","14","15","16","17","18","19","TIMESTAMP","21","22","23","COMPRESSED" };
void Viewer::DrawMap() {
    CivVIState& State = loader.States[iCurrentState];

    ImGuiIO& io = ImGui::GetIO();
    ImVector<ImFont*> Fonts = io.Fonts->Fonts;
    if (zoom < 0.3f)
        ImGui::PushFont(Fonts[1]);
    else if(zoom<0.5f)
        ImGui::PushFont(Fonts[2]);
    else if (zoom < 0.6f)
        ImGui::PushFont(Fonts[3]);
    else if (zoom < 0.8f)
        ImGui::PushFont(Fonts[0]);
    else if (zoom < 1.f)
        ImGui::PushFont(Fonts[4]);
    else
        ImGui::PushFont(Fonts[5]);

    ImVec2 tilePos[10000];
    bool isOnScreen[10000];

    //--
    if (ImGui::GetIO().MouseWheel != 0.0f && ImGui::IsWindowHovered())
    {
        float previous = zoom;
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        zoom += ImGui::GetIO().MouseWheel * 0.1;
        zoom = std::min(std::max(0.1f, zoom), 2.f);
        ImVec2 relativePos = ImVec2(mousePos.x + windowPos.x, mousePos.y + windowPos.y);
        scroll.x = (relativePos.x + scroll.x) * (zoom / previous) - relativePos.x;
        scroll.y = (relativePos.y + scroll.y) * (zoom / previous) - relativePos.y;
    }
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
    {
        mousePos = ImGui::GetMousePos();
    }
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        ImVec2 tempPos = ImGui::GetMousePos();
        scroll.x += mousePos.x - tempPos.x;
        scroll.y += mousePos.y - tempPos.y;
        mousePos = tempPos;
    }
    /*float maxX = ImGui::GetWindowScrollMaxX(ImGui::GetCurrentWindow());
    float maxY = ImGui::GetWindowScrollMaxY(ImGui::GetCurrentWindow());
    scroll.x = min(max(0.f, scroll.x), maxX);
    scroll.y = min(max(0.f, scroll.y), maxY);*/
    /*ImGui::SetScrollX(scroll.x);
    ImGui::SetScrollY(scroll.y);*/

    float mapWidth = State.width * HEX_WIDTH * zoom;
    if (scroll.x < 0)
        scroll.x += mapWidth;
    if (scroll.x > mapWidth)
        scroll.x -= mapWidth;
    sizeMap = ImGui::GetWindowSize();
    //-------------------------
    ImColor roadColors[4] = {
       ImColor(0xCD5e3719),
       ImColor(0xCD836955),
       ImColor(0xCDA89B91),
       ImColor(0xCDCDCDCD)
    };
    //-------------------------
    for (int iTile = 0; iTile < State.tiles.size(); iTile++)
    {
        const Tile& t = State.tiles[iTile];
        //--
        getPos(t, zoom, OUT tilePos[iTile].x, OUT tilePos[iTile].y);
        ImVec4 tintColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        VISIBILITY visi = IsVisible(t.iTile);
        if (visi == V_NONE)
        {
            tintColor = ImVec4(0.f, 0.f, 0.f, 1.f);
        }
        else if (visi == V_PARTIAL)
        {
            tintColor = ImVec4(1.f, 1.f, 1.f, 0.3f);
        }
        //--
        float mapWidth = State.width * HEX_WIDTH * zoom;
        tilePos[iTile].x -= scroll.x;
        tilePos[iTile].y -= scroll.y;
        if (tilePos[iTile].x > mapWidth - HEX_WIDTH)
            tilePos[iTile].x -= mapWidth;
        if (tilePos[iTile].x < -HEX_WIDTH)
            tilePos[iTile].x += mapWidth;
        //--
        float posX = tilePos[iTile].x;
        float posY = tilePos[iTile].y;
        //--
        ImGui::SetCursorPos(ImVec2(posX, posY));
        //--
        DrawImage(t.terrain, posX, posY, zoom, ImVec4(0.f, 0.f, 1.f, 1.f), tintColor);

        if (ImGui::IsItemHovered())
        {
            if (ImGui::IsMouseClicked(0))
            {
                iTempSelectedTile = iTile;
            }
            if (ImGui::IsMouseReleased(0))
            {
                if (iTempSelectedTile == iTile)
                    iSelectedTile = iTile;
            }
        }
        if (t.road != 0xff)
        {
            for (int iNeighbor = 0; iNeighbor < 6; iNeighbor++)
            {
                int neighbor = getNeighbour(iNeighbor, iTile, State);
                if (neighbor != -1 && neighbor < iTile && State.tiles[neighbor].road != 0xff)
                {
                    ImVec2 myPos = tilePos[iTile];
                    ImVec2 neighborPos = tilePos[neighbor];
                    ImVec2 shift = ImVec2(HEX_WIDTH * zoom / 2, HEX_HEIGHT * zoom / 2);
                    myPos.x += shift.x;
                    myPos.y += shift.y;
                    neighborPos.x += shift.x;
                    neighborPos.y += shift.y;
                    if (abs(myPos.x - neighborPos.x) > HEX_WIDTH * 2 * zoom)
                    {
                        float mapWidth = State.width * HEX_WIDTH * zoom;
                        if (myPos.x > neighborPos.x)
                        {
                            myPos.x -= mapWidth;
                        }
                        else
                        {
                            neighborPos.x -= mapWidth;
                        }
                    }
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedLines;
                    int roadMax = std::max(t.road, State.tiles[neighbor].road);
                    draw_list->AddLine(neighborPos, myPos, roadColors[roadMax], 4.f);
                }
            }
        }
    }

    //-------------------------
    for (int iTile = 0; iTile < State.tiles.size(); iTile++)
    {
        const Tile& t = State.tiles[iTile];
        ImVec4 tintColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        VISIBILITY visi = IsVisible(t.iTile);
        if (visi == V_NONE)
        {
            tintColor = ImVec4(0.f, 0.f, 0.f, 1.f);
        }
        else if (visi == V_PARTIAL)
        {
            tintColor = ImVec4(1.f, 1.f, 1.f, 0.3f);
        }
        //--
        float posX = tilePos[iTile].x;
        float posY = tilePos[iTile].y;
        //--
        if (t.feature != 0xFFFFFFFF)
        {
            ImGui::SetCursorPos(ImVec2(posX + 0.1f, posY + 0.1f));
            DrawImage(t.feature, posX, posY, zoom, ImVec4(0.f, 0.f, 1.f, 1.f), tintColor);
        }
        //--
        if (t.improvment != 0xffffffff)
        {
            ImGui::SetCursorPos(ImVec2(posX, posY));
            if ((t.pillage & 1) == 1)
                tintColor.w *= 0.5f;
            DrawImage(t.improvment, posX, posY, zoom*1.0, ImVec4(0.f, 0.f, 1.f, 1.f), tintColor);
        }
        //--
        if (t.resource != 0xFFFFFFFF)
        {
            ImGui::SetCursorPos(ImVec2(posX + 60.f * zoom, posY + 110.f * zoom));
            DrawImage(t.resource, posX, posY, zoom*0.3, ImVec4(0.f, 0.f, 1.f, 1.f), tintColor);
        }
        //--
        if (t.wonder != 0xffffffff)
        {
            ImGui::SetCursorPos(ImVec2(posX, posY));
            if (t.iCity != 0xff && t.iPlayer < State.players.size())
            {
                std::map<unsigned int, unsigned int>& build = State.players[t.iPlayer].pCities[t.iCity]->built;
                std::map<unsigned int, unsigned int>::iterator it = build.find(t.wonder);
                if (it == build.end() || it->second == 0xffff)
                {
                    tintColor.w *= 0.5f;
                }

            }

            DrawImage(t.wonder, posX, posY, zoom * 1.0, ImVec4(0.f, 0.f, 1.f, 1.f), tintColor);
        }
        //--
        if (bShowYieldOnTile)
        {
            int layout = t.yields.size() - 1;

            float layoutYields[6][12] = {
                            {75.f, 50.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
                            {50.f, 50.f, 100.f, 50.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
                            {20.f, 50.f, 70.f, 50.f, 120.f, 50.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
                            {20.f, 40.f, 70.f, 40.f, 120.f, 40.f, 75.f, 80.f, 0.f, 0.f, 0.f, 0.f},
                            {20.f, 40.f, 70.f, 40.f, 120.f, 40.f, 50.f, 80.f, 100.f, 80.f, 0.f, 0.f},
                            {20.f, 40.f, 70.f, 40.f, 120.f, 40.f, 30.f, 80.f, 70.f, 80.f, 110.f, 80.f}
            };
            int count = 0;
            for (std::map<unsigned int, unsigned int>::const_iterator it = t.yields.begin(); it != t.yields.end(); it++)
            {
                ImGui::SetCursorPos(ImVec2(posX + layoutYields[layout][count * 2] * zoom, posY + layoutYields[layout][count * 2 + 1] * zoom));
                DrawImage(it->first, 0.f, 0.f, zoom * 2.f, ImVec4(1.f, 1.f, 1.f, 1.f));
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                float numberPosX = layoutYields[layout][count * 2] + 30;
                float numberPosY = layoutYields[layout][count * 2 + 1] + 25;
                draw_list->AddRectFilled(ImVec2(posX + (numberPosX - 5.f) * zoom, posY + (numberPosY - 5.f) * zoom), ImVec2(posX + (numberPosX + 15.f) * zoom, posY + (numberPosY + 15.f) * zoom), ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.5f)));
                ImGui::SetCursorPos(ImVec2(posX + numberPosX * zoom, posY + numberPosY * zoom));
                ImGui::Text("%d", it->second);
                count++;
            }
        }
        //--
    }
    //-------------------------
    for (int iTile = 0; iTile < State.tiles.size(); iTile++)
    {
        const Tile& t = State.tiles[iTile];
        ImVec4 tintColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        VISIBILITY visi = IsVisible(t.iTile);
        //--
        float posX = tilePos[iTile].x;
        float posY = tilePos[iTile].y;
        //--
        if (visi == V_NONE)
            continue;
        DrawRiver(t, posX, posY);
        //--

    }
    //-------------------------
    for (int iTile = 0; iTile < State.tiles.size(); iTile++)
    {
        const Tile& t = State.tiles[iTile];
        VISIBILITY visi = IsVisible(t.iTile);
        if (visi == V_NONE)
            continue;
        float posX = tilePos[iTile].x;
        float posY = tilePos[iTile].y;
        if (t.iPlayer < State.players.size())
        {
            DrawBorders(t, posX, posY);
        }
        //--
        if (t.pDistrict && t.pDistrict->type != 1660007689) // avoid wonder
        {
            const District& d = *t.pDistrict;
            ImGui::SetCursorPos(ImVec2(posX, posY));
            ImVec4 tint = ImVec4(1, 1, 1, 1);
            bool bGrey = (!d.built || d.pillage);
            DrawImage(d.type, posX, posY, zoom, ImVec4(0.f, 1.f, 0.f, 1.f), tint, bGrey);
            if (d.yieldType != 0 && d.totalProxi >= 1)
            {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(ImVec2(posX + 50.f * zoom, posY + 130.f * zoom), ImVec2(posX + 130.f * zoom, posY + 160.f * zoom), ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.7f)));

                ImGui::SetCursorPos(ImVec2(posX + 60.f * zoom, posY + 130.f * zoom));
                DrawImage(d.yieldType, posX, posY, zoom*1.5f, ImVec4(0.f, 1.f, 0.f, 1.f), tint);

                ImVec4 Color;
                switch ((unsigned int)d.yieldType)
                {
                    case 0x7f0f8e76: //Faith
                    {
                        Color = ImVec4(1.f, 1.f, 1.f, 1.f);
                        break;
                    }
                    case 0xbb19ce0a: //prod
                    {
                        Color = ImVec4(145.f/256.f, 51.f/256.f, 0.f, 1.f);
                        break;
                    }
                    case 0xb761634a: //gold
                    {
                        Color = ImVec4(229.f/256.f, 213.f/256.f, 66.f/256.f, 1.f);
                        break;
                    }
                    case 0xeb3f55e8: //culture
                    {
                        Color = ImVec4(1.f, 0.f, 1.f, 1.f);
                        break;
                    }
                    case 0x3a81f8ab: //science
                    {
                        Color = ImVec4(95.f/256.f, 145.f/256.f, 1.f, 1.f);
                        break;
                    }
                    default:
                        break;
                }
                ImGui::SetCursorPos(ImVec2(posX + 100.f * zoom, posY + 140.f * zoom));
                ImGuiIO& io = ImGui::GetIO();
                ImGui::TextColored(Color, "+%d", d.totalProxi);
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    for (std::map<std::string, std::pair<int,float>>::const_iterator it = d.proxi.begin(); it != d.proxi.end(); it++)
                    {
                        ImGui::Text("+%.1f x %d %s", it->second.second, it->second.first, it->first.c_str());
                    }
                    ImGui::EndTooltip();
                }

            }
        }
    }
    //-------------------------
    if (filter != FILTER_NONE)
    {
        for (int iTile = 0; iTile < State.tiles.size(); iTile++)
        {
            const Tile& t = State.tiles[iTile];
            VISIBILITY visi = IsVisible(t.iTile);
            if (visi == V_NONE)
                continue;
            ImColor color;
            switch (filter)
            {
            case FILTER_RELIGION:
            {
                ImVec4 colors[] =
                {
                    ImVec4(229.f / 255.f, 117.f / 255.f, 116.f / 255.f, 1.f),
                    ImVec4(255.f / 255.f, 178.f / 255.f, 60.f / 255.f, 1.f),
                    ImVec4(234.f / 255.f, 225.f / 255.f, 157.f / 255.f, 1.f),
                    ImVec4(121.f / 255.f, 224.f / 255.f, 119.f / 255.f, 1.f),
                    ImVec4(125.f / 255.f, 236.f / 255.f, 227.f / 255.f, 1.f),
                    ImVec4(116.f / 255.f, 163.f / 255.f, 243.f / 255.f, 1.f),
                    ImVec4(183.f / 255.f, 128.f / 255.f, 230.f / 255.f, 1.f),
                    ImVec4(255.f / 255.f, 153.f / 255.f, 255.f / 255.f, 1.f),
                    ImVec4(249.f / 255.f, 249.f / 255.f, 249.f / 255.f, 1.f)
                };
                if (t.iCity != 0xff && t.iPlayer < State.players.size())
                {
                    const City& otherCity = *State.players[t.iPlayer].pCities[t.iCity];
                    if (religionsColor.find(otherCity.religion) == religionsColor.end())
                        religionsColor[otherCity.religion] = colors[religionsColor.size()];
                    if (otherCity.religion == -1)
                        break;
                    color = religionsColor[otherCity.religion];
                }
                break;

            }
            case FILTER_APPEAL:
            {
                if (t.appeal >= 4)
                {
                    color.Value.x = 97.f / 255.f;
                    color.Value.y = 191.f / 255.f;
                    color.Value.z = 34.f / 255.f;
                }
                else if (t.appeal >= 2)
                {
                    color.Value.x = 121.f / 255.f;
                    color.Value.y = 224.f / 255.f;
                    color.Value.z = 119.f / 255.f;
                }
                else if (t.appeal >= -1)
                {
                    color.Value.x = 174.f / 255.f;
                    color.Value.y = 174.f / 255.f;
                    color.Value.z = 174.f / 255.f;

                }
                else if (t.appeal >= -3)
                {
                    color.Value.x = 255.f / 255.f;
                    color.Value.y = 129.f / 255.f;
                    color.Value.z = 18.f / 255.f;
                }
                else
                {
                    color.Value.x = 202.f / 255.f;
                    color.Value.y = 20.f / 255.f;
                    color.Value.z = 21.f / 255.f;
                }
                break;
            }
            case FILTER_CONTINENT:
            {
                ImVec4 colors[] =
                {
                    ImVec4(229.f / 255.f, 117.f / 255.f, 116.f / 255.f, 1.f),
                    ImVec4(255.f / 255.f, 178.f / 255.f, 60.f / 255.f, 1.f),
                    ImVec4(234.f / 255.f, 225.f / 255.f, 157.f / 255.f, 1.f),
                    ImVec4(121.f / 255.f, 224.f / 255.f, 119.f / 255.f, 1.f),
                    ImVec4(125.f / 255.f, 236.f / 255.f, 227.f / 255.f, 1.f),
                    ImVec4(116.f / 255.f, 163.f / 255.f, 243.f / 255.f, 1.f),
                    ImVec4(183.f / 255.f, 128.f / 255.f, 230.f / 255.f, 1.f),
                    ImVec4(255.f / 255.f, 153.f / 255.f, 255.f / 255.f, 1.f),
                    ImVec4(249.f / 255.f, 249.f / 255.f, 249.f / 255.f, 1.f)
                };
                if (continentsColor.find(t.continent) == continentsColor.end())
                    continentsColor[t.continent] = colors[continentsColor.size()];
                if (t.continent == -1)
                    continue;
                color = continentsColor[t.continent];
                break;
            }
            default:
                break;
            }

            color.Value.w = 0.5f;
            DrawHexagon(color, zoom, ImVec2(), tilePos[iTile]);
        }
    }
    //-------------------------
    for (int iTile = 0; iTile < State.tiles.size(); iTile++)
    {
        const Tile& t = State.tiles[iTile];
        VISIBILITY visi = IsVisible(t.iTile);
        if (visi == V_NONE)
            continue;
        float posX = tilePos[iTile].x;
        float posY = tilePos[iTile].y;
        //--
        const float groupPos[6][12] =
        {
            {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
            {-40.f, 0.f, 40.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
            {-40.f, -20.f, 40.f, -20.f, 0.f, 40.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
            {-40.f, -20.f, 40.f, -20.f, -40.f, 40.f, 40.f, 40.f, 0.f, 0.f, 0.f, 0.f},
            {-40.f, -20.f, 40.f, -20.f, -40.f, 40.f, 40.f, 40.f, 0.f, 80.f, 0.f, 0.f},
            {-40.f, -20.f, 40.f, -20.f, -40.f, 40.f, 40.f, 40.f, -40.f, 80.f, 40.f, 80.f},
        };
        //--
        if (t.pDistrict && t.pDistrict->type != 1660007689) // avoid wonder
        {
            const District& d = *t.pDistrict;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            // Health
            if (d.built && (d.type == 0x8a32007d || d.type == 0x66db35b9 || d.type == 0xb45b1b15 || d.type == 0x66fc953d || d.type == 0x79dd476d))
            {
                ImVec2 nameSize = d.type == 0x8a32007d ? ImVec2(0, 0) : ImVec2(0, 60.f);
                draw_list->AddRectFilled(ImVec2(posX, posY+ (nameSize.y - 10.f) * zoom), ImVec2(posX+ 180.f * zoom, posY+ (nameSize.y + 10.f) * zoom), ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 1.f)));
                int width = (3.f - 178.f) / 200 * d.damage + 178.f;
                draw_list->AddRectFilled(ImVec2(posX + 3.f * zoom, posY+ (nameSize.y - 8.f) * zoom), ImVec2(posX + width * zoom, posY+ (nameSize.y + 8.f) * zoom), ImGui::GetColorU32(HSVtoRGB(200.f - d.damage, 80.f, 80.f)));
                //Wall
                if (d.wall != 0 || d.wallDamage != 0)
                {
                    draw_list->AddRectFilled(ImVec2(posX, posY+ (nameSize.y - 30.f) * zoom), ImVec2(posX+ 180.f * zoom, posY+ (nameSize.y - 10.f) * zoom), ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 1.f)));
                    int width = (3.f - 178.f) / 100 * d.wallDamage + 178.f;
                    draw_list->AddRectFilled(ImVec2(posX + 3.f * zoom, posY+ (nameSize.y - 28.f) * zoom), ImVec2(posX + width * zoom, posY+ (nameSize.y - 12.f) * zoom), ImGui::GetColorU32(ImVec4(0.5f, 0.92f, 0.88f, 1.f)));

                }
            }
        }
        //--
        if (visi == V_TOTAL)
        {
            for (int iUnit = 0; iUnit < t.units.size(); iUnit++)
            {
                const Unit& u = *t.units[iUnit];
                const PlayerInfo& pInfo = State.pGame->playersInfo[u.player->iPlayer];
                ImVec2 relativePosGroup = ImVec2(posX + (50.f + groupPos[t.units.size() - 1][iUnit * 2]) * zoom, posY + (60.f + groupPos[t.units.size() - 1][(iUnit * 2) + 1]) * zoom);
                ImGui::SetCursorPos(relativePosGroup);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 finalPosA = ImVec2(relativePosGroup.x, relativePosGroup.y);
                ImVec2 finalPosB = finalPosA;
                finalPosB.x += 80.f * zoom;
                finalPosB.y += 80.f * zoom;
                draw_list->AddRectFilled(finalPosA, finalPosB,ImGui::GetColorU32(pInfo.primaryColor));
                DrawImage(u.type, posX, posY, zoom * 0.3f, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                //Damage
                if (u.damage != 0)
                {
                    draw_list->AddRectFilled(ImVec2(finalPosA.x - 20.f * zoom, finalPosA.y + 0.f * zoom), ImVec2(finalPosA.x + 0.f * zoom, finalPosA.y + 80.f * zoom), ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 1.f)));
                    int height = (78.f - 2.f) / 100 * u.damage - 2.f;
                    draw_list->AddRectFilled(ImVec2(finalPosA.x - 18.f * zoom, finalPosA.y + height * zoom), ImVec2(finalPosA.x - 2.f * zoom, finalPosA.y + 78.f * zoom), ImGui::GetColorU32(HSVtoRGB(100.f - u.damage, 80.f, 80.f)));
                }
                //level
                if (u.level > 1)
                {
                    ImVec2 levelPosA = ImVec2(finalPosA.x + 80.f * zoom, finalPosA.y + 20.f * zoom);
                    ImVec2 levelPosB = ImVec2(levelPosA.x + 30.f * zoom, levelPosA.y + 40.f * zoom);
                    draw_list->AddRectFilled(levelPosA, levelPosB, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.8f)));
                    ImGui::SetCursorPosX(relativePosGroup.x + 90.f * zoom);
                    ImGui::SetCursorPosY(relativePosGroup.y + 25.f * zoom);
                    ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%d", u.level - 1);
                }
                //army
                if (u.army != 0 && u.army != 0xffffffff)
                {
                    if (u.army == 1)
                    {
                        ImGui::SetCursorPos(ImVec2(relativePosGroup.x + 40.f * zoom, relativePosGroup.y - 5.f * zoom));
                        DrawImage(loader.assets.starIcon, relativePosGroup.x, relativePosGroup.y, zoom, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                        ImGui::SetCursorPos(ImVec2(relativePosGroup.x + 20.f * zoom, relativePosGroup.y - 5.f * zoom));
                        DrawImage(loader.assets.starIcon, relativePosGroup.x, relativePosGroup.y, zoom, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                    }
                    else if (u.army == 2)
                    {
                        ImGui::SetCursorPos(ImVec2(relativePosGroup.x + 40.f * zoom, relativePosGroup.y - 5.f * zoom));
                        DrawImage(loader.assets.starIcon, relativePosGroup.x, relativePosGroup.y, zoom, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                        ImGui::SetCursorPos(ImVec2(relativePosGroup.x + 20.f * zoom, relativePosGroup.y - 5.f * zoom));
                        DrawImage(loader.assets.starIcon, relativePosGroup.x, relativePosGroup.y, zoom, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                        ImGui::SetCursorPos(ImVec2(relativePosGroup.x + 0.f * zoom, relativePosGroup.y - 5.f * zoom));
                        DrawImage(loader.assets.starIcon, relativePosGroup.x, relativePosGroup.y, zoom, ImVec4(1.f, 0.f, 0.f, 1.f), pInfo.secondaryColor);
                    }
                    else
                    {
                        assert(0); //??
                    }
                }
            }
        }
        //--
        if (t.pCity)
        {
            const City& c = *t.pCity;
            const PlayerInfo& pInfo = State.pGame->playersInfo[c.player->iPlayer];
            float posX = tilePos[iTile].x;
            float posY = tilePos[iTile].y;
            ImGui::SetCursorPos(ImVec2(posX + 10.f * zoom, posY + 20.f * zoom));
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImVec2(posX- 50.f * zoom, posY+ 10.f * zoom), ImVec2(posX+ 220.f * zoom, posY+ 60.f * zoom), ImGui::GetColorU32(pInfo.primaryColor), 7.0f);
            ImGui::TextColored(pInfo.secondaryColor, "%s", c.pretty_name);
            //districtsInfo.districts[0].Draw(Vulkan, zoom * 0.5f, ImVec4(1, 0, 0, 1));
            if (!c.currentProds.empty())
            {
                ImGui::SetCursorPos(ImVec2(posX + 140.f * zoom, posY + 1.0f * zoom));
                ImVec4 tint = ImVec4(1, 1, 1, 1);
                if (strcmp(loader.assets[c.currentProds[0]].folderName,"unit") == 0)
                    tint = pInfo.secondaryColor;
                DrawImage(c.currentProds[0], posX, posY, zoom * 0.25f, ImVec4(0.f, 0.f, 1.f, 1.f), tint);
            }
            if (c.governor)
            {
                ImGui::SetCursorPos(ImVec2(posX + 200.f * zoom, posY + 9.0f * zoom));
                DrawImage(c.governor->type, posX, posY, zoom * 0.25f, ImVec4(0.f, 0.f, 1.f, 1.f));
                ImGui::SetCursorPos(ImVec2(posX + 220.f * zoom, posY + 10.0f * zoom));
                int turnToGo = loader.assets.governorTurnToGo[c.governor->type];
                if (turnToGo - c.governor->turn)
                    ImGui::Text("%d", turnToGo - c.governor->turn);

                ImVec2 levelPosA = ImVec2(posX + 250.f * zoom, posY + 20.f * zoom);
                ImVec2 levelPosB = ImVec2(levelPosA.x + 20.f * zoom, levelPosA.y + 30.f * zoom);
                draw_list->AddRectFilled(levelPosA, levelPosB, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.8f)));
                ImGui::SetCursorPosX(posX + 255.f * zoom);
                ImGui::SetCursorPosY(posY + 25.f * zoom);
                ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%d", c.governor->promotions.size());
            }
            ImGui::SetCursorPos(ImVec2(posX - 40.f * zoom, posY + 20.0f * zoom));
            ImGui::TextColored(pInfo.secondaryColor, "%d", c.population);

        }
    }
    //---
    if (iSelectedTile != -1)
    {
        DrawHexagon(ImColor(1.0f, 0.f, 0.f, 0.2f), zoom, ImVec2(), tilePos[iSelectedTile]);
    }
    ImGui::PopFont();
}

void Viewer::DrawMinimap() {
    CivVIState& State = loader.States[iCurrentState];
    float zoomMinimap = (float)minimapSize / 100;

    float width = State.width * HEX_WIDTH * zoomMinimap;
    float height = ((State.tiles.size() / State.width) + 10) * HEX_HEIGHT * 2/3 * zoomMinimap;

    ImGui::SetNextWindowPos(ImVec2(0.f, WinSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    if (ImGui::Begin("Minimap", NULL, ImGuiWindowFlags_NoMove| ImGuiWindowFlags_NoDecoration))
    {
        ImVec2 pos = ImGui::GetWindowPos();
        pos.x = -pos.x;
        pos.y = -pos.y;
        for (int iTile = 0; iTile < State.tiles.size(); iTile++)
        {
            const Tile& t = State.tiles[iTile];
            float alpha = 1.0f;
            if (fogOfWarMinimap && iVisibilityFromPlayer != -1 && !State.players[iVisibilityFromPlayer].visible[t.iTile])
            {
                alpha = 0.f;
            }
            else if (fogOfWarMinimap && iVisibilityFromPlayer != -1 && !State.players[iVisibilityFromPlayer].inSightCount[t.iTile])
            {
                alpha = 0.5f;
            }
            ImColor color;
            if (t.pCity)
            {
                color = State.pGame->playersInfo[t.iPlayer].secondaryColor;
                color.Value.w = alpha;
            }
            else if (t.iPlayer < State.players.size())
            {
                color = State.pGame->playersInfo[t.iPlayer].primaryColor;
                color.Value.w = alpha;
            }
            else if (t.terrain == 0x47c909dd) //sea
            {
                color = ImColor(18.f / 256.f, 18.f / 256.f, 34.f / 256.f, alpha);
            }
            else if (t.terrain == 0x4a707a11) //coast and lake
            {
                color = ImColor(20.f / 256.f, 35.f / 256.f, 50.f / 256.f, alpha);
            }
            else if (t.terrain == 0x29b146f4 || t.terrain == 0xe5031280 || t.terrain == 0x67ea840f) //snow
            {
                color = ImColor(80.f / 256.f, 90.f / 256.f, 90.f / 256.f, alpha);
            }
            else if (t.terrain == 0xdf49ddbd || t.terrain == 0xeb62b0f6 || t.terrain == 0x48f29eab) //tundra
            {
                color = ImColor(66.f / 256.f, 65.f / 256.f, 52.f / 256.f, alpha);
            }
            else if (t.terrain == 0x5490bef9 || t.terrain == 0xb94134b3 || t.terrain == 0xe5a80a2c) //desert
            {
                color = ImColor(100.f / 256.f, 80.f / 256.f, 45.f / 256.f, alpha);
            }
            else if (t.terrain == 0x5f83b433 || t.terrain == 0x6e9d0c70 || t.terrain == 0x83e7c630) //grass
            {
                color = ImColor(40.f / 256.f, 50.f / 256.f, 20.f / 256.f, alpha);
            }
            else //plain
            {
                color = ImColor(60.f / 256.f, 65.f / 256.f, 30.f / 256.f, alpha);
            }

            ImVec2 tilePos;
            getPos(t, zoomMinimap, OUT tilePos.x, OUT tilePos.y);

            DrawHexagon(color, zoomMinimap, pos, tilePos);
            if (t.terrain == 0x5f83b433 || t.terrain == 0xa3b9a8f0 || t.terrain == 0x5490bef9 || t.terrain == 0xdf49ddbd || t.terrain == 0x29b146f4 || t.feature == 0xa5329cb5)
            {
                DrawHexagon(ImColor(0.f / 256.f, 0.f / 256.f, 0.f / 256.f, alpha * 0.6), zoomMinimap, pos, tilePos);
            }
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->Flags = ImDrawListFlags_None;
        float factor = zoomMinimap / zoom;
        ImVec2 a = ImVec2(scroll.x* factor - pos.x, scroll.y * factor - pos.y);
        ImVec2 b = a;
        b.x += sizeMap.x * factor;
        b.y += sizeMap.y * factor;
        draw_list->AddRect(a, b, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)), 0.f, ImDrawFlags_None, 2.f);
        if (b.x > width)
        {
            b.x -= width;
            draw_list->AddRect(ImVec2(0.f, a.y), b, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)), 0.f, ImDrawFlags_None, 2.f);

        }
        draw_list->Flags = ImDrawListFlags_AntiAliasedLines;


        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 relativePos = ImVec2(mousePos.x + pos.x, mousePos.y + pos.y);
            scroll.x = (relativePos.x ) / factor - sizeMap.x / 2 * zoom;
            scroll.y = (relativePos.y ) / factor - sizeMap.y / 2 * zoom;
        }
    }

    ImGui::End();
}

float test(void* data, int index)
{
    std::vector<CivVIState>& States = *(std::vector<CivVIState>*)data;
    if (States[index].players.size() <= 0)
        return 0;

    return States[index].players[0].yields[0xbb19ce0a];
}

void Viewer::DrawEconomicStats()
{

    int loadedStateCount;
    loader.mtx.lock();
    loadedStateCount = loader.loadedStateCount;
    loader.mtx.unlock();

    ImGui::Begin("Economic stats");
    ImGui::PlotLines("test", test, (void*)&loader.States, loadedStateCount);
    ImGui::End();
}


void Viewer::DrawHardTech_(std::vector<HardTech>& techs)
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Columns(4);
    ImGui::Text("Player");
    ImGui::NextColumn();
    ImGui::Text("Turn");
    ImGui::NextColumn();
    ImGui::Text("Tech or Civic");
    ImGui::NextColumn();
    ImGui::Text("Team");
    ImGui::NextColumn();
    ImGui::Separator();

    for (int i = 0; i < techs.size(); i++)
    {
        const HardTech& h = techs[i];
        PlayerInfo& playerInfo = State.pGame->playersInfo[h.playerId];
        unsigned int leader = playerInfo.leader;
        DrawImage(leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
        ImGui::SameLine();
        ImGui::Text("%s", playerInfo.pseudo);
        ImGui::NextColumn();
        ImGui::Text("%d", h.turn);
        ImGui::NextColumn();
        ImGui::Text("%s", loader.assets[h.type].name);
        ImGui::NextColumn();
        ImGui::Text("%d", playerInfo.team);
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

void Viewer::DrawHardTech()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Hard Tech");

    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
        {
        if (ImGui::BeginTabItem("Tech"))
        {
            DrawHardTech_(State.pGame->hardTechs);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Civic"))
        {
            DrawHardTech_(State.pGame->hardDogmes);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

}

void Viewer::DrawMapStats()
{
    CivVIState& State = loader.States[iCurrentState];

    ImGui::Begin("Map stats");
    if (ImGui::CollapsingHeader("Terrain")) {
        for (std::map<unsigned int, unsigned int>::const_iterator it = State.pGame->terrainStats.begin(); it != State.pGame->terrainStats.end(); it++)
        {
            DrawImage(it->first, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::SameLine();
            if (it->first != -1)
                ImGui::Text("%s %d (%f %%)", loader.assets[it->first].name, it->second, (float)it->second / (float)State.tiles.size() * 100);
        }
    }
    if (ImGui::CollapsingHeader("Feature")) {
        for (std::map<unsigned int, unsigned int>::const_iterator it = State.pGame->featureStats.begin(); it != State.pGame->featureStats.end(); it++)
        {
            DrawImage(it->first, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::SameLine();
            if(it->first != -1)
                ImGui::Text("%s %d (%f %%)", loader.assets[it->first].name, it->second, (float)it->second / (float)State.tiles.size() * 100);
        }
    }
    if (ImGui::CollapsingHeader("Resource")) {
        for (std::map<unsigned int, unsigned int>::const_iterator it = State.pGame->resourceStats.begin(); it != State.pGame->resourceStats.end(); it++)
        {
            DrawImage(it->first, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::SameLine();
            if (it->first != -1)
                ImGui::Text("%s %d (%f %%)", loader.assets[it->first].name, it->second, (float)it->second / (float)State.tiles.size() * 100);
        }
    }
    ImGui::End();

}

void Viewer::DrawGameProgression()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Game Progression");

    std::vector<std::string> columnName{ "Leader", "scientific", "Cultural", "Chop production (with Magnus)", "District cost (Discount)", "Settler/Builder cost"};

    ImGui::Columns(columnName.size(), "mixed");

    for (int i = 0; i < columnName.size(); i++)
    {
        ImGui::Text("%s", columnName[i].c_str());
        ImGui::NextColumn();
    }


    for (int i = 0; i < State.pGame->playersByTeams.size(); i++)
    {
        for (int j = 0; j < State.pGame->playersByTeams[i].size(); j++)
        {
            int iPlayer = State.pGame->playersByTeams[i][j];

            PlayerInfo& playerInfo = State.pGame->playersInfo[iPlayer];
            Player& p = State.players[iPlayer];
            DrawImage(playerInfo.leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::NextColumn();

            float maxProgression = 0.f;
            int totalResearch[] = { 77, 61 };
            for (int k = 0; k < 2; k++)
            {
                float progression = (float)p.techDogmeInfo[k].researchCount / (float)totalResearch[k];
                ImGui::Text("%.1f %%", progression * 100);
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%d / %d", p.techDogmeInfo[k].researchCount, totalResearch[k]);

                    ImGui::EndTooltip();
                }
                ImGui::NextColumn();
                if (progression > maxProgression)
                    maxProgression = floor(progression*100)/100;
            }

            float chopCost = std::floor(10.f * (1.f + 9.f * maxProgression));
            ImGui::Text("%.0f (%.0f)", chopCost, floor(chopCost + chopCost*0.5));
            ImGui::NextColumn();

            float districtCost = std::floor(27.f * (1.f + 9.f * maxProgression));
            ImGui::Text("%.0f (%.0f)", districtCost, floor(districtCost*60/100));
            ImGui::NextColumn();

            ImGui::Text("%d / %d", 40 + 15 * p.settlerBuilt, 25 + 2 * p.builderBuilt);
            ImGui::NextColumn();
        }
    }

    ImGui::End();
}

void Viewer::DrawGoldUsage()
{
    CivVIState& State = loader.States[iCurrentState];
    int size = State.missingGolds.size();
    ImGui::Begin("Gold Usage");
    //if (size)
    //{
    //    ImGui::Columns(size);
    //    for (int i = 0; i < size; i++)
    //    {
    //        ImGui::Text("%d", State.missingGolds[i]);
    //    }
        ImGui::Columns(1);
        for (int i = 0; i < 64; i++)
        {
            PlayerInfo& playerInfo = State.pGame->playersInfo[i];
            Player& p = State.players[i];
            if (!p.cities.size() && !p.units.size())
                continue;
            else if (playerInfo.typeCiv == FULL_CIV)
            {
                ImGui::Text("%d / %d", p.unitLost, p.unitUpgraded);
                //ImGui::Text("%d / %d / %d / %d", p.unitLost, p.unitChoppedOrUpgraded, p.unitBuy, p.unitCreated);
            }
        }

    //}

    ImGui::End();

}

void Viewer::DrawDistrict()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("District");
    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (int iPlayer = 0; iPlayer < State.pGame->districtStats.size(); iPlayer++)
        {
            if (ImGui::BeginTabItem(State.pGame->playersInfo[iPlayer].pseudo))
            {
                std::map<unsigned int, unsigned int> proxies;
                std::map<unsigned int, unsigned int> districtCount;
                Player& p = State.players[iPlayer];
                for (int iDistrict = 0; iDistrict < p.districts.size(); iDistrict++)
                {
                    if(p.districts[iDistrict].totalProxi)
                        proxies[p.districts[iDistrict].yieldType] += p.districts[iDistrict].totalProxi;
                    if(p.districts[iDistrict].type != 0x62f1b509)
                        districtCount[p.districts[iDistrict].type]++;
                }
                ImGui::Separator();
                ImGui::Text("Pure yield from districts");
                bool first = false;
                for (std::map<unsigned int, unsigned int>::const_iterator it = proxies.begin(); it != proxies.end(); it++)
                {
                    if (it->second != 0)
                    {
                        if (!first)
                            first = true;
                        else
                            ImGui::SameLine();
                        DrawImage(it->first, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
                        ImGui::SameLine();
                        ImGui::Text("%d", it->second);
                    }
                }
                first = false;
                ImGui::Text("district count");
                for (std::map<unsigned int, unsigned int>::const_iterator it = districtCount.begin(); it != districtCount.end(); it++)
                {
                    if (it->second != 0)
                    {
                        if (!first)
                            first = true;
                        else
                            ImGui::SameLine();
                        DrawImage(it->first, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
                        ImGui::SameLine();
                        ImGui::Text("%d", it->second);
                    }
                }

                ImGui::Separator();

                ImGui::Columns(5);

                ImGui::Text("City");
                ImGui::NextColumn();
                ImGui::Text("District");
                ImGui::NextColumn();
                ImGui::Text("Turn Begin");
                ImGui::NextColumn();
                ImGui::Text("TurnEnd");
                ImGui::NextColumn();
                ImGui::Text("Cost");
                ImGui::NextColumn();
                ImGui::Separator();

                const std::vector<DistrictStat>& vds = State.pGame->districtStats[iPlayer];
                for (int i = 0; i < vds.size(); i++)
                {
                    const DistrictStat& ds = vds[i];

                    if (ds.turnBegin == -1)
                        continue;

                    ImGui::Text("%s", ds.name);
                    ImGui::NextColumn();

                    DrawImage(ds.type, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
                    ImGui::NextColumn();

                    ImGui::Text("%d", ds.turnBegin);
                    ImGui::NextColumn();

                    if(ds.turnEnd == -1)
                        ImGui::Text("-");
                    else
                        ImGui::Text("%d", ds.turnEnd);

                    ImGui::NextColumn();

                    ImGui::Text("%d", ds.cost);
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

}

void Viewer::DrawHuts()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Goody huts");
    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (int iPlayer = 0; iPlayer < State.pGame->nbPlayer; iPlayer++)
        {
            if (ImGui::BeginTabItem(State.pGame->playersInfo[iPlayer].pseudo))
            {
                ImGui::Columns(2);
                ImGui::Text("Turn");
                ImGui::NextColumn();
                ImGui::Text("Type of hut");
                ImGui::NextColumn();
                ImGui::Separator();

                for (int i = 0; i < State.pGame->huts.size(); i++)
                {
                    const GoodyHut& h = State.pGame->huts[i];
                    if (h.playerId != iPlayer)
                        continue;
                    ImGui::Text("%d", h.turn);
                    ImGui::NextColumn();
                    ImGui::Text("%s", loader.assets[h.type].name);
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}


void Viewer::DrawMilitaryStats()
{

}

void Viewer::DrawCityStatePanel()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("City State");
    for (int i = 0; i < 64; i++)
    {
        PlayerInfo& playerInfo = State.pGame->playersInfo[i];
        if (playerInfo.typeCiv == FULL_CIV)
            continue;
        if (playerInfo.typeCiv != CITY_STATE)
            break;
        Player& p = State.players[i];
        ImGui::TextColored(playerInfo.secondaryColor, "%s", &playerInfo.civStr[13]);
        ImGui::Indent();
        for (int k = 0; k < p.emissaries.size(); k++)
        {
            int emissaries = p.emissaries[k];
            if (emissaries)
            {
                unsigned int leader = State.pGame->playersInfo[k].leader;
                DrawImage(leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
                ImGui::SameLine();
                char buf[32];
                sprintf(buf, "%d", emissaries);
                ImGui::ProgressBar((float)emissaries / (float)p.maxEmissary, ImVec2(-1,0), buf);
            }
        }
        ImGui::Unindent();
        ImGui::Separator();

    }
    ImGui::End();

}

void Viewer::DrawGreatPeople()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Great People");

    ImGui::Columns(9, "mixed");

    unsigned int greatPeoples[] = { 0xA0426FED, 0x05B84C1A, 0x35A7F88E, 0xD51BB7B6, 0x20FE0A67, 0xFDB5F1E1, 0x9895E813, 0x0E6CA616, 0x90C1D370 }; // we avoid comandante general from colombia
    unsigned int costs[10] = { 0 };
    for (int iGreatPeople = 0; iGreatPeople < 9; iGreatPeople++)
    {
        unsigned int gp = greatPeoples[iGreatPeople];
        for (int i = 0; i < State.greatPeople.size(); i++)
        {
            if (State.greatPeople[i].type == gp)
            {
                ImGui::Dummy(ImVec2(20.f, 1.f));
                ImGui::SameLine();
                DrawImage(State.greatPeople[i].type, 0.f, 0.f, 0.5f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
                if(loader.assets.assets.find(State.greatPeople[i].name) != loader.assets.assets.end())
                    ImGui::Text("%s", loader.assets[State.greatPeople[i].name].name);
                else
                    ImGui::Text("%s", loader.assets[State.greatPeople[i].type].name);
                ImGui::NextColumn();
                costs[iGreatPeople] = State.greatPeople[i].cost > 0xffffff ? 0.f : State.greatPeople[i].cost;
                break;
            }
        }
    }

    ImGui::Separator();

    for (int iGreatPeople = 0; iGreatPeople < 9; iGreatPeople++)
    {
        unsigned int gp = greatPeoples[iGreatPeople];
        for (int i = 0; i < 64; i++)
        {
            PlayerInfo& playerInfo = State.pGame->playersInfo[i];
            const Player& p = State.players[i];
            if (!p.cities.size() && !p.units.size())
                continue;
            else if (playerInfo.typeCiv == FULL_CIV)
            {
                DrawImage(playerInfo.leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
                ImGui::SameLine();
                char buf[32];
                sprintf(buf, "%.1f/%d (+%.1f)", p.greatPeopleCurrent.at(gp), costs[iGreatPeople], p.greatPeopleProd.at(gp));
                ImGui::ProgressBar(p.greatPeopleCurrent.at(gp) / (float)costs[iGreatPeople], ImVec2(0.f, 0.f), buf);
            }
            else if (playerInfo.typeCiv == CITY_STATE)
                continue;
            else
                break;
        }

        ImGui::NextColumn();
    }
    ImGui::End();

}

void Viewer::DrawPolicies()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Policies");
    ImGui::Columns(6);
    ImGui::Text("Leader"); ImGui::NextColumn();
    ImGui::Text("Government"); ImGui::NextColumn();
    ImGui::Text("Economic"); ImGui::NextColumn();
    ImGui::Text("Military"); ImGui::NextColumn();
    ImGui::Text("Diplomatic"); ImGui::NextColumn();
    ImGui::Text("Wildcard"); ImGui::NextColumn();
    ImGui::Separator();
    ImGui::Separator();

    for (int i = 0; i < 64; i++)
    {
        PlayerInfo& playerInfo = State.pGame->playersInfo[i];
        Player& p = State.players[i];
        if (!p.cities.size() && !p.units.size())
            continue;
        if (playerInfo.typeCiv == FULL_CIV)
        {
            unsigned int leader = playerInfo.leader;
            DrawImage(leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::NextColumn();
            ImGui::Text("%s", loader.assets[p.government].name);
            ImGui::NextColumn();
            for (int j = 0; j < 4; j++)
            {
                for (int k = 0; k < p.policies[j].size(); k++)
                {
                    if (p.policies[j][k] == 0xffffffff)
                        continue;
                    ImGui::Text("%s", loader.assets[p.policies[j][k]].name);
                }
                ImGui::NextColumn();
            }
            ImGui::Separator();
        }
        else if (playerInfo.typeCiv == CITY_STATE)
            continue;
        else
            break;
    }
    ImGui::End();
}

void Viewer::DrawReligions()
{
    CivVIState& State = loader.States[iCurrentState];
    ImGui::Begin("Religions");

    if (State.religions.size())
    {
        ImGui::Columns(State.religions.size(), "mixed");
        ImGui::Separator();
        ImGui::Text("Beliefs");
        ImGui::Separator();
        for (int iReligion = 0; iReligion < State.religions.size(); iReligion++)
        {
            const Religion& r = State.religions[iReligion];
            ImGui::Indent();
            PlayerInfo& playerInfo = State.pGame->playersInfo[r.iPlayer];
            DrawImage(playerInfo.leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
            ImGui::SameLine();
            ImGui::BeginGroup();
            for (int iBelief = 0; iBelief < r.beliefs.size(); iBelief++)
            {
                ImGui::Text("%s", loader.assets[r.beliefs[iBelief]].name);
            }
            ImGui::EndGroup();
            ImGui::SameLine();
            DrawImage(r.iSymbol, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
            ImGui::Unindent();
            ImGui::NextColumn();
        }
        ImGui::Separator();
    }
    /*ImGui::Text("Buildings");
    ImGui::Separator();
    for (int iReligion = 0; iReligion < State.religions.size(); iReligion++)
    {
        const Religion& r = State.religions[iReligion];
        ImGui::Indent();
        for (int iBuilding = 0; iBuilding < r.buildings.size(); iBuilding++)
        {
            ImGui::Text("%s", loader.assets[r.buildings[iBuilding]].name);
        }
        ImGui::Unindent();
        ImGui::NextColumn();
    }
    ImGui::Separator();
    ImGui::Text("Units");
    ImGui::Separator();
    for (int iReligion = 0; iReligion < State.religions.size(); iReligion++)
    {
        const Religion& r = State.religions[iReligion];
        ImGui::Indent();
        for (int iUnit = 0; iUnit < r.units.size(); iUnit++)
        {
            ImGui::Text("%s", loader.assets[r.units[iUnit]].name);
        }
        ImGui::Unindent();
        ImGui::NextColumn();
    }
    ImGui::Separator();*/


    ImGui::Columns(1);
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Pantheon");

    ImGui::Columns(2);
    for (int i = 0; i < 64; i++)
    {
        PlayerInfo& playerInfo = State.pGame->playersInfo[i];
        const Player& p = State.players[i];
        if (!p.cities.size() && !p.units.size())
            continue;
        else if (playerInfo.typeCiv == FULL_CIV)
        {
            PlayerInfo& playerInfo = State.pGame->playersInfo[i];
            DrawImage(playerInfo.leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
            ImGui::NextColumn();
            ImGui::Text("%s", p.pantheon == -1 ? "NONE" : loader.assets[p.pantheon].name);
            ImGui::NextColumn();
        }
        else if (playerInfo.typeCiv == CITY_STATE)
            continue;
        else
            break;
    }
    ImGui::End();
}

void DrawCivicTechPanel(Viewer& v, const char* name, const std::map<unsigned int, CivicTech>& tree, TechDogmeType techType)
{
    CivVIState& State = loader.States[v.iCurrentState];
    ImGui::Begin(name, 0, ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 sizeBox = ImVec2(260.f, 80.f);
    for (std::map<unsigned int, CivicTech>::const_iterator it = tree.begin(); it != tree.end(); it++)
    {
        const CivicTech& t = it->second;
        ImVec2 pos = ImVec2(50.f + t.column * 300.f, 800.f * (t.row + 1) / 9);
        ImVec2 winPos = ImGui::GetWindowPos();
        float scroll_x = ImGui::GetScrollX();
        float scroll_y = ImGui::GetScrollY();
        ImVec2 a = ImVec2(winPos.x + pos.x - scroll_x, winPos.y + pos.y - scroll_y);
        ImVec2 b = ImVec2(a.x + sizeBox.x, a.y + sizeBox.y);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(a, b, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));

        for (int iParent = 0; iParent < t.parents.size(); iParent++)
        {

            const CivicTech& otherT = tree.at(t.parents[iParent]);
            ImVec2 otherPos = ImVec2(50.f + otherT.column * 300.f, 800.f * (otherT.row + 1) / 9);
            ImVec2 c = ImVec2(a.x, a.y + sizeBox.y/2);
            ImVec2 d = ImVec2(winPos.x + otherPos.x - scroll_x + sizeBox.x, winPos.y + otherPos.y - scroll_y + sizeBox.y / 2);
            draw_list->AddLine(c, d, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
        }

        ImGui::SetCursorPos(pos);
        v.DrawImage(t.id, 0.f, 0.f, 0.7f, ImVec4(0.f, 0.f, 1.f, 1.f));
        pos.x += 50.f;
        ImGui::SetCursorPos(pos);
        ImGui::Text("%s (%d)", loader.assets[t.id].name, t.cost);
        pos.x -= 70.f;
        pos.y += 30.f;
        for (int i = 0; i < 64; i++)
        {
            PlayerInfo& playerInfo = State.pGame->playersInfo[i];
            const Player& p = State.players[i];
            if (!p.cities.size() && !p.units.size())
                continue;
            else if (playerInfo.typeCiv == FULL_CIV)
            {
                pos.x += 30.f;
                ImGui::SetCursorPos(pos);
                float alpha = p.techDogmeInfo[techType].research.at(t.id) / (float)t.cost;
                if (p.techDogmeInfo[techType].found.at(t.id))
                    alpha = 1.0f;
                v.DrawImage(playerInfo.leader, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, alpha));
                if (p.techDogmeInfo[techType].boost.at(t.id))
                {
                    pos.y += 30.f;
                    ImGui::SetCursorPos(pos);
                    v.DrawImage(loader.assets.inspirationIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
                    pos.y -= 30.f;
                }
                if (p.techDogmeInfo[techType].current.size() && *p.techDogmeInfo[techType].current.begin() == t.id)
                {
                    ImVec2 a = ImVec2(winPos.x + pos.x - scroll_x, winPos.y + pos.y - scroll_y);
                    ImVec2 b = ImVec2(a.x + 25.f, a.y + 45.f);
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddRect(a, b, ImGui::GetColorU32(ImVec4(1.f, 0.f, 0.f, 1.f)));
                }
            }
            else if (playerInfo.typeCiv == CITY_STATE)
                continue;
            else
                break;
        }
    }
    ImGui::End();
}


void Viewer::DrawCivicPanel()
{
    DrawCivicTechPanel(*this, "Civics", loader.civicTree2, DOGME);
}

void Viewer::DrawTechPanel()
{
    DrawCivicTechPanel(*this, "Technologies", loader.techTree2, TECH);
}

typedef void (*resumeFunc)(Viewer& v, int i);
void DrawResumeInternal(Viewer& v, resumeFunc f)
{
    CivVIState& State = loader.States[v.iCurrentState];

    if (!v.bLimitResumeToVisibility || v.iVisibilityFromPlayer == -1)
        ImGui::Columns(State.pGame->nbPlayer, "mixed");
     for (int i = 0; i < State.pGame->playersByTeams.size(); i++)
    {
        for (int j = 0; j < State.pGame->playersByTeams[i].size(); j++)
        {
            int iPlayer = State.pGame->playersByTeams[i][j];
            if (v.bLimitResumeToVisibility && iPlayer != v.iVisibilityFromPlayer && v.iVisibilityFromPlayer != -1)
                continue;
            PlayerInfo& playerInfo = State.pGame->playersInfo[iPlayer];
            const Player& p = State.players[iPlayer];
            if (!p.cities.size() && !p.units.size())
                continue;
            v.DrawImage(playerInfo.leader, 0.f, 0.f, 0.3f, ImVec4(0.f, 0.f, 1.f, 1.f));


            if (ImGui::IsWindowHovered() && ImGui::IsItemClicked())
            {
                if (v.iVisibilityFromPlayer == iPlayer)
                    v.iVisibilityFromPlayer = -1;
                else
                    v.iVisibilityFromPlayer = iPlayer;
            }

            ImGui::SameLine();
            ImVec2 winPos = ImGui::GetWindowPos();
            float scroll = ImGui::GetScrollY();
            ImVec2 a = ImGui::GetCursorPos();
            a.x += winPos.x;
            a.y += winPos.y;
            a.y -= scroll;
            a.x -= 7;
            a.y -= 3;
            ImVec2 b = a;
            b.x += 25;
            b.y += 20;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(a, b, ImGui::GetColorU32(playerInfo.primaryColor));
            ImGui::TextColored(playerInfo.secondaryColor, "P%d ", iPlayer);

            f(v, iPlayer);

            ImGui::NextColumn();
        }
    }
    ImGui::Columns(1);
}

void DrawResumeGeneral(Viewer& v, int iPlayer)
{
    CivVIState& State = loader.States[v.iCurrentState];
    PlayerInfo& playerInfo = State.pGame->playersInfo[iPlayer];
    const Player& p = State.players[iPlayer];
    ImVec4 teamColored[] = { ImVec4(220.0f / 255.f, 15.0f / 255.f, 20.0f / 255.f, 1.0f),
                             ImVec4(10.0f / 255.f, 80.0f / 255.f, 225.0f / 255.f, 1.0f),
                             ImVec4(115.0f / 255.f, 160.0f / 255.f, 230.0f / 255.f, 1.0f),
                             ImVec4(255.0f / 255.f, 120.0f / 255.f, 5.0f / 255.f, 1.0f),
                             ImVec4(20.0f / 255.f, 115.0f / 255.f, 40.0f / 255.f, 1.0f),
                             ImVec4(120.0f / 255.f, 0.0f / 255.f, 220.0f / 255.f, 1.0f),
                             ImVec4(235.0f / 255.f, 115.0f / 255.f, 105.0f / 255.f, 1.0f),
                             ImVec4(0.0f / 255.f, 190.0f / 255.f, 160.0f / 255.f, 1.0f),
                             ImVec4(155.0f / 255.f, 250.0f / 255.f, 155.0f / 255.f, 1.0f),
                             ImVec4(240.0f / 255.f, 230.0f / 255.f, 0.0f / 255.f, 1.0f),
                             ImVec4(100.0f / 255.f, 185.0f / 255.f, 135.0f / 255.f, 1.0f),
                             ImVec4(255.0f / 255.f, 255.0f / 255.f, 255.0f / 255.f, 1.0f)
                           };
    ImGui::TextColored(teamColored[playerInfo.team%12], "Team %d", playerInfo.team);
    ImGui::Text("%s", playerInfo.pseudo);

    /*<Color Name = "COLOR_FLOAT_FOOD"			Color = "85,155,6,255" / >
     <Color Name = "COLOR_FLOAT_GOLD"			Color = "229,213,66,255" / >
     <Color Name = "COLOR_FLOAT_MILITARY"	Color = "188,22,22,255" / >
     <Color Name = "COLOR_FLOAT_PRODUCTION" Color = "145,51,0,255" / >
     <Color Name = "COLOR_FLOAT_SCIENCE"		Color = "95,145,255,255" / >
     <Color Name = "COLOR_FLOAT_FAITH"			Color = "255,255,255,255" / >
     <Color Name = "COLOR_FLOAT_DIPLOMATIC" Color = "140,107,64,255" / >*/

    v.DrawImage(loader.assets.strengthIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::SameLine();
    ImGui::Text("%d", p.militaryStrength);

    v.DrawImage(loader.assets.deathIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Number of unit lost from beginning of the game.");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Text("%d", p.unitLost);
    ImGui::SameLine();
    v.DrawImage(loader.assets.upgradeIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Number of unit upgraded from beginning of the game.");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Text("%d", p.unitUpgraded);

    v.DrawImage(loader.assets.populationIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::SameLine();
    ImGui::Text("%d", p.totalPop);
    ImGui::SameLine();
    v.DrawImage(loader.assets.citycountIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::SameLine();
    ImGui::Text("%d", p.cities.size());

    v.DrawImage(loader.assets.builderIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f)); //worker

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Number of worker built from beginning of the game.");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Text("%d", p.builderBuilt);
    ImGui::SameLine();
    v.DrawImage(loader.assets.tileWorkedIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Number of improvment / number of tiles available.");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Text("%d/%d", p.tileWorked, p.freeTile);

    v.DrawImage(loader.assets.tradeRouteIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::SameLine();
    ImGui::Text("%d/%d", p.unitsCount.at(0xE2BE9ECE), p.availableRoads);

    int techSize = p.techDogmeInfo[TECH].current.size();
    if (techSize)
    {
        ImGui::Text("Tech");
        unsigned int currentTech = *p.techDogmeInfo[TECH].current.begin();
        v.DrawTooltipImage(currentTech);
        if (techSize > 1)
        {
            ImGui::SameLine();
            ImGui::Text("-->");
            ImGui::SameLine();
            v.DrawTooltipImage(p.techDogmeInfo[TECH].current.back());
        }
        ImGui::Text("%d turns %s", p.techDogmeInfo[TECH].turnTo.at(currentTech), p.techDogmeInfo[TECH].boost.at(currentTech) ? "(boost)" : "");

    }

    int dogmeSize = p.techDogmeInfo[DOGME].current.size();
    if (dogmeSize)
    {
        ImGui::Text("Dogme");
        unsigned int currentDogme = *p.techDogmeInfo[DOGME].current.begin();
        v.DrawTooltipImage(currentDogme);
        if (dogmeSize > 1)
        {
            ImGui::SameLine();
            ImGui::Text("-->");
            ImGui::SameLine();
            v.DrawTooltipImage(p.techDogmeInfo[DOGME].current.back());
        }
        ImGui::Text("%d turns", p.techDogmeInfo[DOGME].turnTo.at(currentDogme), p.techDogmeInfo[DOGME].turnTo.at(currentDogme) ? "(boost)" : "");

    }

}

void DrawResumeYield(Viewer& v, int iPlayer)
{
    CivVIState& State = loader.States[v.iCurrentState];
    const Player& p = State.players[iPlayer];
    if (p.cities.size() && p.yields.size())
    {
        unsigned int yields[] = { 0x24F057EC,  0xBB19CE0A,  0x3A81F8AB,  0xEB3F55E8,  0x7F0F8E76,  0xB761634A };
        for (int iYield = 0; iYield < 6; iYield++)
        {
            unsigned int key = yields[iYield];
            float value = p.yields.at(key);
            v.DrawImage(key, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::SameLine();
            if (key == 0x7f0f8e76)
            {
                ImGui::Text("%d (+%d)", p.faith, (int)value);
            }
            else if (key == 0xb761634a)
            {
                float goldYield = value - p.maintenance;
                ImGui::Text("%d", p.gold);
                ImGui::SameLine();
                if (goldYield < 0)
                {
                    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "(%.1f)", goldYield);
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "(+%.1f)", goldYield);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "(yield) +%.1f", value);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "-%.1f (maintenance)", p.maintenance);
                    ImGui::EndTooltip();
                }
            }
            else
                ImGui::Text("%.1f", value);
        }

        v.DrawImage(loader.assets.diplomaticFavorIcon, 0.f, 0.f, 1.2f, ImVec4(0.f, 0.f, 1.f, 1.f));
        ImGui::SameLine();
        ImGui::Text("%d", p.diploFavor);

        for (int iStrat = 0; iStrat < p.strats.size(); iStrat++)
        {
            const Resource& r = p.strats[iStrat];
            if (r.current == 0 && r.prod == 0)
                continue;
            if (r.type != 0xe3e70273 && r.type != 0x64ec0edd && r.type != 0x9353a548 && r.type != 0x6f229238 && r.type != 0x6bb73300 && r.type != 0xa6573f0a && r.type != 0x13cb7793)
                continue;

            v.DrawImage(r.type, 0.f, 0.f, 0.1f, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::SameLine();
            ImGui::Text("%d / %d (+%d)", r.current, r.max, r.prod);
        }
    }
}

void DrawResumeEra(Viewer& v, int iPlayer)
{
    CivVIState& State = loader.States[v.iCurrentState];
    const Player& p = State.players[iPlayer];

    int normalGoal = p.eraPointNormal + p.eraPointModifiers;
    int heroicGoal = p.eraPointHeroic + p.eraPointModifiers;
    float progress = 0;
    char buf[32];
    if (p.eraPoint < normalGoal)
    {
        float a = 50.f / (normalGoal - p.eraPointInitEra);
        float b = -50.f * p.eraPointInitEra / (normalGoal - p.eraPointInitEra);
        progress = (a * p.eraPoint + b) / 100;
        sprintf(buf, "%d/%d", p.eraPoint, normalGoal);
    }
    else if (p.eraPoint < heroicGoal)
    {
        float a = 50.f / (heroicGoal - normalGoal);
        float b = -50.f * normalGoal / (heroicGoal - normalGoal) + 50.f;
        progress = (a * p.eraPoint + b) / 100;
        sprintf(buf, "%d/%d", p.eraPoint, heroicGoal);
    }
    else {
        progress = 1.f;
        sprintf(buf, "%d/%d", p.eraPoint, heroicGoal);
    }

    unsigned int ageIcon = p.ageState == DARK ? loader.assets.darkAgeIcon : p.ageState == NORMAL ? loader.assets.normalAgeIcon : p.ageState == GOLDEN ? loader.assets.goldenAgeIcon : loader.assets.heroicAgeIcon;
    v.DrawImage(ageIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
    ImGui::SameLine();

    if (progress < 0.5f)
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 0.f, 0.f, 1.f));
    else if (progress < 1.f)
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
    else
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.8f, 0.2f, 1.f));

    float width = ImGui::GetColumnWidth(-1);
    ImGui::ProgressBar(progress, ImVec2(width - 90.f, 20.f), buf);
    ImGui::SameLine();

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 winPos = ImGui::GetWindowPos();
    float scroll = ImGui::GetScrollY();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    cursorPos.y -= scroll;
    cursorPos.x += winPos.x;
    cursorPos.y += winPos.y;
    draw_list->AddTriangleFilled(ImVec2(cursorPos.x - 10.f, cursorPos.y + 30.f), ImVec2(cursorPos.x - 10.f, cursorPos.y - 10.f), ImVec2(cursorPos.x + 10.f, cursorPos.y + 10.f), ImGui::GetColorU32(ImVec4(0.16f, 0.29f, 0.48f, 0.54f)));

    ImGui::Dummy(ImVec2(10.f, 20.f));
    ImGui::SameLine();

    if (progress < 0.5f)
        v.DrawImage(loader.assets.darkAgeIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
    else if (progress < 1.f)
        v.DrawImage(loader.assets.normalAgeIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
    else
        v.DrawImage(loader.assets.heroicAgeIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));

    ImGui::PopStyleColor();

    for (int iCom = 0; iCom < p.commemorations.size(); iCom++)
    {
        v.DrawTooltipImage(p.commemorations[iCom], 0.5f);
    }

}
void DrawResumeTotal(Viewer& v, int iPlayer)
{
    CivVIState& State = loader.States[v.iCurrentState];
    const Player& p = State.players[iPlayer];
    if (p.cities.size())
    {
        unsigned int yields[] = { 0x24F057EC,  0xBB19CE0A,  0x3A81F8AB,  0xEB3F55E8,  0x7F0F8E76,  0xB761634A };
        for (int iYield = 0; iYield < 6; iYield++)
        {
            unsigned int key = yields[iYield];
            if (!p.totalYields.size())
            {
                v.DrawImage(key, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
                ImGui::SameLine();
                ImGui::Text("0.0");
            }
            else
            {
                float value = p.totalYields.at(key);
                v.DrawImage(key, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f));
                ImGui::SameLine();
                ImGui::Text("%.1f", value);
            }
        }
    }
}

void DrawResumeVictory(Viewer& v, int iPlayer)
{
    CivVIState& State = loader.States[v.iCurrentState];
    const Player& p = State.players[iPlayer];
    PlayerInfo& playerInfo = State.pGame->playersInfo[iPlayer];

    ImGui::Text("Diplomatic");
    ImGui::Text("%d/20", p.diploPoint);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Cultural");
    ImGui::Text("%d/%d", p.externalTourist, p.counterTourist+1);

    ImGui::Text("%d",p.internalTourist);
    ImGui::SameLine();
    v.DrawImage(loader.assets.tourismIcon, 0.f, 0.f, 1.f, ImVec4(0.f, 0.f, 1.f, 1.f), playerInfo.primaryColor);
}

void Viewer::DrawResume() {

    CivVIState& State = loader.States[iCurrentState];

    if (ImGui::Begin("Game"))
    {
        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_FittingPolicyScroll))
        {
            if (ImGui::BeginTabItem("General"))
            {
                DrawResumeInternal(*this, DrawResumeGeneral);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Yield"))
            {
                DrawResumeInternal(*this,DrawResumeYield);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Total"))
            {
                DrawResumeInternal(*this,DrawResumeTotal);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Era"))
            {
                DrawResumeInternal(*this,DrawResumeEra);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Victory"))
            {
                DrawResumeInternal(*this, DrawResumeVictory);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void Viewer::DrawInfo() {
    const CivVIState& s = loader.States[iCurrentState];

    if (iSelectedTile == -1 || !s.tiles.size())
        return;
    if (iSelectedTile >= s.tiles.size())
    {
        iSelectedTile = -1;
        return;
    }
    const Tile& t = s.tiles[iSelectedTile];
    if (ImGui::Begin("Info"))
    {
        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
        {
            //--
            if (t.pCity && ImGui::BeginTabItem("City"))
            {
                ImGui::Text("CITY");
                ImGui::Text("Name: %s (%d)", t.pCity->name.c_str(), t.pCity->id);
                ImGui::Text("PRODUCTION");
                ImGui::Indent();
                for (std::vector<unsigned int>::iterator it = t.pCity->currentProds.begin(); it != t.pCity->currentProds.end(); it++)
                {
                    ImGui::Text("%s : %d", loader.assets[*it].name, t.pCity->prod[*it ]>>8);
                }
                ImGui::Unindent();
                ImGui::Text("BUILDING");
                ImGui::Indent();
                for (std::map<unsigned int, unsigned int>::iterator it = t.pCity->built.begin(); it != t.pCity->built.end(); it++)
                {
                    if (it->second != 0xffff) //I may clean this in post parse !
                    {
                        ImGui::Text("%s", loader.assets[it->first].name);//ImGui::Text("%s : %d", loader.assets[it->first].name, it->second);
                    }
                }
                ImGui::Unindent();
                ImGui::Text("Yields");
                ImGui::Indent();
                for (std::map<unsigned int, float>::iterator it = t.pCity->yields.begin(); it != t.pCity->yields.end(); it++)
                {
                    ImGui::Text("%s %.2f | %.2f", loader.assets[it->first].name, it->second, t.pCity->yields2[it->first]);
                }
                ImGui::Unindent();

                ImGui::EndTabItem();
            }
            //--
            if (t.pDistrict && ImGui::BeginTabItem("District"))
            {
                const District& d = *t.pDistrict;
                ImGui::Text("DISTRICT %d", d.id, d.cityId);
                ImGui::Text("Type: %s (%x)", d.type == -1 ? "UNKNOWN" : loader.assets[d.type].name, d.type);
                if (d.type == 1660007689)
                {
                    ImGui::Text("Type: %s (%x)", loader.assets[t.wonder].name, t.wonder);
                }
                ImGui::Text("City : %s", s.players[t.iPlayer].pCities[d.cityId]->name.c_str());
                if (d.built && (d.type == 0x8a32007d || d.type == 0x66db35b9 || d.type == 0xb45b1b15 || d.type == 0x66fc953d || d.type == 0x79dd476d))
                {
                    ImGui::Text("Health : %d (damage %d, wall damage %d)", 200-d.damage, d.damage, d.wallDamage);
                }

                ImGui::Text("Wall %d %d", t.pDistrict->wall, t.pDistrict->wallDamage);
                if (t.pDistrict->pillage)
                    ImGui::Text("PILLAGED");
                ImGui::EndTabItem();
            }
            //--
            if (t.units.size() && ImGui::BeginTabItem("Units"))
            {
                for (int i = 0; i < t.units.size(); i++)
                {
                    ImGui::Text("Type: %s (%x)", t.units[i]->type == -1 ? "UNKNOWN" : loader.assets[t.units[i]->type].name, t.units[i]->type);
                    ImGui::Text("id : %d", t.units[i]->id);
                    ImGui::Text("Xp : %d", t.units[i]->xp);
                    ImGui::Text("Damage : %d", t.units[i]->damage);
                    ImGui::Text("Level : %d", t.units[i]->level);
                    ImGui::Text("Army : %d", t.units[i]->army);
                    ImGui::Text("Upgraded : %d", t.units[i]->fortified);
                    ImGui::Spacing();
                }
                ImGui::EndTabItem();
            }
            //--
            if (ImGui::BeginTabItem("Terrain"))
            {
                int x = (iSelectedTile % s.width);
                int y = (iSelectedTile / s.width);
                ImGui::Text("iTile: %d, x: %d (%02x), y: %d (%02x)", t.iTile, x, x, y, y);
                ImGui::Text("terrain : %s (%x)", loader.assets[t.terrain].name, t.terrain);
                if (t.feature != -1 && loader.assets.assets.find(t.feature) != loader.assets.assets.end())
                    ImGui::Text("feature : %s (%x)", loader.assets[t.feature].name, t.feature);
                if (t.resource != -1)
                    ImGui::Text("resource : %s (%d)", loader.assets[t.resource].name, t.resourceCount);
                ImGui::Text("Yield : ");

                for (std::map<unsigned int, unsigned int>::const_iterator it = t.yields.begin(); it != t.yields.end(); it++)
                {
                    if (!it->second)
                        continue;
                    ImGui::Text("%d", it->second); ImGui::SameLine();
                    DrawImage(it->first, 0.f, 0.f, 1.f, ImVec4(1.f, 1.f, 1.f, 1.f));
                    if (t.disaster.size() && t.disaster.at(it->first) != 0)
                    {
                        ImGui::SameLine();
                        ImGui::Text("(with %d from disaster)", t.disaster.at(it->first));
                    }
                }
                ImGui::Text("Appeal : %d", t.appeal);
                ImGui::Text("River : %d", t.river);
                if(t.improvment != 0xffffffff)
                    ImGui::Text("Improvment: %s (%x)", loader.assets[t.improvment].name, t.improvment);
                if (t.continent != 0xffffffff)
                    ImGui::Text("Continent : %s", loader.assets[t.continent].name);
                if(t.pillage)
                    ImGui::Text("Pillage : %d", t.pillage);
                if (t.road)
                    ImGui::Text("Road lvl %d / %d", t.road, t.roadLvl);


                if (t.iCity != 0xff && t.iPlayer < s.players.size())
                {
                    const City& otherCity = *s.players[t.iPlayer].pCities[t.iCity];
                    if(otherCity.religion != -1)
                        ImGui::Text("Religion: %s", loader.assets[otherCity.religion].name);
                }
                ImGui::EndTabItem();
            }
            //--
        }
        ImGui::EndTabBar();
        //--
    }
    ImGui::End();
}

void Viewer::DrawNavigation() {

    if (ImGui::Begin("Navigation"))
    {
        int loadedStateCount;
        int maxStateCount;

        loader.mtx.lock();
        loadedStateCount = loader.loadedStateCount;
        maxStateCount = loader.States.size() + loader.paths.size();
        loader.mtx.unlock();

        if (loadedStateCount != maxStateCount)
            ImGui::Text("Loading %d/%d", loadedStateCount, maxStateCount);
        ImGui::Text("Turn : %d", loader.States[iCurrentState].turn);
        ImGui::SliderInt("Turn", &iCurrentState, 0, loadedStateCount - 1);
        if (!bAutomatic)
        {
            if ((ImGui::Button("<", ImVec2(100.f, 20.f)) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) && iCurrentState > 0)
            {
                iCurrentState--;
            }
            ImGui::SameLine();
            if ((ImGui::Button(">", ImVec2(100.f, 20.f)) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) && iCurrentState < loadedStateCount - 1)
            {
                iCurrentState++;
            }
        }
        if (loader.States[iCurrentState].error != "")
        {
            ImGui::Text(loader.States[iCurrentState].error.c_str());
        }

        if (!bAutomatic)
        {
            if (ImGui::Button("Play")) {
                bAutomatic = true;
                t_start = std::chrono::high_resolution_clock::now();
            }
        }
        else
        {
            std::chrono::time_point<std::chrono::high_resolution_clock> t_end = std::chrono::high_resolution_clock::now();
            double elapsed_time_s = std::chrono::duration<double>(t_end - t_start).count();
            if (elapsed_time_s > timeBeforeNext && iCurrentState < loadedStateCount - 1)
            {
                iCurrentState++;
                t_start = t_end;
            }
            if (ImGui::Button("Stop"))
                bAutomatic = false;
        }
        ImGui::SameLine();
        ImGui::InputFloat("Time before next (s)", &timeBeforeNext);

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("GameInfo"))
        {
            const CivVIGame& Game = *loader.States[iCurrentState].pGame;
            ImGui::Text("%s", Game.map);
            ImGui::Text("%s", Game.mapSize);
            ImGui::Text("%s", Game.gamespeed);
            struct tm* dt;
            const time_t rawtime = (const time_t)Game.timestamp;
            dt = localtime(&rawtime);
            char buffer[30];
            strftime(buffer, sizeof(buffer), "%d/%m/%y %H:%M", dt);
            ImGui::Text("%s", buffer);
            ImGui::Text("%s", Game.multiPlayer ? "Multiplayer" : "SinglePlayer");
        }
    }
    ImGui::End();

}

void DrawArrayPacket(CivVIPacket& packet)
{
    char title[100];
    sprintf(title, "%x (%u)", packet.marker, packet.packets.size());
    //ImGui::SetNextTreeNodeOpen(true)
    if (ImGui::TreeNode(title))
    {
        if (packet.type == DATA_TYPES::ARRAY || packet.type == DATA_TYPES::OBJECT)
        {
            ImGui::Text("Marker : %x", packet.marker);
            ImGui::Text("Length : %x", packet.length);
        }
        else {
            ImGui::Text("Marker : %x", packet.marker);
            ImGui::Text("Info : %x", packet.info);
            ImGui::Text("Type : %s", types[packet.type]);
        }
        switch (packet.type) {
        case DATA_TYPES::BOOLEAN_:
            ImGui::Text("Data : %d", packet.data.bool_);
            break;
        case DATA_TYPES::POINTER:
            ImGui::Text("Data : %x", packet.data.int_);
            break;
        case DATA_TYPES::INTEGER:
            ImGui::Text("Data : %d", packet.data.int_);
            break;
        case DATA_TYPES::STRING:
            ImGui::Text("Data : %s", packet.data.pointer);
            break;
        case DATA_TYPES::ARRAY:
        case DATA_TYPES::OBJECT:
        {
            for (std::map<unsigned int, CivVIPacket>::iterator it = packet.packets.begin(); it != packet.packets.end(); it++)
            {
                ImGui::PushID(it->first);
                DrawArrayPacket(it->second);
                ImGui::PopID();
            }
            break;
        }
        case DATA_TYPES::COMPRESSED:
            ImGui::Text("Data compressed : %x", packet.data.int_);
            break;

        }
        ImGui::TreePop();
    }
}

void Viewer::DrawPackets() {
#ifdef _DEBUG
    CivVIParser& Parser = loader.ParserCpy;

    if (ImGui::Begin("Packets"))
    {
        for (int i = 0; i < Parser.packets.size(); i++)
        {
            ImGui::PushID(i);
            DrawArrayPacket(Parser.packets[i]);
            ImGui::PopID();
        }
    }
    ImGui::End();
#endif
}

Viewer::SelfTest* Viewer::selfTest = nullptr;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    if (Viewer::selfTest)
        Viewer::selfTest->glfwErrors.push_back(description);
}

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C // not in the GL 1.1 header shipped with Windows
#endif

// Saves the back buffer as PNG; distinctColors receives a rough measure of whether anything was drawn.
static bool saveScreenshot(const std::string& path, int width, int height, int& distinctColors)
{
    distinctColors = 0;
    if (width <= 0 || height <= 0)
        return false;
    std::vector<unsigned char> pixels((size_t)width * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    // The window is opaque but ImGui's blending leaves alpha < 1 in the framebuffer;
    // force it so viewers don't composite the UI over white.
    std::set<unsigned int> colors;
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
        pixels[i + 3] = 255;
        if ((i / 4) % 97 == 0) // sample
            colors.insert((unsigned)pixels[i] << 16 | (unsigned)pixels[i + 1] << 8 | pixels[i + 2]);
    }
    distinctColors = (int)colors.size();
    // OpenGL rows start at the bottom
    std::vector<unsigned char> row((size_t)width * 4);
    for (int y = 0; y < height / 2; y++)
    {
        unsigned char* top = pixels.data() + (size_t)y * width * 4;
        unsigned char* bottom = pixels.data() + (size_t)(height - 1 - y) * width * 4;
        memcpy(row.data(), top, row.size());
        memcpy(top, bottom, row.size());
        memcpy(bottom, row.data(), row.size());
    }
    return stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

void Viewer::Start(unsigned int Width, unsigned int Height) {

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        if (selfTest)
            selfTest->error = "glfwInit failed";
        return;
    }

#ifdef __APPLE__
    // macOS only exposes OpenGL 3.2+ through a forward-compatible core profile
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(Width, Height, "CivReplay", nullptr, nullptr);
    if (!window)
    {
        if (selfTest)
            selfTest->error = "glfwCreateWindow failed";
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    Assets::setIcon(window);

    // On Retina displays the framebuffer is larger than the window; rasterize the fonts at
    // that resolution and scale them back so the UI keeps its size but stays sharp.
    float fontScale = 1.0f;
    {
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        glfwGetWindowSize(window, &winW, &winH);
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (winW > 0 && fbW > winW)
            fontScale = (float)fbW / (float)winW;
    }


    // setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // imgui.ini stores the civ/BBG paths, keep it in a writable per-user location
    static const std::string iniPath = (Platform::configDir() / "imgui.ini").string();
    io.IniFilename = selfTest ? nullptr : iniPath.c_str(); // the self-test must not rewrite the user's settings

    const std::string fontPath = Platform::assetPath("font/ProggyTiny.ttf").string();
    ImFontConfig defaultConfig; // what AddFontDefault() uses when no config is given
    defaultConfig.SizePixels = 13.0f * fontScale;
    defaultConfig.OversampleH = defaultConfig.OversampleV = 1;
    defaultConfig.PixelSnapH = true;
    io.Fonts->AddFontDefault(&defaultConfig);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 6.0f * fontScale);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 8.0f * fontScale);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 10.0f * fontScale);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.0f * fontScale);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f * fontScale);
    io.FontGlobalScale = 1.0f / fontScale;

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // theme
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    bool backendOk = ImGui_ImplOpenGL3_Init(glsl_version);
    if (selfTest)
    {
        int winW = 0, winH = 0, fbW = 0, fbH = 0;
        glfwGetWindowSize(window, &winW, &winH);
        glfwGetFramebufferSize(window, &fbW, &fbH);
        auto str = [](GLenum name) { const GLubyte* s = glGetString(name); return s ? (const char*)s : "?"; };
        char info[1024];
        snprintf(info, sizeof(info), "window %dx%d, framebuffer %dx%d (font scale %.2g), OpenGL %s, renderer %s, GLSL %s, requested %s, GLFW %s",
            winW, winH, fbW, fbH, fontScale, str(GL_VERSION), str(GL_RENDERER), str(GL_SHADING_LANGUAGE_VERSION), glsl_version, glfwGetVersionString());
        selfTest->windowOk = true;
        selfTest->loaderOk = backendOk;
        selfTest->glInfo = info;
    }
    int frameCount = 0;
    int readyFrame = -1;

    initIniHandler();

    bool keep_window_open = true;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (!selfTest && glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) // keep rendering in the self-test so it always ends
        {
            //ImGui_ImplGlfw_Sleep(10);
            Platform::sleepMs(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        this->DrawGUI();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        this->WinSize = io.DisplaySize; // window size in ImGui units (differs from the framebuffer on Retina)
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (selfTest)
        {
            frameCount++;
            // Textures upload over several frames (LoadAssets); wait for them, and for a save if one was expected
            bool ready = loaded && (!selfTest->expectSaves || loader.loadedStateCount > 0);
            if (ready && readyFrame < 0)
                readyFrame = frameCount;
            if ((ready && frameCount >= readyFrame + selfTest->settleFrames) || frameCount >= selfTest->maxFrames)
            {
                selfTest->uiReady = ready;
                selfTest->capturedFrame = frameCount;
                selfTest->screenshotOk = saveScreenshot(selfTest->screenshotPath, display_w, display_h, selfTest->distinctColors);
                selfTest->glError = glGetError();
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}