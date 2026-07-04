#include "tickline/protocol/command_envelope_codec.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace tickline::protocol {
namespace {

void write_u16(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint16_t value) noexcept
{
    bytes[offset] =
        std::byte{static_cast<std::uint8_t>(value >> 8U)};

    bytes[offset + 1] =
        std::byte{static_cast<std::uint8_t>(value)};
}

void write_u32(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint32_t value) noexcept
{
    bytes[offset] =
        std::byte{static_cast<std::uint8_t>(value >> 24U)};

    bytes[offset + 1] =
        std::byte{static_cast<std::uint8_t>(value >> 16U)};

    bytes[offset + 2] =
        std::byte{static_cast<std::uint8_t>(value >> 8U)};

    bytes[offset + 3] =
        std::byte{static_cast<std::uint8_t>(value)};
}

void write_u64(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint64_t value) noexcept
{
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift =
            static_cast<unsigned>((7U - index) * 8U);

        bytes[offset + index] = std::byte{
            static_cast<std::uint8_t>(value >> shift)};
    }
}

[[nodiscard]] std::uint16_t read_u16(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(bytes[offset]) << 8U) |
        std::to_integer<std::uint16_t>(bytes[offset + 1]));
}

[[nodiscard]] std::uint64_t read_u64(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    std::uint64_t value = 0;

    for (std::size_t index = 0; index < 8; ++index) {
        value =
            (value << 8U) |
            std::to_integer<std::uint64_t>(bytes[offset + index]);
    }

    return value;
}

void encode_payload(
    const command::CommandEnvelope& envelope,
    const std::span<std::byte> bytes) noexcept
{
    write_u16(bytes, 0, envelope.schema_version);

    write_u16(
        bytes,
        2,
        static_cast<std::uint16_t>(envelope.type));

    write_u64(bytes, 4, envelope.client_id.value());
    write_u64(bytes, 12, envelope.session_id.value());
    write_u64(bytes, 20, envelope.sequence);
    write_u64(bytes, 28, envelope.target_tick);
    write_u64(bytes, 36, envelope.payload.entity_id.value());

    write_u64(
        bytes,
        44,
        std::bit_cast<std::uint64_t>(
            envelope.payload.velocity.x.count()));

    write_u64(
        bytes,
        52,
        std::bit_cast<std::uint64_t>(
            envelope.payload.velocity.y.count()));
}

}

CommandEnvelopeBytes encode_command_envelope(
    const command::CommandEnvelope& envelope) noexcept
{
    CommandEnvelopeBytes bytes{};
    encode_payload(envelope, bytes);
    return bytes;
}

CommandFrameBytes encode_command_frame(
    const command::CommandEnvelope& envelope) noexcept
{
    static_assert(command_frame_size <= default_max_frame_size);

    CommandFrameBytes bytes{};
    const std::span<std::byte> output{bytes};

    for (std::size_t index = 0; index < frame_magic.size(); ++index) {
        output[index] = frame_magic[index];
    }

    write_u16(output, 4, protocol_version);
    write_u16(output, 6, frame_header_size);

    write_u32(
        output,
        8,
        static_cast<std::uint32_t>(command_frame_size));

    write_u16(
        output,
        12,
        static_cast<std::uint16_t>(MessageType::command_envelope));

    write_u16(output, 14, 0);

    encode_payload(
        envelope,
        output.subspan(frame_header_size));

    return bytes;
}

CommandEnvelopeDecodeResult decode_command_envelope(
    const std::span<const std::byte> payload)
{
    if (payload.size() != command_envelope_payload_size) {
        return std::unexpected{
            ParseErrorCode::payload_size_mismatch};
    }

    const auto entity_id = read_u64(payload, 36);

    if (entity_id == 0) {
        return std::unexpected{ParseErrorCode::invalid_entity_id};
    }

    return command::CommandEnvelope{
        .schema_version = read_u16(payload, 0),
        .type = static_cast<command::CommandType>(
            read_u16(payload, 2)),
        .client_id = command::ClientId{read_u64(payload, 4)},
        .session_id = command::SessionId{read_u64(payload, 12)},
        .sequence = read_u64(payload, 20),
        .target_tick = read_u64(payload, 28),
        .payload = command::SetVelocityPayload{
            .entity_id = simulation::EntityId{entity_id},
            .velocity = simulation::Velocity2{
                .x = simulation::MillimetersPerSecond{
                    std::bit_cast<std::int64_t>(
                        read_u64(payload, 44))},
                .y = simulation::MillimetersPerSecond{
                    std::bit_cast<std::int64_t>(
                        read_u64(payload, 52))},
            },
        },
    };
}

CommandEnvelopeDecodeResult decode_command_frame(
    const std::span<const std::byte> bytes,
    const ProtocolLimits limits)
{
    const auto frame_result = parse_frame(bytes, limits);

    if (!frame_result.has_value()) {
        return std::unexpected{frame_result.error()};
    }

    return decode_command_envelope(
        frame_result->payload);
}

}
