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

add_executable(Project135ThermalNetworkTests
    thermal/thermal_network_tests.cpp
)
target_link_libraries(Project135ThermalNetworkTests PRIVATE Project135::Thermal)
add_test(NAME Project135ThermalNetworkTests COMMAND Project135ThermalNetworkTests)

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
    target_compile_options(Project135ThermalNetworkTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ThermalSteadyStateTests PRIVATE -UNDEBUG)
    target_compile_options(Project135ThermalFilterBridgeTests PRIVATE -UNDEBUG)
    target_compile_options(Project135ThermalNetworkTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135DesignParameterTests
    domain/design_parameter_tests.cpp
)
target_link_libraries(Project135DesignParameterTests PRIVATE Project135::Domain)
add_test(NAME Project135DesignParameterTests COMMAND Project135DesignParameterTests)
if(MSVC)
    target_compile_options(Project135DesignParameterTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135DesignParameterTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135DesignDependencyTests
    domain/design_dependency_tests.cpp
)
target_link_libraries(Project135DesignDependencyTests PRIVATE Project135::Domain)
add_test(NAME Project135DesignDependencyTests COMMAND Project135DesignDependencyTests)
if(MSVC)
    target_compile_options(Project135DesignDependencyTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135DesignDependencyTests PRIVATE -UNDEBUG)
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

# Focused reproducer for the archived TEST-01 three-limb magnetic case.
# This target links only the domain library; the desktop application is not started.
add_executable(Project135ThreeLimbTest01Regression
    domain/three_limb_test01_regression.cpp
)
target_link_libraries(Project135ThreeLimbTest01Regression PRIVATE Project135::Domain)
add_test(
    NAME Project135ThreeLimbTest01Regression
    COMMAND Project135ThreeLimbTest01Regression
)
if(MSVC)
    target_compile_options(Project135ThreeLimbTest01Regression PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ThreeLimbTest01Regression PRIVATE -UNDEBUG)
endif()

add_executable(Project135ThreeLimbSolverTests
    domain/three_limb_solver_tests.cpp
)
target_link_libraries(Project135ThreeLimbSolverTests PRIVATE Project135::Domain)
add_test(NAME Project135ThreeLimbSolverTests COMMAND Project135ThreeLimbSolverTests)
if(MSVC)
    target_compile_options(Project135ThreeLimbSolverTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ThreeLimbSolverTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ThreeLimbGeometryAdapterTests
    domain/three_limb_geometry_adapter_tests.cpp
)
target_link_libraries(Project135ThreeLimbGeometryAdapterTests PRIVATE Project135::Domain)
add_test(NAME Project135ThreeLimbGeometryAdapterTests COMMAND Project135ThreeLimbGeometryAdapterTests)
if(MSVC)
    target_compile_options(Project135ThreeLimbGeometryAdapterTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ThreeLimbGeometryAdapterTests PRIVATE -UNDEBUG)
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

add_executable(Project135FilterReactorMagneticTests
    application/filter_reactor_magnetic_tests.cpp
)
target_link_libraries(Project135FilterReactorMagneticTests PRIVATE
    Project135::Application Project135::Domain Project135::Filter
)
add_test(NAME Project135FilterReactorMagneticTests COMMAND Project135FilterReactorMagneticTests)
if(MSVC)
    target_compile_options(Project135FilterReactorMagneticTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135FilterReactorMagneticTests PRIVATE -UNDEBUG)
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

add_executable(Project135ReactorDesignInputsTests
    application/reactor_design_inputs_tests.cpp
)
target_link_libraries(Project135ReactorDesignInputsTests PRIVATE Project135::Application)
add_test(NAME Project135ReactorDesignInputsTests COMMAND Project135ReactorDesignInputsTests)
if(MSVC)
    target_compile_options(Project135ReactorDesignInputsTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ReactorDesignInputsTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ReactorDesignGraphTests
    application/reactor_design_graph_tests.cpp
)
target_link_libraries(Project135ReactorDesignGraphTests PRIVATE Project135::Application)
add_test(NAME Project135ReactorDesignGraphTests COMMAND Project135ReactorDesignGraphTests)
if(MSVC)
    target_compile_options(Project135ReactorDesignGraphTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ReactorDesignGraphTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ReactorDesignExecutionTests
    application/reactor_design_execution_tests.cpp
)
target_link_libraries(Project135ReactorDesignExecutionTests PRIVATE Project135::Application)
add_test(NAME Project135ReactorDesignExecutionTests COMMAND Project135ReactorDesignExecutionTests)
if(MSVC)
    target_compile_options(Project135ReactorDesignExecutionTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ReactorDesignExecutionTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135PhysicalFilterReactorMagneticTests
    application/physical_filter_reactor_magnetic_tests.cpp
)
target_link_libraries(Project135PhysicalFilterReactorMagneticTests PRIVATE
    Project135::Application Project135::Domain Project135::Filter
)
add_test(
    NAME Project135PhysicalFilterReactorMagneticTests
    COMMAND Project135PhysicalFilterReactorMagneticTests
)
if(MSVC)
    target_compile_options(Project135PhysicalFilterReactorMagneticTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135PhysicalFilterReactorMagneticTests PRIVATE -UNDEBUG)
