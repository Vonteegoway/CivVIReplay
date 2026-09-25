#include <CivVIDatabase.h>
#include <sstream>
#include <filesystem>
#include "Platform.h"

CivVIDatabase::CivVIDatabase() : database(":memory:")
{
}


void CivVIDatabase::LoadDatabase()
{
	path = Platform::civCacheDbPath();
	if (path.empty() || !database.openFile(path))
	{
		path = Platform::assetPath("image") / "database" / "backup.sqlite";
		database.openFile(path);
	}
}

void CivVIDatabase::GetTable(const std::string tableName, Database::RowCallback callback, void* data)
{
	database.query("SELECT * FROM " + tableName, callback, data);
}