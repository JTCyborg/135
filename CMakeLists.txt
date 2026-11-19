cmake_minimum_required(VERSION 3.25)

project(Project135
    VERSION 0.1.0
    DESCRIPTION "Engineering design and calculation platform for wound magnetic products and passive power electronics"
    LANGUAGES CXX
)

option(P135_BUILD_TESTS "Build Project135 unit and integration tests" ON)
option(P135_BUILD_DESKTOP "Build the Qt desktop application" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(Project135Core INTERFACE)
add_library(Project135::Core ALIAS Project135Core)

target_include_directories(Project135Core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core/include>
)

target_compile_features(Project135Core INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(Project135Core INTERFACE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Project135Core INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/core)
add_subdirectory(src/circuit)
add_subdirectory(src/filter)

if(P135_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(P135_BUILD_DESKTOP)
    # Qt can be selected explicitly through Qt6_DIR or P135_QT6_ROOT.
    # On Windows, also discover the standard Qt for MSVC installation layout.
    set(P135_QT6_ROOT "" CACHE PATH "Qt 6 installation root (contains lib/cmake/Qt6)")

    if(NOT Qt6_DIR)
        if(P135_QT6_ROOT AND EXISTS "${P135_QT6_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
            set(Qt6_DIR "${P135_QT6_ROOT}/lib/cmake/Qt6")
        elseif(WIN32)
            file(GLOB _P135_QT6_CONFIGS LIST_DIRECTORIES false
                "C:/Qt/*/msvc2022_64/lib/cmake/Qt6/Qt6Config.cmake"
            )
            if(_P135_QT6_CONFIGS)
                list(SORT _P135_QT6_CONFIGS COMPARE NATURAL ORDER DESCENDING)
                list(GET _P135_QT6_CONFIGS 0 _P135_QT6_CONFIG)
                get_filename_component(Qt6_DIR "${_P135_QT6_CONFIG}" DIRECTORY)
                message(STATUS "Project135: auto-detected Qt 6 at ${Qt6_DIR}")
            endif()
        endif()
    endif()

    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    add_subdirectory(app/desktop)
endif()
