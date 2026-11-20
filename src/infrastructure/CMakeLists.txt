# Prefer the vcpkg target, then the modern FindSQLite3 target.
# Keep the legacy target only as a compatibility fallback.
find_package(unofficial-sqlite3 CONFIG QUIET)
find_package(SQLite3 REQUIRED)

if(TARGET unofficial::sqlite3::sqlite3)
    set(P135_SQLITE_TARGET unofficial::sqlite3::sqlite3)
elseif(TARGET SQLite3::SQLite3)
    set(P135_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
    # Compatibility fallback for older CMake FindSQLite3 modules.
    set(P135_SQLITE_TARGET SQLite::SQLite3)
else()
    message(FATAL_ERROR "SQLite3 was found, but no supported SQLite imported target is available")
endif()
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(Project135InfrastructureSqlite
    sqlite/canonical_json.cpp
    sqlite/sqlite_database.cpp
    sqlite/migration_runner.cpp
    sqlite/sqlite_material_revision_store.cpp
    sqlite/core_material_revision_loader.cpp
    sqlite/conductor_material_revision_loader.cpp
    sqlite/revision_bound_winding_copper_loss.cpp
    sqlite/revision_bound_winding_electrothermal.cpp
    sqlite/revision_bound_physical_filter_magnetic.cpp
    sqlite/starter_material_catalog.cpp
)
add_library(Project135::InfrastructureSqlite ALIAS Project135InfrastructureSqlite)

target_include_directories(Project135InfrastructureSqlite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sqlite/include>
)
target_link_libraries(Project135InfrastructureSqlite
    PUBLIC Project135::Core Project135::Application Project135::Thermal ${P135_SQLITE_TARGET}
    PRIVATE OpenSSL::Crypto nlohmann_json::nlohmann_json
)
target_compile_features(Project135InfrastructureSqlite PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135InfrastructureSqlite PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135InfrastructureSqlite PRIVATE -Wall -Wextra -Wpedantic)
endif()
