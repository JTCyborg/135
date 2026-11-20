add_library(Project135Thermal
    src/steady_state.cpp
    src/filter_bridge.cpp
    src/winding_electrothermal.cpp
    src/thermal_network.cpp
    src/reactor_thermal_network.cpp
)
add_library(Project135::Thermal ALIAS Project135Thermal)

target_include_directories(Project135Thermal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Thermal PUBLIC Project135::Core Project135::Filter Project135::Domain)
target_compile_features(Project135Thermal PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Thermal PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Thermal PRIVATE -Wall -Wextra -Wpedantic)
endif()
