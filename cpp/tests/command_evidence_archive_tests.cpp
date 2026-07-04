#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/command/command_evidence_archive.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using tickline::command::AuthoritativeCommandPipeline;
using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandEvidenceArchiveError;
using tickline::command::CommandEvidenceArchiveErrorCode;
using tickline::command::CommandEvidenceDecodeError;
using tickline::command::CommandEvidenceDecodeErrorCode;
using tickline::command::CommandSession;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::security::Sha256Digest;
using tickline::simulation::AddEntityResult;
using tickline::simulation::EntityId;
using tickline::simulation::EntityState;
using tickline::simulation::Microseconds;
using tickline::simulation::Millimeters;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Position2;
using tickline::simulation::Velocity2;
using tickline::simulation::World;
using tickline::simulation::WorldLimits;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{
            std::string{message}};
    }
}

template <typename Function>
void expect_archive_error(
    Function function,
    const CommandEvidenceArchiveErrorCode expected,
    const std::string_view message)
{
    try {
        function();
    } catch (const CommandEvidenceArchiveError& error) {
        expect(error.code() == expected, message);
        return;
    }

    throw std::runtime_error{
        std::string{message}};
}

template <typename Function>
void expect_decode_error(
    Function function,
    const CommandEvidenceDecodeErrorCode expected,
    const std::string_view message)
{
    try {
        function();
    } catch (const CommandEvidenceDecodeError& error) {
        expect(error.code() == expected, message);
        return;
    }

    throw std::runtime_error{
        std::string{message}};
}

[[nodiscard]] std::string bytes_to_hex(
    const std::span<const std::byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    std::string output;
    output.reserve(bytes.size() * 2);

    for (const auto byte : bytes) {
        const auto value =
            std::to_integer<std::uint8_t>(
                byte);

        output.push_back(
            digits[
                static_cast<std::size_t>(
                    value >> 4U)]);

        output.push_back(
            digits[
                static_cast<std::size_t>(
                    value & 0x0fU)]);
    }

    return output;
}

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity =
            MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] World make_world()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position =
                Millimeters{100'000},
            .max_abs_velocity =
                MillimetersPerSecond{1'000},
        }};

    const auto result =
        world.add_entity(
            EntityState{
                .id = EntityId{3},
                .position =
                    Position2{
                        .x = Millimeters{0},
                        .y = Millimeters{0},
                    },
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{0},
                        .y =
                            MillimetersPerSecond{0},
                    },
                .last_sequence = 0,
            });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "archive test entity could not be added"};
    }

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 3)
{
    return CommandEnvelope{
        .schema_version =
            tickline::command::
                command_schema_version,
        .type =
            CommandType::set_velocity,
        .client_id =
            ClientId{7},
        .session_id =
            SessionId{11},
        .sequence =
            sequence,
        .target_tick =
            target_tick,
        .payload =
            SetVelocityPayload{
                .entity_id =
                    EntityId{entity_id},
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{25},
                        .y =
                            MillimetersPerSecond{-40},
                    },
            },
    };
}

void populate_history(
    AuthoritativeCommandPipeline& pipeline,
    CommandSession& session,
    World& world)
{
    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 3)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3, 99)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3)));
}

void write_u16(
    std::vector<std::byte>& bytes,
    const std::size_t offset,
    const std::uint16_t value)
{
    bytes[offset] =
        static_cast<std::byte>(
            (value >> 8U) & 0xffU);

    bytes[offset + 1] =
        static_cast<std::byte>(
            value & 0xffU);
}

class TemporaryDirectory final {
public:
    TemporaryDirectory()
    {
        const auto seed =
            std::chrono::high_resolution_clock::
                now()
                .time_since_epoch()
                .count();

        for (int attempt = 0;
             attempt < 100;
             ++attempt) {
            path_ =
                std::filesystem::
                    temp_directory_path() /
                (
                    "tickline-evidence-" +
                    std::to_string(seed) +
                    "-" +
                    std::to_string(attempt));

            std::error_code error;

            if (std::filesystem::create_directory(
                    path_,
                    error)) {
                return;
            }
        }

        throw std::runtime_error{
            "temporary archive directory could not be created"};
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(
            path_,
            error);
    }

