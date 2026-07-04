cmake_minimum_required(VERSION 3.22)

project(
    Tickline
    VERSION 0.2.0
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
    cpp/src/command/authoritative_command_pipeline.cpp
    cpp/src/command/command_evidence.cpp
    cpp/src/command/command_evidence_archive.cpp
    cpp/src/command/command_session.cpp
    cpp/src/command/command_validator.cpp
    cpp/src/security/sha256.cpp
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

function(tickline_add_test target source)
    add_executable(
        ${target}
        ${source}
    )

    target_link_libraries(
        ${target}
        PRIVATE
            tickline_core
            tickline_project_options
            tickline_project_warnings
    )

    add_test(
        NAME ${target}
        COMMAND ${target}
    )
endfunction()

tickline_add_test(
    tickline_version_tests
    cpp/tests/version_tests.cpp
)

tickline_add_test(
    tickline_simulation_units_tests
    cpp/tests/simulation_units_tests.cpp
)

tickline_add_test(
    tickline_world_tests
    cpp/tests/world_tests.cpp
)

tickline_add_test(
    tickline_canonical_state_tests
    cpp/tests/canonical_state_tests.cpp
)

tickline_add_test(
    tickline_command_validator_tests
    cpp/tests/command_validator_tests.cpp
)

tickline_add_test(
    tickline_command_session_tests
    cpp/tests/command_session_tests.cpp
)

tickline_add_test(
    tickline_authoritative_command_pipeline_tests
    cpp/tests/authoritative_command_pipeline_tests.cpp
)

tickline_add_test(
    tickline_command_evidence_tests
    cpp/tests/command_evidence_tests.cpp
)

tickline_add_test(
    tickline_command_evidence_archive_tests
    cpp/tests/command_evidence_archive_tests.cpp
)

tickline_add_test(
    tickline_sha256_tests
    cpp/tests/sha256_tests.cpp
)
