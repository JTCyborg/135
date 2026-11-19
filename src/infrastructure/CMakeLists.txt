find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(Project135InfrastructureSqlite
    sqlite/sqlite_database.cpp
    sqlite/migration_runner.cpp
    sqlite/sqlite_material_revision_store.cpp
)
add_library(Project135::InfrastructureSqlite ALIAS Project135InfrastructureSqlite)

target_include_directories(Project135InfrastructureSqlite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sqlite/include>
)
target_link_libraries(Project135InfrastructureSqlite
    PUBLIC Project135::Core unofficial::sqlite3::sqlite3
    PRIVATE OpenSSL::Crypto
)
target_compile_features(Project135InfrastructureSqlite PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135InfrastructureSqlite PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135InfrastructureSqlite PRIVATE -Wall -Wextra -Wpedantic)
endif()
