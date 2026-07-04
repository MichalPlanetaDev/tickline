#include "tickline/protocol/authoritative_command_gateway.hpp"
#include "tickline/simulation/canonical_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandRejectionCode;
using tickline::command::CommandSession;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::protocol::AuthoritativeCommandGateway;
using tickline::protocol::ParseErrorCode;
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
        throw std::runtime_error{std::string{message}};
    }
}

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity = MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] World make_world()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position = Millimeters{100'000},
            .max_abs_velocity = MillimetersPerSecond{1'000},
        }};

    const auto result = world.add_entity(
        EntityState{
            .id = EntityId{1},
            .position = Position2{
                .x = Millimeters{0},
                .y = Millimeters{0},
            },
            .velocity = Velocity2{
                .x = MillimetersPerSecond{0},
                .y = MillimetersPerSecond{0},
            },
            .last_sequence = 0,
        });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "protocol gateway test entity could not be added"};
    }

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence = 1,
    const std::uint64_t target_tick = 2)
{
    return CommandEnvelope{
        .schema_version = tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = sequence,
        .target_tick = target_tick,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{1},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{25},
                .y = MillimetersPerSecond{-25},
            },
        },
    };
}

void write_u32(
    std::vector<std::byte>& bytes,
    const std::size_t offset,
    const std::uint32_t value)
{
    bytes[offset] = std::byte{
        static_cast<std::uint8_t>(value >> 24U)};

    bytes[offset + 1] = std::byte{
        static_cast<std::uint8_t>(value >> 16U)};

    bytes[offset + 2] = std::byte{
        static_cast<std::uint8_t>(value >> 8U)};

    bytes[offset + 3] = std::byte{
        static_cast<std::uint8_t>(value)};
}

void test_valid_frame_reaches_authoritative_pipeline()
{
    AuthoritativeCommandGateway gateway;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    const auto result = gateway.submit(session, world, frame);

    expect(result.parsed(), "valid frame should decode");
    expect(result.accepted(), "valid command frame should be accepted");
    expect(
        result.parse_error() == ParseErrorCode::none,
        "parsed frame should have no parse error");
    expect(
        result.command_result().has_value(),
        "parsed frame should expose command result");
    expect(
        result.command_result()->code() == CommandRejectionCode::none,
        "accepted command should have no rejection code");
    expect(
        session.highest_accepted_sequence() == 1,
        "accepted wire command should commit session sequence");
    expect(
        world.pending_command_count() == 1,
        "accepted wire command should reach the world queue");
    expect(
        gateway.evidence().size() == 1,
        "accepted wire command should produce command evidence");
}

void test_parser_rejection_mutates_no_authoritative_state()
{
    AuthoritativeCommandGateway gateway;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();
    const auto before =
        tickline::simulation::encode_world_state(world);

    auto frame =
        tickline::protocol::encode_command_frame(make_command());

    frame[0] = std::byte{0};

    const auto result = gateway.submit(session, world, frame);

    expect(!result.parsed(), "malformed frame should not decode");
    expect(!result.accepted(), "malformed frame cannot be accepted");
    expect(
        result.parse_error() == ParseErrorCode::invalid_magic,
        "gateway should preserve exact parser error");
    expect(
        !result.command_result().has_value(),
        "parser rejection should not expose command admission");
    expect(
        session.highest_accepted_sequence() == 0,
        "parser rejection must not consume session sequence");
    expect(
        tickline::simulation::encode_world_state(world) == before,
        "parser rejection must not mutate world state");
    expect(
        gateway.evidence().empty(),
        "incomplete malformed frame should not create command evidence");
}

void test_payload_size_mismatch_is_rejected_before_validation()
{
    AuthoritativeCommandGateway gateway;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();
    const auto encoded =
        tickline::protocol::encode_command_frame(make_command());

    std::vector<std::byte> frame{encoded.begin(), encoded.end() - 1};

    write_u32(
        frame,
        8,
        static_cast<std::uint32_t>(frame.size()));

    const auto result = gateway.submit(session, world, frame);

    expect(!result.parsed(), "wrong payload size should not decode");
    expect(
        result.parse_error() == ParseErrorCode::payload_size_mismatch,
        "gateway should classify exact payload-size mismatch");
    expect(
        gateway.evidence().empty(),
        "structural payload failure should not create command evidence");
}

void test_structurally_valid_rejection_produces_command_evidence()
{
    AuthoritativeCommandGateway gateway;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();
    auto command = make_command();
    command.schema_version = 999;

    const auto frame =
        tickline::protocol::encode_command_frame(command);

    const auto result = gateway.submit(session, world, frame);

    expect(result.parsed(), "unsupported command schema is structurally valid");
    expect(!result.accepted(), "unsupported command schema should be rejected");
    expect(
        result.parse_error() == ParseErrorCode::none,
        "authoritative rejection should not be reported as parser failure");
    expect(
        result.command_result()->code() ==
            CommandRejectionCode::unsupported_schema_version,
        "authoritative validator should classify unsupported schema");
    expect(
        gateway.evidence().size() == 1,
        "complete rejected command should produce command evidence");
    expect(
        session.highest_accepted_sequence() == 0,
        "authoritative rejection should not consume sequence state");
    expect(
        world.pending_command_count() == 0,
        "authoritative rejection should not queue a command");
}

void test_zero_entity_identifier_remains_a_parser_error()
{
    AuthoritativeCommandGateway gateway;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();
    auto frame =
        tickline::protocol::encode_command_frame(make_command());

    constexpr std::size_t entity_offset =
        tickline::protocol::frame_header_size + 36;

    for (std::size_t index = entity_offset;
         index < entity_offset + 8;
         ++index) {
        frame[index] = std::byte{0};
    }

    const auto result = gateway.submit(session, world, frame);

    expect(!result.parsed(), "zero entity identifier should not decode");
    expect(
        result.parse_error() == ParseErrorCode::invalid_entity_id,
        "zero entity identifier should have a stable parser error");
    expect(
        gateway.evidence().empty(),
        "unrepresentable entity identifier should not create command evidence");
}

}

int main()
{
    try {
        test_valid_frame_reaches_authoritative_pipeline();
        test_parser_rejection_mutates_no_authoritative_state();
        test_payload_size_mismatch_is_rejected_before_validation();
        test_structurally_valid_rejection_produces_command_evidence();
        test_zero_entity_identifier_remains_a_parser_error();
    } catch (const std::exception& error) {
        std::cerr
            << "authoritative command gateway test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
