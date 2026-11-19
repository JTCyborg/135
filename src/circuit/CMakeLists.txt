add_library(Project135Circuit
    src/solver.cpp
)
add_library(Project135::Circuit ALIAS Project135Circuit)

target_include_directories(Project135Circuit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Circuit PUBLIC Project135::Core)
target_compile_features(Project135Circuit PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Circuit PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Circuit PRIVATE -Wall -Wextra -Wpedantic)
endif()
