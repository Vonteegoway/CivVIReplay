#include "Asset.h"
#include "ImGui/imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <ctype.h>
#include "Crc.h"
#include "Platform.h"

GLuint init_texture(const char* imagepath, float* newWidth, float* newHeight)
{

	int width, height, nrChannels;
	unsigned char* data = stbi_load(imagepath, &width, &height, &nrChannels, 0);
	*newWidth = width;
	*newHeight = height;

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Nice trilinear filtering.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glGenerateMipmap(GL_TEXTURE_2D);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	// Read the file, call glTexImage2D with the right parameters
	//glfwLoadTexture2D(imagepath, 0);

	GLenum  err = glGetError();

	stbi_image_free(data);
	// Return the ID of the texture we just created
	return textureID;
}

bool Asset::LoadAsset(const char* filePath)
{
	if (loaded)
		return false;
	if (!bImage)
		return true;
	int channels;

	TextureID = init_texture(filePath, &x, &y);

	loaded = true;
	return loaded;
}

void Assets::prettifyName(char* name)
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

void Assets::setIcon(GLFWwindow* window)
{
#ifdef __APPLE__
	(void)window; // macOS windows have no icon of their own (the app bundle's icon is used)
#else
	// Set icon
	GLFWimage image;
	image.pixels = stbi_load(Platform::assetPath("image/icon/icon1.png").string().c_str(), &image.width, &image.height, 0, 4);

	if (image.pixels)
	{
		glfwSetWindowIcon(window, 1, &image);
		stbi_image_free(image.pixels);
	}
#endif
}

void Asset::prettifyName()
{
	Assets::prettifyName(name);
}

void Asset::Draw(float zoom, const ImVec4& tint_col /*= ImVec4(1, 1, 1, 1)*/) const
{
	ImGui::Image((ImTextureID)TextureID, ImVec2(x * zoom, y * zoom), ImVec2(0, 0), ImVec2(1, 1), tint_col);
}

bool exists(const char* name) {
	if (FILE* file = fopen(name, "r")) {
		fclose(file);
		return true;
	}
	else {
		return false;
	}
}

unsigned int Assets::GetCrc(const char* _folder, const char* _name)
{
	char fullName[512];
	char* out = fullName;

	const char* s1 = _folder;
	while (*s1) {
		*out = toupper((unsigned char)*s1);
		s1++;
		out++;
	}

	*out = '_';
	out++;
	const char* s2 = _name;
	while (*s2) {
		*out = toupper((unsigned char)*s2);
		s2++;
		out++;
	}
	*out = 0;

	return ~Crc32(fullName);
}

void Assets::InitAsset(const char* _folder, const char* _name)
{
	unsigned int crc = Assets::GetCrc(_folder, _name);
	assets[crc] = Asset();
	strcpy(assets[crc].name, _name);
	assets[crc].prettifyName();
	strcpy(assets[crc].imageName, _name);
	strcpy(assets[crc].folderName, _folder);


	if (!Platform::findImage(assets[crc].folderName, assets[crc].imageName).empty())
	{
		imageAssets.push_back(crc);
		assets[crc].bImage = true;
	}
	else
		assets[crc].bImage = false;

}

