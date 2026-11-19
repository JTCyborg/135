add_library(Project135Filter
    src/synthesis.cpp
    src/network.cpp
    src/analysis.cpp
)
add_library(Project135::Filter ALIAS Project135Filter)

target_include_directories(Project135Filter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Filter PUBLIC Project135::Core Project135::Circuit)
target_compile_features(Project135Filter PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Filter PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Filter PRIVATE -Wall -Wextra -Wpedantic)
endif()
