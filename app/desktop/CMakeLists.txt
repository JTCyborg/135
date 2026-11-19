qt_add_executable(Project135
    WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
    src/material_database_widget.cpp
    src/material_database_widget.hpp
)

target_link_libraries(Project135
    PRIVATE
        Qt6::Widgets
        Project135::Core
        Project135::Circuit
        Project135::Filter
        Project135::InfrastructureSqlite
)

target_compile_features(Project135 PRIVATE cxx_std_20)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/migrations")
configure_file(
    "${PROJECT_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql"
    "${CMAKE_BINARY_DIR}/bin/migrations/001_initial_schema.sql"
    COPYONLY
)

if(MSVC)
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
endif()