void Assets::InitIcons()
{
	Assets::InitAsset("icon", "star");
	Assets::starIcon = ~Crc32("ICON_STAR");
	Assets::InitAsset("icon", "eureka");
	Assets::eurekaIcon = ~Crc32("ICON_EUREKA");
	Assets::InitAsset("icon", "inspiration");
	Assets::inspirationIcon = ~Crc32("ICON_INSPIRATION");
	Assets::InitAsset("icon", "population");
	Assets::populationIcon = ~Crc32("ICON_POPULATION");
	Assets::InitAsset("icon", "settle");
	Assets::citycountIcon = ~Crc32("ICON_SETTLE");
	Assets::InitAsset("icon", "strength");
	Assets::strengthIcon = ~Crc32("ICON_STRENGTH");
	Assets::InitAsset("icon", "Dark_Age");
	Assets::darkAgeIcon = ~Crc32("ICON_DARK_AGE");
	Assets::InitAsset("icon", "Normal_Age");
	Assets::normalAgeIcon = ~Crc32("ICON_NORMAL_AGE");
	Assets::InitAsset("icon", "Golden_Age");
	Assets::goldenAgeIcon = ~Crc32("ICON_GOLDEN_AGE");
	Assets::InitAsset("icon", "Heroic_Age");
	Assets::heroicAgeIcon = ~Crc32("ICON_HEROIC_AGE");
	Assets::InitAsset("icon", "diplomatic_favor");
	Assets::diplomaticFavorIcon = ~Crc32("ICON_DIPLOMATIC_FAVOR");
	Assets::InitAsset("icon", "tourism");
	Assets::tourismIcon = ~Crc32("ICON_TOURISM");
	Assets::InitAsset("icon", "tradeRoute");
	Assets::tradeRouteIcon = ~Crc32("ICON_TRADEROUTE");
	Assets::InitAsset("icon", "tileWorked");
	Assets::tileWorkedIcon = ~Crc32("ICON_TILEWORKED");
	Assets::InitAsset("icon", "death");
	Assets::deathIcon = ~Crc32("ICON_DEATH");
	Assets::InitAsset("icon", "upgrade");
	Assets::upgradeIcon = ~Crc32("ICON_UPGRADE");
	Assets::InitAsset("icon", "buildericon");
	Assets::builderIcon = ~Crc32("ICON_BUILDERICON");
}

struct YieldModifierData {
	YieldModifierData(LAYERTYPE layerType, const char* typeName, std::vector<StaticModifier>& staticYieldModifiers) :layerType(layerType), typeName(typeName), staticYieldModifiers(staticYieldModifiers){}
	LAYERTYPE layerType;
	const char* typeName;
	std::vector<StaticModifier>& staticYieldModifiers;
};

int storeYieldModifier(void* data, int argc, char** argv, char** colNames)
{
	YieldModifierData& yieldModifier = *((YieldModifierData*)data);
	yieldModifier.staticYieldModifiers.emplace_back();
	StaticModifier& modifier = yieldModifier.staticYieldModifiers.back();
	modifier.layerType = yieldModifier.layerType;
	for (int i = 0; i < argc; i++) {
		if (argv[i] == NULL)
			continue;

		if (!strcmp(colNames[i], yieldModifier.typeName))
			modifier.type = ~Crc32(argv[i]);
		if (!strcmp(colNames[i], "YieldType"))
			modifier.yield = ~Crc32(argv[i]);
		if (!strcmp(colNames[i], "YieldChange"))
		{
			modifier.count = atoi(argv[i]);
		}
		//bonus
		if (!strcmp(colNames[i], "BonusYieldChange"))
		{
			modifier.count = atoi(argv[i]);
		}
		if (!strcmp(colNames[i], "PrereqTech") && argv[i])
		{
			modifier.reqTech = ~Crc32(argv[i]);
		}
		if (!strcmp(colNames[i], "PrereqCivic") && argv[i])
		{
			modifier.reqCivic = ~Crc32(argv[i]);
		}
	}
	return 0;
}

int storeNaturalWonders(void* data, int argc, char** argv, char** colNames)
{
	Assets& assets = *((Assets*)data);

	unsigned int type = 0;
	bool naturalWonder = false;
	for (int i = 0; i < argc; i++) {
		if (argv[i] == NULL)
			continue;
		if (!strcmp(colNames[i], "NaturalWonder")) {
			if (!strcmp(argv[i], "1")) {
				naturalWonder = true;
			}
			else {
				return 0;
			}
		}
		if (!strcmp(colNames[i], "FeatureType")) {
			type = ~Crc32(argv[i]);
		}
	}
	if(type)
		assets.naturalWonders.push_back(type);
	return 0;

}

