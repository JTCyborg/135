find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(Project135InfrastructureSqlite
    sqlite/canonical_json.cpp
    sqlite/sqlite_database.cpp
    sqlite/migration_runner.cpp
    sqlite/sqlite_material_revision_store.cpp
    sqlite/starter_material_catalog.cpp
)
add_library(Project135::InfrastructureSqlite ALIAS Project135InfrastructureSqlite)

target_include_directories(Project135InfrastructureSqlite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sqlite/include>
)
target_link_libraries(Project135InfrastructureSqlite
    PUBLIC Project135::Core Project135::Application SQLite3::SQLite3
    PRIVATE OpenSSL::Crypto nlohmann_json::nlohmann_json
)
target_compile_features(Project135InfrastructureSqlite PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135InfrastructureSqlite PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135InfrastructureSqlite PRIVATE -Wall -Wextra -Wpedantic)
endif()