endif()


add_executable(Project135CoreMaterialRevisionLoaderTests
    infrastructure/core_material_revision_loader_tests.cpp
)
target_link_libraries(Project135CoreMaterialRevisionLoaderTests PRIVATE
    Project135::InfrastructureSqlite
)
add_test(
    NAME Project135CoreMaterialRevisionLoaderTests
    COMMAND Project135CoreMaterialRevisionLoaderTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135CoreMaterialRevisionLoaderTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135CoreMaterialRevisionLoaderTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135RevisionBoundPhysicalFilterMagneticTests
    infrastructure/revision_bound_physical_filter_magnetic_tests.cpp
)
target_link_libraries(Project135RevisionBoundPhysicalFilterMagneticTests PRIVATE
    Project135::InfrastructureSqlite
)
add_test(
    NAME Project135RevisionBoundPhysicalFilterMagneticTests
    COMMAND Project135RevisionBoundPhysicalFilterMagneticTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135RevisionBoundPhysicalFilterMagneticTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135RevisionBoundPhysicalFilterMagneticTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135ConductorMaterialRevisionLoaderTests
    infrastructure/conductor_material_revision_loader_tests.cpp
)
target_link_libraries(Project135ConductorMaterialRevisionLoaderTests PRIVATE
    Project135::InfrastructureSqlite
)
add_test(
    NAME Project135ConductorMaterialRevisionLoaderTests
    COMMAND Project135ConductorMaterialRevisionLoaderTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135ConductorMaterialRevisionLoaderTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135ConductorMaterialRevisionLoaderTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135WindingElectricalTests
    domain/winding_electrical_tests.cpp
)
target_link_libraries(Project135WindingElectricalTests PRIVATE Project135::Domain)
add_test(NAME Project135WindingElectricalTests COMMAND Project135WindingElectricalTests)
if(MSVC)
    target_compile_options(Project135WindingElectricalTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135WindingElectricalTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135RevisionBoundWindingCopperLossTests
    infrastructure/revision_bound_winding_copper_loss_tests.cpp
)
target_link_libraries(Project135RevisionBoundWindingCopperLossTests PRIVATE
    Project135::InfrastructureSqlite
)
add_test(
    NAME Project135RevisionBoundWindingCopperLossTests
    COMMAND Project135RevisionBoundWindingCopperLossTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135RevisionBoundWindingCopperLossTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135RevisionBoundWindingCopperLossTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135WindingElectrothermalTests
    thermal/winding_electrothermal_tests.cpp
)
target_link_libraries(Project135WindingElectrothermalTests PRIVATE
    Project135::Thermal Project135::Domain
)
add_test(NAME Project135WindingElectrothermalTests COMMAND Project135WindingElectrothermalTests)
if(MSVC)
    target_compile_options(Project135WindingElectrothermalTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135WindingElectrothermalTests PRIVATE -UNDEBUG)
endif()

add_executable(Project135RevisionBoundWindingElectrothermalTests
    infrastructure/revision_bound_winding_electrothermal_tests.cpp
)
target_link_libraries(Project135RevisionBoundWindingElectrothermalTests PRIVATE
    Project135::InfrastructureSqlite Project135::Thermal
)
add_test(
    NAME Project135RevisionBoundWindingElectrothermalTests
    COMMAND Project135RevisionBoundWindingElectrothermalTests
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/001_initial_schema.sql
        ${CMAKE_SOURCE_DIR}/src/infrastructure/sqlite/migrations/002_calculation_snapshots.sql
)
if(MSVC)
    target_compile_options(Project135RevisionBoundWindingElectrothermalTests PRIVATE /UNDEBUG)
else()
    target_compile_options(Project135RevisionBoundWindingElectrothermalTests PRIVATE -UNDEBUG)
endif()
