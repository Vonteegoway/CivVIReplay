#pragma once
#include <Database.h>
struct CivVIDatabase
{
	CivVIDatabase();
	void LoadDatabase();
	void GetTable(const std::string tableName, Database::RowCallback callback, void* data);
	std::filesystem::path path;
private :
	Database database;
};
