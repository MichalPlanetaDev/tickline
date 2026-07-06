cmake_minimum_required(VERSION 3.22)

project(
    Tickline
    VERSION 0.8.0
    DESCRIPTION "Deterministic security-engineering range"
    LANGUAGES CXX
)

option(
    TICKLINE_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

option(
    TICKLINE_BUILD_FUZZ_TARGETS
    "Build libFuzzer protocol targets"
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

if(TICKLINE_BUILD_FUZZ_TARGETS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(
            FATAL_ERROR
            "TICKLINE_BUILD_FUZZ_TARGETS requires Clang with libFuzzer"
        )
    endif()

    target_compile_options(
        tickline_project_options
        INTERFACE
            -fsanitize=fuzzer-no-link
    )
endif()

add_library(
    tickline_core
    cpp/src/command/authoritative_command_pipeline.cpp
    cpp/src/command/command_evidence.cpp
    cpp/src/command/command_evidence_archive.cpp
    cpp/src/command/command_evidence_replay.cpp
    cpp/src/command/command_session.cpp
    cpp/src/command/command_validator.cpp
    cpp/src/protocol/authoritative_command_gateway.cpp
    cpp/src/protocol/command_envelope_codec.cpp
    cpp/src/protocol/frame_parser.cpp
    cpp/src/protocol/stream_parser.cpp
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
    tickline_command_evidence_replay_tests
    cpp/tests/command_evidence_replay_tests.cpp
)

tickline_add_test(
    tickline_sha256_tests
    cpp/tests/sha256_tests.cpp
)

tickline_add_test(
    tickline_protocol_frame_parser_tests
    cpp/tests/protocol_frame_parser_tests.cpp
)

tickline_add_test(
    tickline_command_envelope_codec_tests
    cpp/tests/command_envelope_codec_tests.cpp
)

tickline_add_test(
    tickline_authoritative_command_gateway_tests
    cpp/tests/authoritative_command_gateway_tests.cpp
)

tickline_add_test(
    tickline_protocol_stream_parser_tests
    cpp/tests/protocol_stream_parser_tests.cpp
)

tickline_add_test(
    tickline_protocol_compatibility_tests
    cpp/tests/protocol_compatibility_tests.cpp
)

add_executable(
    tickline_protocol_fuzz_corpus_tests
    cpp/tests/protocol_fuzz_corpus_tests.cpp
)

target_link_libraries(
    tickline_protocol_fuzz_corpus_tests
    PRIVATE
        tickline_core
        tickline_project_options
        tickline_project_warnings
)

add_test(
    NAME tickline_protocol_fuzz_corpus_tests
    COMMAND
        tickline_protocol_fuzz_corpus_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/fuzz/corpus/protocol_parser
)


if(TICKLINE_BUILD_FUZZ_TARGETS)
    add_executable(
        tickline_protocol_parser_fuzz
        cpp/fuzz/protocol_parser_fuzz.cpp
    )

    target_link_libraries(
        tickline_protocol_parser_fuzz
        PRIVATE
            tickline_core
            tickline_project_options
            tickline_project_warnings
    )

    target_link_options(
        tickline_protocol_parser_fuzz
        PRIVATE
            -fsanitize=fuzzer
    )
endif()

find_package(SQLite3 REQUIRED)

target_sources(
    tickline_core
    PRIVATE
        cpp/src/storage/investigation_store.cpp
        cpp/src/storage/investigation_bundle_exporter.cpp
)

target_link_libraries(
    tickline_core
    PUBLIC
        SQLite::SQLite3
)

tickline_add_test(
    tickline_investigation_store_tests
    cpp/tests/investigation_store_tests.cpp
)

tickline_add_test(
    tickline_investigation_bundle_exporter_tests
    cpp/tests/investigation_bundle_exporter_tests.cpp
)

target_compile_definitions(
    tickline_investigation_bundle_exporter_tests
    PRIVATE
        TICKLINE_UNITY_BUNDLE_FIXTURE=\"${CMAKE_CURRENT_SOURCE_DIR}/unity/TicklineForensics/Assets/Tickline/Forensics/Tests/EditMode/Fixtures/valid-investigation-bundle.json\"
)
