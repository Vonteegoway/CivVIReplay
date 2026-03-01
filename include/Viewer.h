#pragma once
#include "SaveLoader.h"
#include "DistrictSimulator.h"
#include "Tile.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <Windows.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "Array.h"
#include "imgui_internal.h"
#include "ImGuiFileBrowser.h"


#include "CivVILoader.h"


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

struct INSTANCE_TO_CREATE {
	enum UPDATE_TYPE {
		INSTANCE_TO_INSERT,
		INSTANCE_TO_UPDATE,
		INSTANCE_TO_DELETE,
	};

	static const char* UpdateTypeToStr(const UPDATE_TYPE& UpdateType) {
		switch (UpdateType) {
		case INSTANCE_TO_INSERT: return "Insert";
		case INSTANCE_TO_UPDATE: return "Update";
		case INSTANCE_TO_DELETE: return "Delete";
		default:
			VkBreak();
			break;
		}
		return NULL;
	}

	TABLE* pTable;
	UPDATE_TYPE Type;
	const TABLE::INSTANCE* pOriginalInstance;

	struct VALUE {
		char StringValue[256];
		bool bIsSet;
		bool bIsWhere;
		bool bIsToBeCreatedForeignKey;
		unsigned int ForeignKeyIndex;
		TABLE::INSTANCE::VALUE _;
		//--
		void SetValue(const TABLE::DESCRIPTOR& Descriptor, const TABLE::INSTANCE::VALUE& InstanceValue, bool bFromDefault = false) {
			bIsSet = true;
			//--
			switch (Descriptor.Type) {
			case TABLE::DESCRIPTOR::TEXT:
			case TABLE::DESCRIPTOR::FOREIGN_KEY: {
				_.szValue = StringValue;
				if (bFromDefault) {
					strcpy(_.szValue, InstanceValue.szValue);
				}
				else {
					if (InstanceValue.szValue)
						strcpy(_.szValue, InstanceValue.szValue);
				}
				break;
			}
			case TABLE::DESCRIPTOR::BOOLEAN: {
				if (bFromDefault) {
					_.bValue = atoi(InstanceValue.szValue);
				}
				else {
					_.bValue = InstanceValue.bValue;
				}
				break;
			}
			case TABLE::DESCRIPTOR::INTEGER: {
				if (bFromDefault) {
					_.iValue = atoi(InstanceValue.szValue);
				}
				else {
					_.iValue = InstanceValue.iValue;
				}
				break;
			}
			case TABLE::DESCRIPTOR::REAL: {
				break;
			}
			}
		}
	};
	//--
	VALUE Values[64];
};


struct Viewer
{
	Viewer(HINSTANCE hInstance, unsigned int Width, unsigned int Height);
	~Viewer()
	{
	}
	void Start(HINSTANCE hInstance, unsigned int Width, unsigned int Height);

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

	void DrawInstance(const TABLE::INSTANCE& Instance, int Depth = 0, const TABLE::INSTANCE* pParentInstance = NULL);
	void DrawTable(TABLE& Table);
	void DrawEditor();
	void GenerateSQL();
	char* RetrieveForeignValue(INSTANCE_TO_CREATE::VALUE& Value);


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

	bool bDrawDatabaseInfo;
	bool bDrawEditorDatabase;
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

	TABLE* pTableKinds;
	TABLE* pTableTypes;
	DATA_ARRAY<TABLE*>* pTables;
	ImGuiTextFilter conceptFilter;

	HANDLE hsicon;

	bool searchFocus;

	DATA_ARRAY<INSTANCE_TO_CREATE> ToCreateInstances;
	DATA_ARRAY<const TABLE::INSTANCE*> OpenedInstances;
	ImGuiTextFilter Search;
};