    [[nodiscard]] const std::filesystem::path&
    path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void test_empty_archive_exact_layout()
{
    const auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                std::span<
                    const tickline::command::
                        CommandEvidenceRecord>{},
                Sha256Digest{});

    const std::string expected{
        "544c4341"
        "0001"
        "0002"
        "000000a0"
        "00000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000",
    };

    expect(
        encoded.size() ==
            tickline::command::
                command_evidence_archive_header_size,
        "empty archive should contain only its header");

    expect(
        bytes_to_hex(encoded) == expected,
        "empty archive header must match its canonical layout");

    const auto decoded =
        tickline::command::
            decode_command_evidence_archive(
                encoded,
                Sha256Digest{});

    expect(
        decoded.records.empty(),
        "empty archive should decode without records");

    expect(
        decoded.head_digest ==
            Sha256Digest{},
        "empty archive should use the zero chain head");
}

void test_archive_round_trip()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    populate_history(
        pipeline,
        session,
        world);

    const auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                pipeline.evidence().records(),
                pipeline.evidence().head_digest());

    const auto decoded =
        tickline::command::
            decode_command_evidence_archive(
                encoded,
                pipeline.evidence().head_digest());

    const auto original =
        pipeline.evidence().records();

    expect(
        decoded.records.size() ==
            original.size(),
        "archive round trip should preserve record count");

    for (std::size_t index = 0;
         index < original.size();
         ++index) {
        expect(
            decoded.records[index] ==
                original[index],
            "archive round trip should preserve each record");
    }

    expect(
        decoded.head_digest ==
            pipeline.evidence().head_digest(),
        "archive round trip should preserve the chain head");
}

void test_truncation_and_trailing_data()
{
    auto header =
        tickline::command::
            encode_command_evidence_archive(
                std::span<
                    const tickline::command::
                        CommandEvidenceRecord>{},
                Sha256Digest{});

    header.resize(
        tickline::command::
            command_evidence_archive_header_size -
        1);

    expect_archive_error(
        [&header] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        header,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            truncated_header,
        "truncated archive header should be rejected");

    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    populate_history(
        pipeline,
        session,
        world);

    auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                pipeline.evidence().records(),
                pipeline.evidence().head_digest());

    auto truncated = encoded;
    truncated.pop_back();

    expect_archive_error(
        [&truncated, &pipeline] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        truncated,
                        pipeline.evidence().
                            head_digest()));
        },
        CommandEvidenceArchiveErrorCode::
            truncated_records,
        "truncated archive record should be rejected");

    auto trailing = encoded;
    trailing.push_back(std::byte{0});

    expect_archive_error(
        [&trailing, &pipeline] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        trailing,
                        pipeline.evidence().
                            head_digest()));
        },
        CommandEvidenceArchiveErrorCode::
            trailing_data,
        "archive trailing data should be rejected");
}

void test_archive_header_validation()
{
    auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                std::span<
                    const tickline::command::
                        CommandEvidenceRecord>{},
                Sha256Digest{});

    auto invalid_magic = encoded;
    invalid_magic[0] = std::byte{'X'};

    expect_archive_error(
        [&invalid_magic] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        invalid_magic,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            invalid_magic,
        "invalid archive magic should be rejected");

    auto archive_schema = encoded;
    write_u16(archive_schema, 4, 2);

    expect_archive_error(
        [&archive_schema] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        archive_schema,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            unsupported_archive_schema,
        "unsupported archive schema should be rejected");

    auto record_schema = encoded;
    write_u16(record_schema, 6, 3);

    expect_archive_error(
        [&record_schema] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        record_schema,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            unsupported_record_schema,
        "unsupported record schema should be rejected");

    auto record_size = encoded;
    record_size[11] = std::byte{0x9f};

    expect_archive_error(
        [&record_size] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        record_size,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            invalid_record_size,
        "invalid archive record size should be rejected");

    auto reserved = encoded;
    reserved[15] = std::byte{1};

    expect_archive_error(
        [&reserved] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        reserved,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            nonzero_reserved_field,
        "nonzero archive reserved field should be rejected");
}

