add_library(Project135Cad
    src/three_limb_core.cpp
    src/winding_surface_geometry.cpp
    src/winding_surface_visibility.cpp
    src/assembly_presentation.cpp
    src/drawing_export.cpp
)
add_library(Project135::Cad ALIAS Project135Cad)

target_include_directories(Project135Cad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${OpenCASCADE_INCLUDE_DIR}
        ${OpenCASCADE_INCLUDE_DIRS}
)
target_link_libraries(Project135Cad
    PUBLIC
        Project135::Core
        Project135::Domain
        ${OpenCASCADE_LIBRARIES}
)
target_compile_features(Project135Cad PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Cad PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Cad PRIVATE -Wall -Wextra -Wpedantic)
endif()
