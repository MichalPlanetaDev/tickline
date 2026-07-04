#pragma once

#include "tickline/command/command_envelope.hpp"
#include "tickline/protocol/frame_parser.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <span>

namespace tickline::protocol {

inline constexpr std::size_t command_envelope_payload_size = 60;
inline constexpr std::size_t command_frame_size =
    frame_header_size + command_envelope_payload_size;

using CommandEnvelopeBytes =
    std::array<std::byte, command_envelope_payload_size>;

using CommandFrameBytes =
    std::array<std::byte, command_frame_size>;

using CommandEnvelopeDecodeResult =
    std::expected<command::CommandEnvelope, ParseErrorCode>;

[[nodiscard]] CommandEnvelopeBytes encode_command_envelope(
    const command::CommandEnvelope& envelope) noexcept;

[[nodiscard]] CommandFrameBytes encode_command_frame(
    const command::CommandEnvelope& envelope) noexcept;

[[nodiscard]] CommandEnvelopeDecodeResult decode_command_envelope(
    std::span<const std::byte> payload);

[[nodiscard]] CommandEnvelopeDecodeResult decode_command_frame(
    std::span<const std::byte> bytes,
    ProtocolLimits limits = {});

}