void test_embedded_record_schema_is_strict()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                pipeline.evidence().records(),
                pipeline.evidence().head_digest());

    const auto record_schema_offset =
        tickline::command::
            command_evidence_archive_header_size +
        4;

    write_u16(
        encoded,
        record_schema_offset,
        3);

    expect_decode_error(
        [&encoded, &pipeline] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        encoded,
                        pipeline.evidence().
                            head_digest()));
        },
        CommandEvidenceDecodeErrorCode::
            unsupported_schema_version,
        "embedded unsupported record schema should be rejected");
}

void test_record_tampering_breaks_chain()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                pipeline.evidence().records(),
                pipeline.evidence().head_digest());

    const auto world_fingerprint_offset =
        tickline::command::
            command_evidence_archive_header_size +
        48;

    encoded[world_fingerprint_offset] ^=
        std::byte{1};

    expect_archive_error(
        [&encoded, &pipeline] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        encoded,
                        pipeline.evidence().
                            head_digest()));
        },
        CommandEvidenceArchiveErrorCode::
            chain_verification_failed,
        "tampered record should fail SHA-256 chain verification");
}

void test_trusted_head_is_external()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    const auto encoded =
        tickline::command::
            encode_command_evidence_archive(
                pipeline.evidence().records(),
                pipeline.evidence().head_digest());

    expect_archive_error(
        [&encoded] {
            static_cast<void>(
                tickline::command::
                    decode_command_evidence_archive(
                        encoded,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            trusted_head_mismatch,
        "self-declared archive head must not replace the trusted head");
}

void test_filesystem_round_trip()
{
    TemporaryDirectory directory;

    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    populate_history(
        pipeline,
        session,
        world);

    const auto path =
        directory.path() /
        "command-evidence.tlca";

    tickline::command::
        write_command_evidence_archive(
            path,
            pipeline.evidence());

    const auto loaded =
        tickline::command::
            read_command_evidence_archive(
                path,
                pipeline.evidence().head_digest());

    const auto original =
        pipeline.evidence().records();

    expect(
        loaded.records.size() ==
            original.size(),
        "filesystem round trip should preserve record count");

    for (std::size_t index = 0;
         index < original.size();
         ++index) {
        expect(
            loaded.records[index] ==
                original[index],
            "filesystem round trip should preserve records");
    }

    expect(
        loaded.head_digest ==
            pipeline.evidence().head_digest(),
        "filesystem round trip should preserve the trusted head");
}

void test_missing_file_is_reported()
{
    TemporaryDirectory directory;

    const auto missing =
        directory.path() /
        "missing.tlca";

    expect_archive_error(
        [&missing] {
            static_cast<void>(
                tickline::command::
                    read_command_evidence_archive(
                        missing,
                        Sha256Digest{}));
        },
        CommandEvidenceArchiveErrorCode::
            io_error,
        "missing archive should produce an I/O error");
}

}

int main()
{
    try {
        test_empty_archive_exact_layout();
        test_archive_round_trip();
        test_truncation_and_trailing_data();
        test_archive_header_validation();
        test_embedded_record_schema_is_strict();
        test_record_tampering_breaks_chain();
        test_trusted_head_is_external();
        test_filesystem_round_trip();
        test_missing_file_is_reported();
    } catch (const std::exception& error) {
        std::cerr
            << "command evidence archive test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
