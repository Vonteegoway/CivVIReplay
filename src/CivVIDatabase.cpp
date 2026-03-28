#include <CivVIDatabase.h>
#include <sstream>
#include <filesystem>

CivVIDatabase::CivVIDatabase() : database(":memory:")
{
}


void CivVIDatabase::LoadDatabase()
{
	std::filesystem::path appdata = std::getenv("LOCALAPPDATA");
	std::filesystem::path filePath = appdata / "Firaxis Games" / "Sid Meier's Civilization VI" / "Cache" / "DebugGameplay.sqlite";
	if (!std::filesystem::exists(filePath))
	{
		filePath = std::filesystem::path("./image") / "database" / "backup.sqlite";
	}

	database.openFile(filePath);
}

void CivVIDatabase::GetTable(const std::string tableName, Database::RowCallback callback, void* data)
{
	database.query("SELECT * FROM " + tableName, callback, data);
}