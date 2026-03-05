#include "Asset.h"
#include "CivVILoader.h"
#include "Imgui/imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

unsigned int Assets::GetCRC(const char* _folder, const char* _name)
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

void Assets::InitAsset(const char* _folder, const char* _name, TABLE::INSTANCE* dbValue)
{
	unsigned int crc = Assets::GetCRC(_folder, _name);
	assets[crc] = Asset();
	strcpy(assets[crc].name, _name);
	assets[crc].prettifyName();
	strcpy(assets[crc].imageName, _name);
	strcpy(assets[crc].folderName, _folder);
	assets[crc].dbValue = dbValue;


    char selected_path[] = "./image";
	char path[512];

	sprintf(path, "%s\\%s\\%s.png", selected_path, assets[crc].folderName, assets[crc].imageName);
	if (exists(path))
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

void ExtractLayerModifier(const char* table, int tableNameLen, LAYERTYPE layerType, const char* typeName, std::vector<StaticModifier>& staticYieldModifiers)
{
	TABLE* pTableYieldChanges = GetTable(table, tableNameLen);
	if (!pTableYieldChanges)
		return;
	for (int iInstance = 0; iInstance < pTableYieldChanges->Instances.Count; iInstance++)
	{
		TABLE::INSTANCE& Instance = *pTableYieldChanges->Instances[iInstance];
		staticYieldModifiers.emplace_back();
		StaticModifier& modifier = staticYieldModifiers.back();
		modifier.layerType = layerType;
		for (int iValue = 0; iValue < Instance.Values.Count; iValue++)
		{
			const TABLE::DESCRIPTOR& Descriptor = *Instance.pTable->Descriptors[iValue];
			TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];
			if (!Value.szValue)
				continue;

			if (!strcmp(Descriptor.Name, typeName))
				modifier.type = ~Crc32(Value.szValue);
			if (!strcmp(Descriptor.Name, "YieldType"))
				modifier.yield = ~Crc32(Value.szValue);
			if (!strcmp(Descriptor.Name, "YieldChange"))
			{
				modifier.count = Value.iValue;
			}
			//bonus
			if (!strcmp(Descriptor.Name, "BonusYieldChange"))
			{
				modifier.count = Value.iValue;
			}
			if (!strcmp(Descriptor.Name, "PrereqTech") && Value.szValue)
			{
				modifier.reqTech = ~Crc32(Value.szValue);
			}
			if (!strcmp(Descriptor.Name, "PrereqCivic") && Value.szValue)
			{
				modifier.reqCivic = ~Crc32(Value.szValue);
			}

		}
	}
}

void Assets::InitTileYieldModifier()
{
	staticYieldModifiers.reserve(256);
	ExtractLayerModifier("TERRAIN_YIELDCHANGES", 20, TERRAIN, "TerrainType", staticYieldModifiers);
	ExtractLayerModifier("FEATURE_YIELDCHANGES", 20, FEATURE, "FeatureType", staticYieldModifiers);
	ExtractLayerModifier("Resource_YieldChanges", 21, RESOURCE, "ResourceType", staticYieldModifiers);
	ExtractLayerModifier("IMPROVEMENT_YIELDCHANGES", 24, IMPROVEMENT, "ImprovementType", staticYieldModifiers);

	ExtractLayerModifier("IMPROVEMENT_BONUSYIELDCHANGES", 29, BONUS_IMPROVEMENT, "ImprovementType", staticYieldModifiers);
}


void Assets::InitAll(const wchar_t* RootFolder, const wchar_t* WorkshopFolder)
{

	//LogA("Loading database %ls %ls \n", RootFolder, WorkshopFolder);
	MOD_UTILITIES::Load(RootFolder, WorkshopFolder);
	//LogA("End Loading database \n");

	TABLE* pTableKinds = GetTable("Kinds", 5);

	for (int iInstance = 0; iInstance < pTableKinds->Instances.Count; iInstance++)
	{
		for (int iChild = 0; iChild < pTableKinds->Instances[iInstance]->Childs.Count; iChild++)
		{
			TABLE::INSTANCE& Instance = *pTableKinds->Instances[iInstance]->Childs[iChild];
			for (int iValue = 0; iValue < Instance.Values.Count; iValue++)
			{
				TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];
				const TABLE::DESCRIPTOR& Descriptor = *Instance.pTable->Descriptors[iValue];
				if (Descriptor.Flags.bPrimaryKey && Descriptor.Type == TABLE::DESCRIPTOR::TEXT) {
					const char* ptr = Value.szValue;
					int size = 0;
					while (*ptr != '_') {
						size++;
						ptr++;
					}
					char folder[256] = "";
					char name[256] = "";
					strncpy(folder, Value.szValue, size);
					strncpy(name, ++ptr, strlen(Value.szValue) - size);

					Assets::InitAsset(folder, name, &Instance);
					if (!strcmp(folder, "DISTRICT"))
					{
						sprintf(folder, "%s//GREY", folder);
						Assets::InitAsset(folder, name);
					}
				}
			}
		}
	}
	//LogA("End Init assets \n");

	TABLE* pTableUpgrades = GetTable("UnitUpgrades", 12);
	for (int iInstance = 0; iInstance < pTableUpgrades->Instances.Count; iInstance++)
	{
		TABLE::INSTANCE& Instance = *pTableUpgrades->Instances[iInstance];
		unsigned int unit = 0;
		unsigned int upgrade = 0;
		for (int iValue = 0; iValue < Instance.Values.Count; iValue++)
		{
			const TABLE::DESCRIPTOR& Descriptor = *Instance.pTable->Descriptors[iValue];
			TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];

			if (!strcmp(Descriptor.Name, "Unit"))
				unit = ~Crc32(Value.szValue);
			if (!strcmp(Descriptor.Name, "UpgradeUnit"))
				upgrade = ~Crc32(Value.szValue);
		}
		unitUpgrade[unit] = upgrade;
	}

	TABLE* pTableReplaces = GetTable("UnitReplaces", 12);
	for (int iInstance = 0; iInstance < pTableReplaces->Instances.Count; iInstance++)
	{
		TABLE::INSTANCE& Instance = *pTableReplaces->Instances[iInstance];
		unsigned int unit = 0;
		unsigned int replace = 0;
		for (int iValue = 0; iValue < Instance.Values.Count; iValue++)
		{
			const TABLE::DESCRIPTOR& Descriptor = *Instance.pTable->Descriptors[iValue];
			TABLE::INSTANCE::VALUE& Value = Instance.Values[iValue];

			if (!strcmp(Descriptor.Name, "CivUniqueUnitType"))
				replace = ~Crc32(Value.szValue);
			if (!strcmp(Descriptor.Name, "ReplacesUnitType"))
				unit = ~Crc32(Value.szValue);
		}
		unitReplace[unit].push_back(replace);
	}

	InitTileYieldModifier();

	//LogA("End Loading database \n");
	Assets::InitIcons();
}