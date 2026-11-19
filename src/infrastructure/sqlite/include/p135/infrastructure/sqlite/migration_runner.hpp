#pragma once

#include "p135/core/result.hpp"
#include "p135/infrastructure/sqlite/sqlite_database.hpp"

#include <filesystem>
#include <string>

namespace p135::infrastructure::sqlite {

struct Migration {
    int version{};
    std::string name;
    std::filesystem::path file;
};

class MigrationRunner {
public:
    explicit MigrationRunner(SqliteDatabase& database) : database_(database) {}
    [[nodiscard]] Result<void> apply(const Migration& migration);

private:
    SqliteDatabase& database_;
};

} // namespace p135::infrastructure::sqlite
