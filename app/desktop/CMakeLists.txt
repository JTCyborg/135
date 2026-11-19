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
    OUTPUT_NAME "Project135"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
