add_library(Project135Domain
    src/design_dependency.cpp
    src/design_parameter.cpp
    src/mechanical_geometry.cpp
    src/magnetic_product.cpp
    src/magnetic_circuit.cpp
    src/core_loss.cpp
    src/three_limb_core_loss.cpp
    src/initial_magnetization.cpp
    src/three_limb_magnetic.cpp
    src/three_limb_geometry_adapter.cpp
    src/material_catalog.cpp
    src/winding_electrical.cpp
    src/component_catalog.cpp
)
add_library(Project135::Domain ALIAS Project135Domain)

target_include_directories(Project135Domain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(Project135Domain PUBLIC Project135::Core)
target_compile_features(Project135Domain PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Project135Domain PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Domain PRIVATE -Wall -Wextra -Wpedantic)
endif()
