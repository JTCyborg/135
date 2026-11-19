add_executable(Project135CoreTests
    core/core_smoke_tests.cpp
)

target_link_libraries(Project135CoreTests PRIVATE Project135::Core)

add_test(NAME Project135CoreTests COMMAND Project135CoreTests)
