#pragma once
#include <sqlite3.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <climits>
#include <unistd.h>

class Database {
private:
    sqlite3* db_ = nullptr;

    // Resolves a relative DB path against the directory containing the running
    // executable, so the DB file is found regardless of the process's CWD
    // (e.g. launched from build/ vs. from the source dir).
    static std::string resolvePath(const std::string& path) {
        if (!path.empty() && path.front() == '/') {
            return path; // already absolute
        }

        char exePath[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len <= 0) {
            return path; // fall back to CWD-relative if we can't resolve
        }
        exePath[len] = '\0';

        std::string exeDir(exePath);
        size_t slash = exeDir.find_last_of('/');
        if (slash == std::string::npos) {
            return path;
        }
        exeDir.resize(slash + 1);
        return exeDir + path;
    }

public:
    Database() = default;

    ~Database() {
        if (db_) sqlite3_close(db_);
    }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& path) {
        const std::string resolved = resolvePath(path);
        if (sqlite3_open(resolved.c_str(), &db_) != SQLITE_OK) {
            std::fprintf(stderr, "Can't open DB %s: %s\n",
                         resolved.c_str(), sqlite3_errmsg(db_));
            return false;
        }
        std::fprintf(stderr, "Opened DB at %s\n", resolved.c_str());
        sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
        return true;
    }

    sqlite3* handle() const { return db_; }

    // Chạy SQL không trả kết quả (CREATE, INSERT, UPDATE, DELETE)
    bool exec(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::fprintf(stderr, "SQL error: %s\n", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    // Trả true nếu tìm thấy user, đồng thời set outPassword.
    bool getUserPassword(const std::string& name, std::string& outPassword)
    {
        const char* selectSql = "SELECT password FROM users WHERE name = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db_));
            return false;
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = true;
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            outPassword = text ? reinterpret_cast<const char*>(text) : "";
        }

        sqlite3_finalize(stmt);
        return found;
    }

    // Thêm user mới. Trả false nếu name đã tồn tại (UNIQUE constraint) hoặc lỗi khác.
    bool addUser(const std::string& name, const std::string& password)
    {
        const char* insertSql = "INSERT INTO users (name, password) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db_));
            return false;
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            std::fprintf(stderr, "insert failed: %s\n", sqlite3_errmsg(db_));
        }

        sqlite3_finalize(stmt);
        return ok;
    }

};