#include "tickline/protocol/command_envelope_codec.hpp"
#include "tickline/protocol/frame_parser.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandType;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::protocol::MessageType;
using tickline::protocol::ParseErrorCode;
using tickline::protocol::ProtocolLimits;
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

template <typename ExpectedException, typename Function>
void expect_throws(
    Function function,
    const std::string_view message)
{
    try {
        function();
    } catch (const ExpectedException&) {
        return;
    }

    throw std::runtime_error{std::string{message}};
}

[[nodiscard]] CommandEnvelope make_command()
{
    return CommandEnvelope{
        .schema_version = tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = 1,
        .target_tick = 2,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{3},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{25},
                .y = MillimetersPerSecond{-25},
            },
        },
    };
}

template <std::size_t Size>
void write_u16(
    std::array<std::byte, Size>& bytes,
    const std::size_t offset,
    const std::uint16_t value)
{
    bytes[offset] = std::byte{
        static_cast<std::uint8_t>(value >> 8U)};

    bytes[offset + 1] = std::byte{
        static_cast<std::uint8_t>(value)};
}

template <std::size_t Size>
void write_u32(
    std::array<std::byte, Size>& bytes,
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

void expect_error(
    const std::span<const std::byte> bytes,
    const ParseErrorCode expected,
    const std::string_view message,
    const ProtocolLimits limits = {})
{
    const auto result =
        tickline::protocol::parse_frame(bytes, limits);

    expect(!result.has_value(), message);
    expect(result.error() == expected, message);
}

void test_valid_header_and_frame()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    const auto header =
        tickline::protocol::decode_frame_header(frame);

    expect(header.has_value(), "valid header should decode");

    expect(
        header->version == tickline::protocol::protocol_version,
        "protocol version should be preserved");

    expect(
        header->header_size == tickline::protocol::frame_header_size,
        "header size should be preserved");

    expect(
        header->frame_size == tickline::protocol::command_frame_size,
        "frame size should be preserved");

    expect(
        header->message_type == MessageType::command_envelope,
        "message type should be preserved");

    expect(header->flags == 0, "reserved flags should be zero");

    const auto parsed =
        tickline::protocol::parse_frame(frame);

    expect(parsed.has_value(), "valid frame should parse");

    expect(
        parsed->payload.size() ==
            tickline::protocol::command_envelope_payload_size,
        "parsed payload should have the fixed command size");
}

void test_truncated_header()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    for (std::size_t size = 0;
         size < tickline::protocol::frame_header_size;
         ++size) {
        const auto bytes = std::span<const std::byte>{frame}.first(size);

        expect_error(
            bytes,
            ParseErrorCode::truncated_header,
            "short header should be classified as truncated");
    }
}

void test_header_field_rejections()
{
    const auto original =
        tickline::protocol::encode_command_frame(make_command());

    auto frame = original;
    frame[0] = std::byte{0};

    expect_error(
        frame,
        ParseErrorCode::invalid_magic,
        "invalid magic should be rejected");

    frame = original;
    write_u16(frame, 4, 2);

    expect_error(
        frame,
        ParseErrorCode::unsupported_protocol_version,
        "unsupported protocol version should be rejected");

    frame = original;
    write_u16(frame, 6, 15);

    expect_error(
        frame,
        ParseErrorCode::invalid_header_size,
        "unexpected header size should be rejected");

    frame = original;
    write_u32(frame, 8, 15);

    expect_error(
        frame,
        ParseErrorCode::frame_too_small,
        "declared frame smaller than the header should be rejected");

    frame = original;
    write_u32(
        frame,
        8,
        tickline::protocol::default_max_frame_size + 1);

    expect_error(
        frame,
        ParseErrorCode::frame_too_large,
        "oversized frame should be rejected before body parsing");

    frame = original;
    write_u16(frame, 14, 1);

    expect_error(
        frame,
        ParseErrorCode::reserved_flags_set,
        "non-zero reserved flags should be rejected");

    frame = original;
    write_u16(frame, 12, 999);

    expect_error(
        frame,
        ParseErrorCode::unknown_message_type,
        "unknown message type should be rejected");
}

void test_declared_length_must_match_available_bytes()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    const std::vector<std::byte> truncated{
        frame.begin(),
        frame.end() - 1,
    };

    expect_error(
        truncated,
        ParseErrorCode::truncated_frame,
        "missing body byte should be classified as truncated frame");

    std::vector<std::byte> trailing{
        frame.begin(),
        frame.end(),
    };

    trailing.push_back(std::byte{0});

    expect_error(
        trailing,
        ParseErrorCode::trailing_data,
        "strict one-frame parser should reject trailing bytes");
}

void test_configured_frame_limit()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    const auto exact_limit = ProtocolLimits{
        .maximum_frame_size =
            static_cast<std::uint32_t>(frame.size()),
    };

    expect(
        tickline::protocol::parse_frame(frame, exact_limit).has_value(),
        "frame exactly at the configured limit should parse");

    const auto lower_limit = ProtocolLimits{
        .maximum_frame_size =
            static_cast<std::uint32_t>(frame.size() - 1),
    };

    expect_error(
        frame,
        ParseErrorCode::frame_too_large,
        "frame above the configured limit should be rejected",
        lower_limit);

    expect_throws<std::invalid_argument>(
        [&frame] {
            static_cast<void>(
                tickline::protocol::parse_frame(
                    frame,
                    ProtocolLimits{
                        .maximum_frame_size = 15,
                    }));
        },
        "trusted parser configuration smaller than the header should throw");
}

void test_error_code_contract()
{
    static_assert(
        static_cast<std::underlying_type_t<ParseErrorCode>>(
            ParseErrorCode::none) == 0);

    static_assert(
        static_cast<std::underlying_type_t<ParseErrorCode>>(
            ParseErrorCode::invalid_magic) == 2);

    static_assert(
        static_cast<std::underlying_type_t<ParseErrorCode>>(
            ParseErrorCode::payload_size_mismatch) == 11);

    expect(
        tickline::protocol::parse_error_code_name(
            ParseErrorCode::trailing_data) == "trailing_data",
        "parse errors should expose stable machine names");

    expect(
        tickline::protocol::parse_error_code_name(
            static_cast<ParseErrorCode>(999)) == "unknown",
        "unknown parse errors should have a stable fallback name");
}

}

int main()
{
    try {
        test_valid_header_and_frame();
        test_truncated_header();
        test_header_field_rejections();
        test_declared_length_must_match_available_bytes();
        test_configured_frame_limit();
        test_error_code_contract();
    } catch (const std::exception& error) {
        std::cerr
            << "protocol frame parser test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
