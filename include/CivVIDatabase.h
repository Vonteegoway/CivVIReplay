#pragma once
#include <Database.h>
struct CivVIDatabase
{
	CivVIDatabase();
	void LoadDatabase();
	void GetTable(const std::string tableName, Database::RowCallback callback, void* data);
	std::filesystem::path path;
	std::string warning; // the game's cache database was found but unusable (we fell back to the bundled copy)
	std::string error;   // no usable database at all; GetTable will throw
private :
	Database database;
};
