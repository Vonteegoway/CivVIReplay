#include "Database.h"
#include <fstream>
#include <sstream>

// --- Constructor ---
Database::Database(const std::string& filename) {
    int rc = sqlite3_open(filename.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Cannot open DB: " + err);
    }
}

// --- Destructor ---
Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

// --- Move semantics ---
Database::Database(Database&& other) noexcept {
    db_ = other.db_;
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close(db_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}


// --- Execute SQL file ---
bool Database::openFile(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path.u8string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    if (db_)
        sqlite3_close(db_);
    db_ = db;
    return true;
}

// --- Query with callback ---
void Database::query(const std::string& sql, RowCallback callback, void* data) {
    char* errMsg = nullptr;

    int rc = sqlite3_exec(db_, sql.c_str(), callback, data, &errMsg);

    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Query error: " + err);
    }
}

// --- File reader ---
std::string Database::readFile(const std::filesystem::path& path) {

    std::ifstream file(path);
    if (!file) {
        char buffer[256];
		snprintf(buffer, sizeof(buffer), "Cannot open file: %s", path.u8string().c_str());
        throw std::runtime_error(buffer);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}