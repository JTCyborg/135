#pragma once

#include "p135/core/result.hpp"

#include <sqlite3.h>
#include <string>

namespace p135::infrastructure::sqlite {

class SqliteDatabase {
public:
    SqliteDatabase() = default;
    ~SqliteDatabase();
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&& other) noexcept;
    SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;

    [[nodiscard]] Result<void> open(const std::string& path);
    void close() noexcept;
    [[nodiscard]] Result<void> execute(const std::string& sql);
    [[nodiscard]] Result<void> beginImmediate();
    [[nodiscard]] Result<void> commit();
    [[nodiscard]] Result<void> rollback();
    [[nodiscard]] sqlite3* handle() const noexcept { return handle_; }

private:
    sqlite3* handle_{nullptr};
};

} // namespace p135::infrastructure::sqlite
