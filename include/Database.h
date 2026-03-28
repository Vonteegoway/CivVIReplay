#pragma once

#include <sqlite3.h>
#include <string>
#include <functional>
#include <stdexcept>

struct Database {
public:
    using RowCallback = int(void*, int, char**, char**);

    explicit Database(const std::string& filename = ":memory:");
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Core API
    void Database::openFile(const std::wstring& path);

    void query(const std::string& sql, RowCallback callback, void* data);

    sqlite3* get() { return db_; }

private:
    sqlite3* db_ = nullptr;

    static std::string readFile(const std::wstring& path);
    void check(int rc);
};