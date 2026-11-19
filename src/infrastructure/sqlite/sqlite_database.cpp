#include "p135/infrastructure/sqlite/sqlite_database.hpp"

#include <utility>

namespace p135::infrastructure::sqlite {
namespace {
Error ioError(const std::string& message)
{
    Error error;
    error.code = ErrorCode::IoFailure;
    error.message = message;
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "SQLITE_IO_ERROR", message, std::nullopt, std::nullopt, std::nullopt
    });
    return error;
}
}

SqliteDatabase::~SqliteDatabase() { close(); }

SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {}

SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept
{
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

Result<void> SqliteDatabase::open(const std::string& path)
{
    close();
    sqlite3* candidate = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &candidate,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = candidate ? sqlite3_errmsg(candidate) : sqlite3_errstr(rc);
        if (candidate) sqlite3_close(candidate);
        return Result<void>::failure(ioError("Cannot open SQLite database: " + message));
    }
    handle_ = candidate;
    sqlite3_busy_timeout(handle_, 5000);
    auto fk = execute("PRAGMA foreign_keys = ON;");
    if (!fk) {
        close();
        return fk;
    }
    return Result<void>::success();
}

void SqliteDatabase::close() noexcept
{
    if (handle_) {
        sqlite3_close_v2(handle_);
        handle_ = nullptr;
    }
}

Result<void> SqliteDatabase::execute(const std::string& sql)
{
    if (!handle_)
        return Result<void>::failure(ioError("SQLite database is not open."));
    char* errorText = nullptr;
    const int rc = sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &errorText);
    if (rc != SQLITE_OK) {
        const std::string message = errorText ? errorText : sqlite3_errmsg(handle_);
        sqlite3_free(errorText);
        return Result<void>::failure(ioError("SQLite statement failed: " + message));
    }
    return Result<void>::success();
}

Result<void> SqliteDatabase::beginImmediate() { return execute("BEGIN IMMEDIATE;"); }
Result<void> SqliteDatabase::commit() { return execute("COMMIT;"); }
Result<void> SqliteDatabase::rollback() { return execute("ROLLBACK;"); }

} // namespace p135::infrastructure::sqlite