int storeGovernors(void* data, int argc, char** argv, char** colNames)
{
	Assets& assets = *((Assets*)data);

	unsigned int governorType = 0;
	unsigned int turnToGo = 0;
	for (int i = 0; i < argc; i++) {
		if (argv[i] == NULL)
			continue;

		if (!strcmp(colNames[i], "GovernorType"))
			governorType = ~Crc32(argv[i]);
		if (!strcmp(colNames[i], "TransitionStrength"))
			turnToGo = (225 - atoi(argv[i])) / 25; // 150 = 3 turns, 100 = 5 turns
	}
	if (governorType && turnToGo)
	{
		assets.governorTurnToGo[governorType] = turnToGo;
	}
	return 0;
}

void Assets::InitTileYieldModifier()
{
	staticYieldModifiers.reserve(256);

	YieldModifierData yieldModifier1 = YieldModifierData(TERRAIN, "TerrainType", staticYieldModifiers);
	database.GetTable("TERRAIN_YIELDCHANGES", storeYieldModifier, &yieldModifier1);
	YieldModifierData yieldModifier2 = YieldModifierData(FEATURE, "FeatureType", staticYieldModifiers);
	database.GetTable("FEATURE_YIELDCHANGES", storeYieldModifier, &yieldModifier2);
	YieldModifierData yieldModifier3 = YieldModifierData(RESOURCE, "ResourceType", staticYieldModifiers);
	database.GetTable("Resource_YieldChanges", storeYieldModifier, &yieldModifier3);
	YieldModifierData yieldModifier4 = YieldModifierData(IMPROVEMENT, "ImprovementType", staticYieldModifiers);
	database.GetTable("IMPROVEMENT_YIELDCHANGES", storeYieldModifier, &yieldModifier4);
	YieldModifierData yieldModifier5 = YieldModifierData(BONUS_IMPROVEMENT, "ImprovementType", staticYieldModifiers);
	database.GetTable("IMPROVEMENT_BONUSYIELDCHANGES", storeYieldModifier, &yieldModifier5);
}

int storeAssets(void* data, int argc, char** argv, char** colNames)
{
	Assets& assets = *((Assets*)data);
	for (int i = 0; i < argc; i++) {
		if (!strcmp(colNames[i], "Type")) {

			const char* ptr = argv[i];
			int size = 0;
			while (*ptr != '_') {
				size++;
				ptr++;
			}
			char folder[256] = "";
			char name[256] = "";
			strncpy(folder, argv[i], size);
			strncpy(name, ++ptr, strlen(argv[i]) - size);

			assets.InitAsset(folder, name);
			if (!strcmp(folder, "DISTRICT"))
			{
				strcat(folder, "//GREY");
				assets.InitAsset(folder, name);
			}
			return 0;
		}
	}
	return 0;
}

int storeUnitsUpgrade(void* data, int argc, char** argv, char** colNames)
{
	Assets& assets = *((Assets*)data);
	unsigned int unit = 0;
	unsigned int upgrade = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(colNames[i], "Unit")) {
			unit = ~Crc32(argv[i]);
		}
		if (!strcmp(colNames[i], "UpgradeUnit"))
		{
			upgrade = ~Crc32(argv[i]);
		}
	}
	assets.unitUpgrade[unit] = upgrade;
	return 0;
}

int storeUnitsReplaces(void* data, int argc, char** argv, char** colNames)
{
	Assets& assets = *((Assets*)data);
	unsigned int replace = 0;
	unsigned int unit = 0;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(colNames[i], "CivUniqueUnitType")) {
			replace = ~Crc32(argv[i]);
		}
		if (!strcmp(colNames[i], "ReplacesUnitType"))
		{
			unit = ~Crc32(argv[i]);
		}
	}
	assets.unitReplace[unit].push_back(replace);
	return 0;
}

void Assets::InitAll(const std::filesystem::path& RootFolder, const std::filesystem::path& WorkshopFolder)
{
	database.LoadDatabase();

	database.GetTable("Types", storeAssets, this);
	database.GetTable("UnitUpgrades", storeAssets, this);
	database.GetTable("UnitReplaces", storeAssets, this);

	InitTileYieldModifier();
	database.GetTable("Governors", storeGovernors, this);
	database.GetTable("Features", storeNaturalWonders, this);


	//LogA("End Loading database \n");
	Assets::InitIcons();
}