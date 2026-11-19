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
