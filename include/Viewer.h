#pragma once
#include "SaveLoader.h"
#include "DistrictSimulator.h"
#include "Tile.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "imgui_internal.h"
#include "ImGuiFileBrowser.h"


extern SaveLoader loader;

enum FILTER
{
	FILTER_NONE = 0,
	FILTER_APPEAL,
	FILTER_CONTINENT,
	FILTER_RELIGION
};
enum VISIBILITY
{
	V_NONE = 0,
	V_PARTIAL,
	V_TOTAL
};

struct Viewer
{
	Viewer(unsigned int Width, unsigned int Height);
	~Viewer()
	{
	}
	void Start(unsigned int Width, unsigned int Height);

	static imgui_addons::ImGuiFileBrowser file_dialog;
	static imgui_addons::ImGuiFileBrowser file_dialog_civ;
	int iCurrentState;

	bool bAutomatic;
	float timeBeforeNext;

	ImVec2 mousePos;
	ImVec2 scroll;
	ImVec2 centerTo;
	ImVec2 sizeMap;
	float zoom;
	int iTempSelectedTile;
	int iSelectedTile;
	std::chrono::time_point<std::chrono::high_resolution_clock> t_start;

	void DrawGUI();

	void initIniHandler();
	void showMainMenu();
	void DrawPackets();
	void DrawNavigation();
	void DrawInfo();
	void DrawMap();
	bool DrawImage(unsigned int type, float posX, float posY, float zoom, ImVec4 defaultColor, ImVec4 tintColor = ImVec4(1, 1, 1, 1), bool bGrey = false);
	void DrawMinimap();
	void DrawResume();

	//Stats
	void DrawEconomicStats();
	void DrawMilitaryStats();
	void DrawHuts();
	void DrawHardTech();
	void DrawHardTech_(std::vector<HardTech>& techs);
	void DrawDistrict();
	void DrawGoldUsage();
	void DrawGameProgression();
	void DrawDatabaseInfo();
	void DrawMapStats();



	//Panels
	void DrawTechPanel();
	void DrawCivicPanel();
	void DrawCityStatePanel();
	void DrawPolicies();
	void DrawReligions();
	void DrawGreatPeople();


	bool LoadAssets();
	bool loaded;

	VISIBILITY IsVisible(int iTile);
	void getPos(const Tile& t, float zoom, OUT float& posX, OUT float& posY);
	void DrawRiver(const Tile& t, float posX, float posY);
	void DrawHexagon(const ImColor& color, float zoom, const ImVec2& scroll, const ImVec2& pos);
	void DrawBorders(const Tile& t, float posX, float posY);
	void DrawTooltipImage(unsigned int type, float zoom = 1.0f);

	bool bDrawMinimap;
	bool bDrawResume;

	bool bDrawTech;
	bool bDrawCivic;
	bool bDrawCityState;
	bool bDrawReligions;
	bool bDrawPolicies;
	bool bDrawGreatPeople;

	bool bDrawHuts;
	bool bDrawHardTech;
	bool bDrawDistrict;
	bool bDrawMapStats;
	bool bDrawGoldUsage;
	bool bDrawGameProgression;
	bool bShowYieldOnTile;

	int  filter;
	std::map<unsigned int, ImVec4> continentsColor;
	std::map<unsigned int, ImVec4> religionsColor;

	int minimapSize;
	bool fogOfWarMinimap;

	bool bDrawEconomicStats;
	bool bDrawMilitaryStats;

	ImVec2 WinSize;

	int iVisibilityFromPlayer;
	bool bLimitResumeToVisibility;

	DistrictSimulator dSimulator;

	void* hsicon;

	bool searchFocus;

	ImGuiTextFilter Search;
};