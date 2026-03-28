#include <Database.h>
struct CivVIDatabase
{
	CivVIDatabase();
	void LoadDatabase();
	void CivVIDatabase::GetTable(const std::string tableName, Database::RowCallback callback, void* data);
private :
	Database database;
};