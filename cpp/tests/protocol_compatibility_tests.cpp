#include "tickline/protocol/command_envelope_codec.hpp"
#include "tickline/protocol/frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandType;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::protocol::MessageType;
using tickline::protocol::ParseErrorCode;
using tickline::simulation::EntityId;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Velocity2;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

[[nodiscard]] CommandEnvelope make_command()
{
    return CommandEnvelope{
        .schema_version = 1,
        .type = CommandType::set_velocity,
        .client_id = ClientId{0x0102'0304'0506'0708ULL},
        .session_id = SessionId{0x1112'1314'1516'1718ULL},
        .sequence = 0x2122'2324'2526'2728ULL,
        .target_tick = 0x3132'3334'3536'3738ULL,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{0x4142'4344'4546'4748ULL},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{-2},
                .y = MillimetersPerSecond{3},
            },
        },
    };
}

void test_numeric_contract()
{
    using ErrorValue = std::underlying_type_t<ParseErrorCode>;
    using MessageValue = std::underlying_type_t<MessageType>;

    static_assert(tickline::protocol::protocol_version == 1);
    static_assert(tickline::protocol::frame_header_size == 16);
    static_assert(tickline::protocol::command_envelope_payload_size == 60);
    static_assert(tickline::protocol::command_frame_size == 76);

    static_assert(
        static_cast<MessageValue>(MessageType::command_envelope) == 1);

    static_assert(static_cast<ErrorValue>(ParseErrorCode::none) == 0);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::truncated_header) == 1);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::invalid_magic) == 2);
    static_assert(
        static_cast<ErrorValue>(
            ParseErrorCode::unsupported_protocol_version) == 3);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::invalid_header_size) == 4);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::frame_too_small) == 5);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::frame_too_large) == 6);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::reserved_flags_set) == 7);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::unknown_message_type) == 8);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::truncated_frame) == 9);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::trailing_data) == 10);
    static_assert(
        static_cast<ErrorValue>(ParseErrorCode::payload_size_mismatch) == 11);
    static_assert(static_cast<ErrorValue>(ParseErrorCode::invalid_entity_id) == 12);
}

void test_error_names()
{
    const std::array expected{
        std::pair{ParseErrorCode::none, std::string_view{"none"}},
        std::pair{ParseErrorCode::truncated_header, std::string_view{"truncated_header"}},
        std::pair{ParseErrorCode::invalid_magic, std::string_view{"invalid_magic"}},
        std::pair{ParseErrorCode::unsupported_protocol_version, std::string_view{"unsupported_protocol_version"}},
        std::pair{ParseErrorCode::invalid_header_size, std::string_view{"invalid_header_size"}},
        std::pair{ParseErrorCode::frame_too_small, std::string_view{"frame_too_small"}},
        std::pair{ParseErrorCode::frame_too_large, std::string_view{"frame_too_large"}},
        std::pair{ParseErrorCode::reserved_flags_set, std::string_view{"reserved_flags_set"}},
        std::pair{ParseErrorCode::unknown_message_type, std::string_view{"unknown_message_type"}},
        std::pair{ParseErrorCode::truncated_frame, std::string_view{"truncated_frame"}},
        std::pair{ParseErrorCode::trailing_data, std::string_view{"trailing_data"}},
        std::pair{ParseErrorCode::payload_size_mismatch, std::string_view{"payload_size_mismatch"}},
        std::pair{ParseErrorCode::invalid_entity_id, std::string_view{"invalid_entity_id"}},
    };

    for (const auto& [code, name] : expected) {
        expect(
            tickline::protocol::parse_error_code_name(code) == name,
            "parse error name changed incompatibly");
    }
}

void test_wire_offsets()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    const std::array<std::uint8_t, 16> expected_header{
        0x54,
        0x49,
        0x43,
        0x4b,
        0x00,
        0x01,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x4c,
        0x00,
        0x01,
        0x00,
        0x00,
    };

    for (std::size_t index = 0; index < expected_header.size(); ++index) {
        expect(
            std::to_integer<std::uint8_t>(frame[index]) ==
                expected_header[index],
            "frame header byte changed incompatibly");
    }

    expect(
        std::to_integer<std::uint8_t>(frame[16]) == 0x00 &&
            std::to_integer<std::uint8_t>(frame[17]) == 0x01,
        "command schema offset changed");

    expect(
        std::to_integer<std::uint8_t>(frame[18]) == 0x00 &&
            std::to_integer<std::uint8_t>(frame[19]) == 0x01,
        "command type offset changed");

    expect(
        std::to_integer<std::uint8_t>(frame[20]) == 0x01 &&
            std::to_integer<std::uint8_t>(frame[27]) == 0x08,
        "client identifier offset changed");

    for (std::size_t index = 60; index < 67; ++index) {
        expect(
            std::to_integer<std::uint8_t>(frame[index]) == 0xff,
            "negative velocity high byte changed");
    }

    expect(
        std::to_integer<std::uint8_t>(frame[67]) == 0xfe,
        "negative velocity low byte changed");

    expect(
        std::to_integer<std::uint8_t>(frame[75]) == 0x03,
        "final velocity byte changed");
}

}

int main()
{
    try {
        test_numeric_contract();
        test_error_names();
        test_wire_offsets();
    } catch (const std::exception& error) {
        std::cerr
            << "protocol compatibility test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
