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
void Database::openFile(const std::wstring& path) {
    int rc = sqlite3_open16(path.c_str(), &db_);
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
std::string Database::readFile(const std::wstring& path) {

    std::ifstream file(path);
    if (!file) {
        char buffer[256];
		sprintf(buffer, "Cannot open file: %ls", path.c_str());
        throw std::runtime_error(buffer);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}