#include <CivVIDatabase.h>
#include <sstream>
#include <filesystem>
#include "Platform.h"

CivVIDatabase::CivVIDatabase() : database(":memory:")
{
}

static int countRow(void* data, int, char**, char**)
{
	++*static_cast<int*>(data);
	return 0;
}

// sqlite3_open only looks at the file on the first query: an empty, truncated or foreign file
// opens fine and fails later. Probe the table everything else starts from.
static bool usable(Database& database, const std::filesystem::path& path, std::string& why)
{
	if (!database.openFile(path))
	{
		why = "cannot open " + path.u8string();
		return false;
	}
	try
	{
		int rows = 0;
		database.query("SELECT * FROM Types LIMIT 1", countRow, &rows);
		if (rows == 0)
		{
			why = path.u8string() + " has no gameplay data (empty Types table)";
			return false;
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
