add_library(Project135DesktopUi STATIC
    src/main_window.cpp
    src/main_window.hpp
    src/cad_viewport_widget.cpp
    src/cad_viewport_widget.hpp
    src/material_database_widget.cpp
    src/material_database_widget.hpp
)
target_link_libraries(Project135DesktopUi
    PUBLIC
        Qt6::Widgets
        Project135::Core
        Project135::Circuit
        Project135::Filter
        Project135::Domain
        Project135::Cad
        Project135::Thermal
        Project135::InfrastructureSqlite
)
target_compile_features(Project135DesktopUi PUBLIC cxx_std_20)
target_include_directories(Project135DesktopUi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(Project135DesktopUi PROPERTIES AUTOMOC ON)

qt_add_executable(Project135 WIN32 src/main.cpp)
target_link_libraries(Project135 PRIVATE Project135DesktopUi)
target_compile_features(Project135 PRIVATE cxx_std_20)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/migrations")
configure_file(
    "${PROJECT_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql"
    "${CMAKE_BINARY_DIR}/bin/migrations/001_initial_schema.sql"
    COPYONLY
)

configure_file(
    "${PROJECT_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql"
    "${CMAKE_BINARY_DIR}/bin/migrations/002_calculation_snapshots.sql"
    COPYONLY
)

if(MSVC)
    target_compile_options(Project135DesktopUi PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_options(Project135 PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()

set_target_properties(Project135 PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME "Project135"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

# Copy Qt runtime DLLs and the Windows platform plugin beside the executable.
# Qt6_DIR normally points to <Qt>/lib/cmake/Qt6; windeployqt is in <Qt>/bin.
if(WIN32)
    get_filename_component(P135_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(P135_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${P135_QT_BIN_DIR}"
    )
    if(NOT P135_WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found. Project135 cannot be run as a standalone build without Qt runtime deployment.")
    endif()

    add_custom_command(TARGET Project135 POST_BUILD
        COMMAND "${P135_WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            "$<TARGET_FILE:Project135>"
        COMMENT "Deploying Qt runtime for Project135"
        VERBATIM
    )

    # SQLite and OpenSSL are non-Qt runtime dependencies of the material database.
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        set(P135_VCPKG_TRIPLET_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
        add_custom_command(TARGET Project135 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${P135_VCPKG_TRIPLET_ROOT}/debug/bin/sqlite3.dll,${P135_VCPKG_TRIPLET_ROOT}/bin/sqlite3.dll>"
                "$<TARGET_FILE_DIR:Project135>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${P135_VCPKG_TRIPLET_ROOT}/debug/bin/libcrypto-3-x64.dll,${P135_VCPKG_TRIPLET_ROOT}/bin/libcrypto-3-x64.dll>"
                "$<TARGET_FILE_DIR:Project135>"
            COMMENT "Deploying SQLite and OpenSSL runtime for Project135"
            VERBATIM
        )
    endif()
endif()


if(P135_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(Project135DesktopWorkflowE2ETests
        tests/workflow_e2e_tests.cpp
    )
    target_link_libraries(Project135DesktopWorkflowE2ETests
        PRIVATE
            Project135DesktopUi
            Qt6::Test
    )
    target_compile_features(Project135DesktopWorkflowE2ETests PRIVATE cxx_std_20)
    set_target_properties(Project135DesktopWorkflowE2ETests PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/tests"
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests/migrations")
    configure_file(
        "${PROJECT_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql"
        "${CMAKE_BINARY_DIR}/bin/tests/migrations/001_initial_schema.sql"
        COPYONLY
    )
    configure_file(
        "${PROJECT_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql"
        "${CMAKE_BINARY_DIR}/bin/tests/migrations/002_calculation_snapshots.sql"
        COPYONLY
    )
    add_test(NAME Project135DesktopWorkflowE2ETests
        COMMAND Project135DesktopWorkflowE2ETests)
    set_tests_properties(Project135DesktopWorkflowE2ETests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
