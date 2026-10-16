#include <CivVIDatabase.h>
#include <sstream>
#include <filesystem>
#include <cstring>
#include "Platform.h"

CivVIDatabase::CivVIDatabase() : database(":memory:")
{
}

static int countRow(void* data, int, char**, char**)
{
	++*static_cast<int*>(data);
	return 0;
}

// Every table Assets::InitAll reads. Keep in sync with the GetTable calls in Asset.cpp.
static const char* const requiredTables[] = {
	"Types", "UnitUpgrades", "UnitReplaces", "Governors", "Features",
	"TERRAIN_YIELDCHANGES", "FEATURE_YIELDCHANGES", "Resource_YieldChanges",
	"IMPROVEMENT_YIELDCHANGES", "IMPROVEMENT_BONUSYIELDCHANGES",
};

// sqlite3_open only looks at the file on the first query: an empty, truncated, foreign or
// older-schema file opens fine and fails later. Query every table InitAll needs up front, so
// any mismatch falls back to the bundled copy instead of failing halfway through loading.
static bool usable(Database& database, const std::filesystem::path& path, std::string& why)
{
	if (!database.openFile(path))
	{
		why = "cannot open " + path.u8string();
		return false;
	}
	try
	{
		for (const char* table : requiredTables)
		{
			int rows = 0;
			database.query(std::string("SELECT * FROM ") + table + " LIMIT 1", countRow, &rows);
			if (rows == 0 && !strcmp(table, "Types"))
			{
				why = path.u8string() + " has no gameplay data (empty Types table)";
				return false;
			}
		}
	}
	catch (const std::exception& e)
	{
		why = path.u8string() + ": " + e.what();
		return false;
	}
	return true;
}

void CivVIDatabase::LoadDatabase()
{
	warning.clear();
	error.clear();

	std::string why;
	path = Platform::civCacheDbPath();
	if (!path.empty() && usable(database, path, why))
		return;
	if (!why.empty())
		warning = "game database unusable, using the bundled copy (" + why + ")";

	path = Platform::assetPath("image") / "database" / "backup.sqlite";
	if (usable(database, path, why))
		return;
	error = "no usable gameplay database: " + why;
	path.clear();
}

void CivVIDatabase::GetTable(const std::string tableName, Database::RowCallback callback, void* data)
{
	if (!error.empty())
		throw std::runtime_error(error);
	database.query("SELECT * FROM " + tableName, callback, data);
}
