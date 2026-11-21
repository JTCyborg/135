add_library(Project135Application
    src/filter_reactor_magnetic.cpp
    src/physical_filter_reactor_magnetic.cpp
    src/reactor_design_graph.cpp
    src/reactor_design_execution.cpp
    src/filter_excitation_stage_adapter.cpp
    src/magnetic_operating_point_stage_adapter.cpp
    src/material_revision_use_cases.cpp
    src/reactor_design_inputs.cpp
    src/reactor_physical_geometry_adapter.cpp
    src/ideal_reactor_sizing.cpp
)
add_library(Project135::Application ALIAS Project135Application)

target_include_directories(Project135Application
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Application PUBLIC
    Project135::Core
    Project135::Domain
    Project135::Filter
)
target_compile_features(Project135Application PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Application PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Application PRIVATE -Wall -Wextra -Wpedantic)
endif()
