cmake_minimum_required(VERSION 3.22)

project(
    Tickline
    VERSION 0.1.1
    DESCRIPTION "Deterministic security-engineering range"
    LANGUAGES CXX
)

option(
    TICKLINE_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

add_library(tickline_project_options INTERFACE)
target_compile_features(tickline_project_options INTERFACE cxx_std_23)

add_library(tickline_project_warnings INTERFACE)

if(MSVC)
    target_compile_options(
        tickline_project_warnings
        INTERFACE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        tickline_project_warnings
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
    )
endif()

if(TICKLINE_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(
        tickline_project_options
        INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
    )

    target_link_options(
        tickline_project_options
        INTERFACE
            -fsanitize=address,undefined
    )
endif()

add_library(
    tickline_core
    cpp/src/simulation/canonical_state.cpp
    cpp/src/simulation/state_fingerprint.cpp
    cpp/src/simulation/world.cpp
    cpp/src/version.cpp
)

target_include_directories(
    tickline_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
)

target_link_libraries(
    tickline_core
    PUBLIC
        tickline_project_options
    PRIVATE
        tickline_project_warnings
)

enable_testing()

add_executable(
    tickline_version_tests
    cpp/tests/version_tests.cpp
)

target_link_libraries(
    tickline_version_tests
    PRIVATE
        tickline_core
        tickline_project_options
        tickline_project_warnings
)

add_test(
    NAME tickline_version_tests
    COMMAND tickline_version_tests
)

add_executable(
    tickline_simulation_units_tests
    cpp/tests/simulation_units_tests.cpp
)

target_link_libraries(
    tickline_simulation_units_tests
    PRIVATE
        tickline_core
        tickline_project_options
        tickline_project_warnings
)

add_test(
    NAME tickline_simulation_units_tests
    COMMAND tickline_simulation_units_tests
)

add_executable(
    tickline_world_tests
    cpp/tests/world_tests.cpp
)

target_link_libraries(
    tickline_world_tests
    PRIVATE
        tickline_core
        tickline_project_options
        tickline_project_warnings
)

add_test(
    NAME tickline_world_tests
    COMMAND tickline_world_tests
)

add_executable(
    tickline_canonical_state_tests
    cpp/tests/canonical_state_tests.cpp
)

target_link_libraries(
    tickline_canonical_state_tests
    PRIVATE
        tickline_core
        tickline_project_options
        tickline_project_warnings
)

add_test(
    NAME tickline_canonical_state_tests
    COMMAND tickline_canonical_state_tests
)
