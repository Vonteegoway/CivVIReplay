#pragma once
//#include "stb_image.h"
#include "imgui.h"
#include <map>
#include <vector>
#include <filesystem>
#include "CivVIDatabase.h"
#include <GLFW/glfw3.h>

enum LAYERTYPE
{
	TERRAIN,
	FEATURE,
	RESOURCE,
	IMPROVEMENT,
	BONUS_IMPROVEMENT,
};

struct StaticModifier
{
	StaticModifier():reqTech(0),reqCivic(0), yield(0), count(0){}
	LAYERTYPE layerType;
	unsigned int type;
	unsigned int yield;
	unsigned int count;

	//Bonus improvment
	unsigned int reqTech;
	unsigned int reqCivic;
};

struct Asset
{
	float x;
	float y;
	unsigned int Index;
	GLuint TextureID;
	bool loaded;
	bool bImage;

	char name[256];
	char imageName[256];
	char folderName[256];

	Asset()
	{
		loaded = false;
	}
	bool LoadAsset(const char* filePath);

	void Draw(float zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1)) const;
	void prettifyName();
};

struct Assets
{
	CivVIDatabase database;

	std::vector<unsigned int> imageAssets;
	std::map<unsigned int, Asset> assets;

	// From DB
	std::map<unsigned int, unsigned int > unitUpgrade;
	std::map<unsigned int, std::vector<unsigned int> > unitReplace;
	std::vector<StaticModifier> staticYieldModifiers;
	std::vector<unsigned int> naturalWonders;
	std::map<unsigned int, unsigned int > governorTurnToGo;
	void InitTileYieldModifier();


	static void setIcon(GLFWwindow* window);
	static void prettifyName(char* name);
	static unsigned int GetCrc(const char* _folder, const char* _name);

	void InitAsset(const char* _prename, const char* _name);

	void InitAll(const std::filesystem::path& RootFolder, const std::filesystem::path& WorkshopFolder);
	void InitIcons();
	const Asset& operator[](unsigned int id) const { return assets.at(id); }

	unsigned int starIcon;
	unsigned int eurekaIcon;
	unsigned int inspirationIcon;
	unsigned int populationIcon;
	unsigned int strengthIcon;
	unsigned int citycountIcon;
	unsigned int darkAgeIcon;
	unsigned int normalAgeIcon;
	unsigned int goldenAgeIcon;
	unsigned int heroicAgeIcon;
	unsigned int diplomaticFavorIcon;
	unsigned int tourismIcon;
	unsigned int tradeRouteIcon;
	unsigned int tileWorkedIcon;
	unsigned int deathIcon;
	unsigned int upgradeIcon;
	unsigned int builderIcon;
};