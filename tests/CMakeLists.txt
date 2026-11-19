add_executable(Project135CoreTests
    core/core_smoke_tests.cpp
)
target_link_libraries(Project135CoreTests PRIVATE Project135::Core)
add_test(NAME Project135CoreTests COMMAND Project135CoreTests)

add_executable(Project135CircuitTests
    circuit/circuit_tests.cpp
)
target_link_libraries(Project135CircuitTests PRIVATE Project135::Circuit)
add_test(NAME Project135CircuitTests COMMAND Project135CircuitTests)

# Keep assert-based test checks active in Release configurations as well.
if(MSVC)
    target_compile_options(Project135CoreTests PRIVATE /UNDEBUG)
    target_compile_options(Project135CircuitTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135CoreTests PRIVATE -UNDEBUG)
    target_compile_options(Project135CircuitTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135FilterTests
    filter/filter_tests.cpp
)
target_link_libraries(Project135FilterTests PRIVATE Project135::Filter)
add_test(NAME Project135FilterTests COMMAND Project135FilterTests)

if(MSVC)
    target_compile_options(Project135FilterTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135FilterTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135FilterNetworkTests
    filter/network_tests.cpp
)
target_link_libraries(Project135FilterNetworkTests PRIVATE Project135::Filter)
add_test(NAME Project135FilterNetworkTests COMMAND Project135FilterNetworkTests)
if(MSVC)
    target_compile_options(Project135FilterNetworkTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135FilterNetworkTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135FilterAnalysisTests
    filter/analysis_tests.cpp
)
target_link_libraries(Project135FilterAnalysisTests PRIVATE Project135::Filter)
add_test(NAME Project135FilterAnalysisTests COMMAND Project135FilterAnalysisTests)
if(MSVC)
    target_compile_options(Project135FilterAnalysisTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135FilterAnalysisTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135FilterHarmonicsTests
    filter/harmonics_tests.cpp
)
target_link_libraries(Project135FilterHarmonicsTests PRIVATE Project135::Filter)
add_test(NAME Project135FilterHarmonicsTests COMMAND Project135FilterHarmonicsTests)
if(MSVC)
    target_compile_options(Project135FilterHarmonicsTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135FilterHarmonicsTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ThermalSteadyStateTests
    thermal/steady_state_tests.cpp
)
target_link_libraries(Project135ThermalSteadyStateTests PRIVATE Project135::Thermal)
add_test(NAME Project135ThermalSteadyStateTests COMMAND Project135ThermalSteadyStateTests)

add_executable(Project135ThermalFilterBridgeTests
    thermal/filter_bridge_tests.cpp
)
target_link_libraries(Project135ThermalFilterBridgeTests PRIVATE
    Project135::Thermal Project135::Filter Project135::Domain
)
add_test(NAME Project135ThermalFilterBridgeTests COMMAND Project135ThermalFilterBridgeTests)

if(MSVC)
    target_compile_options(Project135ThermalSteadyStateTests PRIVATE /UNDEBUG)
    target_compile_options(Project135ThermalFilterBridgeTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ThermalSteadyStateTests PRIVATE -UNDEBUG)
    target_compile_options(Project135ThermalFilterBridgeTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135DomainTests
    domain/magnetic_product_tests.cpp
)
target_link_libraries(Project135DomainTests PRIVATE Project135::Domain)
add_test(NAME Project135DomainTests COMMAND Project135DomainTests)
if(MSVC)
    target_compile_options(Project135DomainTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135DomainTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ComponentCatalogTests
    domain/component_catalog_tests.cpp
)
target_link_libraries(Project135ComponentCatalogTests PRIVATE Project135::Domain)
add_test(NAME Project135ComponentCatalogTests COMMAND Project135ComponentCatalogTests)
if(MSVC)
    target_compile_options(Project135ComponentCatalogTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ComponentCatalogTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ApplicationTests
    application/material_revision_tests.cpp
)
target_link_libraries(Project135ApplicationTests PRIVATE Project135::Application)
add_test(NAME Project135ApplicationTests COMMAND Project135ApplicationTests)
if(MSVC)
    target_compile_options(Project135ApplicationTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ApplicationTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135SqliteInfrastructureTests
    infrastructure/sqlite_repository_tests.cpp
)
target_link_libraries(Project135SqliteInfrastructureTests PRIVATE
    Project135::InfrastructureSqlite
    Project135::Application
)
add_test(
    NAME Project135SqliteInfrastructureTests
    COMMAND Project135SqliteInfrastructureTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135SqliteInfrastructureTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135SqliteInfrastructureTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135MechanicalGeometryTests
    domain/mechanical_geometry_tests.cpp
)
target_link_libraries(Project135MechanicalGeometryTests PRIVATE Project135::Domain)
add_test(NAME Project135MechanicalGeometryTests COMMAND Project135MechanicalGeometryTests)
if(MSVC)
    target_compile_options(Project135MechanicalGeometryTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135MechanicalGeometryTests PRIVATE -UNDEBUG)
endif()


add_executable(Project135CadTests
    cad/cad_tests.cpp
)
target_link_libraries(Project135CadTests PRIVATE Project135::Cad)
add_test(NAME Project135CadTests COMMAND Project135CadTests)
if(MSVC)
    target_compile_options(Project135CadTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135CadTests PRIVATE -UNDEBUG)
endif()
