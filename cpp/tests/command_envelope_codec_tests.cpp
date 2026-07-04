#include "tickline/protocol/command_envelope_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandType;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
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

[[nodiscard]] CommandEnvelope make_command(
    const std::int64_t velocity_x = -9'876'543'210,
    const std::int64_t velocity_y = 1'234'567'890)
{
    return CommandEnvelope{
        .schema_version = tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{0x0102'0304'0506'0708ULL},
        .session_id = SessionId{0x1112'1314'1516'1718ULL},
        .sequence = 0x2122'2324'2526'2728ULL,
        .target_tick = 0x3132'3334'3536'3738ULL,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{0x4142'4344'4546'4748ULL},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{velocity_x},
                .y = MillimetersPerSecond{velocity_y},
            },
        },
    };
}

[[nodiscard]] std::uint8_t byte_value(
    const std::byte value)
{
    return std::to_integer<std::uint8_t>(value);
}

void test_round_trip_preserves_every_field()
{
    const auto command = make_command();
    const auto bytes =
        tickline::protocol::encode_command_envelope(command);

    const auto decoded =
        tickline::protocol::decode_command_envelope(bytes);

    expect(decoded.has_value(), "encoded command should decode");
    expect(decoded.value() == command, "round trip should preserve command");
}

void test_signed_extremes_round_trip()
{
    const auto command = make_command(
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max());

    const auto decoded =
        tickline::protocol::decode_command_envelope(
            tickline::protocol::encode_command_envelope(command));

    expect(decoded.has_value(), "signed extrema should decode");
    expect(decoded.value() == command, "signed extrema should round trip");
}

void test_big_endian_encoding()
{
    const auto bytes =
        tickline::protocol::encode_command_envelope(make_command());

    const std::array<std::uint8_t, 8> expected_client{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
    };

    for (std::size_t index = 0; index < expected_client.size(); ++index) {
        expect(
            byte_value(bytes[4 + index]) == expected_client[index],
            "client identifier should be encoded big-endian");
    }

    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    expect(byte_value(frame[0]) == 0x54, "magic byte zero should be T");
    expect(byte_value(frame[1]) == 0x49, "magic byte one should be I");
    expect(byte_value(frame[2]) == 0x43, "magic byte two should be C");
    expect(byte_value(frame[3]) == 0x4b, "magic byte three should be K");
    expect(byte_value(frame[8]) == 0, "small frame size should have zero high byte");
    expect(
        byte_value(frame[11]) == tickline::protocol::command_frame_size,
        "frame size should be encoded big-endian");
}

void test_payload_size_is_exact()
{
    const auto bytes =
        tickline::protocol::encode_command_envelope(make_command());

    const auto short_payload =
        std::span<const std::byte>{bytes}.first(bytes.size() - 1);

    const auto short_result =
        tickline::protocol::decode_command_envelope(short_payload);

    expect(!short_result.has_value(), "short payload should fail");
    expect(
        short_result.error() == ParseErrorCode::payload_size_mismatch,
        "short payload should use the stable payload-size error");

    std::vector<std::byte> long_payload{bytes.begin(), bytes.end()};
    long_payload.push_back(std::byte{0});

    const auto long_result =
        tickline::protocol::decode_command_envelope(long_payload);

    expect(!long_result.has_value(), "long payload should fail");
    expect(
        long_result.error() == ParseErrorCode::payload_size_mismatch,
        "long payload should use the stable payload-size error");
}

void test_zero_entity_identifier_is_rejected()
{
    auto bytes =
        tickline::protocol::encode_command_envelope(make_command());

    for (std::size_t index = 36; index < 44; ++index) {
        bytes[index] = std::byte{0};
    }

    const auto result =
        tickline::protocol::decode_command_envelope(bytes);

    expect(!result.has_value(), "zero entity identifier should fail");
    expect(
        result.error() == ParseErrorCode::invalid_entity_id,
        "zero entity identifier should have a stable parse error");
}

void test_authoritative_values_are_not_validated_by_codec()
{
    auto command = make_command();
    command.schema_version = 999;
    command.type = static_cast<CommandType>(999);
    command.client_id = ClientId{0};
    command.session_id = SessionId{0};
    command.sequence = 0;

    const auto decoded =
        tickline::protocol::decode_command_envelope(
            tickline::protocol::encode_command_envelope(command));

    expect(decoded.has_value(), "structurally valid claims should decode");
    expect(
        decoded.value() == command,
        "codec should preserve claims for authoritative validation");
}

void test_frame_error_is_propagated()
{
    auto frame =
        tickline::protocol::encode_command_frame(make_command());

    frame[0] = std::byte{0};

    const auto result =
        tickline::protocol::decode_command_frame(frame);

    expect(!result.has_value(), "malformed frame should not decode");
    expect(
        result.error() == ParseErrorCode::invalid_magic,
        "frame decoder should preserve the parser error");
}

}

int main()
{
    try {
        test_round_trip_preserves_every_field();
        test_signed_extremes_round_trip();
        test_big_endian_encoding();
        test_payload_size_is_exact();
        test_zero_entity_identifier_is_rejected();
        test_authoritative_values_are_not_validated_by_codec();
        test_frame_error_is_propagated();
    } catch (const std::exception& error) {
        std::cerr
            << "command envelope codec test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
