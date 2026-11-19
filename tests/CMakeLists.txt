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
