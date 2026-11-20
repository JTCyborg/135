add_library(Project135Application
    src/filter_reactor_magnetic.cpp
    src/material_revision_use_cases.cpp
)
add_library(Project135::Application ALIAS Project135Application)

target_include_directories(Project135Application
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Application PUBLIC Project135::Core)
target_compile_features(Project135Application PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Application PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Application PRIVATE -Wall -Wextra -Wpedantic)
endif()
