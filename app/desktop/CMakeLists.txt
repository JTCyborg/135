qt_add_executable(Project135
    WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
)

target_link_libraries(Project135
    PRIVATE
        Qt6::Widgets
        Project135::Core
        Project135::Circuit
)

target_compile_features(Project135 PRIVATE cxx_std_20)

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
# Without this deployment step a freshly built GUI may silently fail to start
# when launched outside a Qt-enabled developer command prompt.
if(TARGET Qt6::windeployqt)
    add_custom_command(TARGET Project135 POST_BUILD
        COMMAND Qt6::windeployqt
            --no-translations
            --no-compiler-runtime
            "$<TARGET_FILE:Project135>"
        COMMENT "Deploying Qt runtime for Project135"
        VERBATIM
    )
else()
    message(WARNING "Qt6::windeployqt target is unavailable; Project135 runtime deployment must be performed manually.")
endif()
