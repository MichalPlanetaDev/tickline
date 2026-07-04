#include "tickline/protocol/frame_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace tickline::protocol {
namespace {

[[nodiscard]] constexpr std::uint16_t read_u16(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(bytes[offset]) << 8U) |
        std::to_integer<std::uint16_t>(bytes[offset + 1]));
}

[[nodiscard]] constexpr std::uint32_t read_u32(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return
        (std::to_integer<std::uint32_t>(bytes[offset]) << 24U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 16U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 8U) |
        std::to_integer<std::uint32_t>(bytes[offset + 3]);
}

void validate_limits(const ProtocolLimits limits)
{
    if (limits.maximum_frame_size < frame_header_size) {
        throw std::invalid_argument{
            "maximum frame size must include the fixed frame header"};
    }
}

[[nodiscard]] bool has_valid_magic(
    const std::span<const std::byte> bytes) noexcept
{
    for (std::size_t index = 0; index < frame_magic.size(); ++index) {
        if (bytes[index] != frame_magic[index]) {
            return false;
        }
    }

    return true;
}

}

FrameHeaderDecodeResult decode_frame_header(
    const std::span<const std::byte> bytes,
    const ProtocolLimits limits)
{
    validate_limits(limits);

    if (bytes.size() < frame_header_size) {
        return std::unexpected{ParseErrorCode::truncated_header};
    }

    if (!has_valid_magic(bytes)) {
        return std::unexpected{ParseErrorCode::invalid_magic};
    }

    const auto version = read_u16(bytes, 4);

    if (version != protocol_version) {
        return std::unexpected{
            ParseErrorCode::unsupported_protocol_version};
    }

    const auto declared_header_size = read_u16(bytes, 6);

    if (declared_header_size != frame_header_size) {
        return std::unexpected{ParseErrorCode::invalid_header_size};
    }

    const auto declared_frame_size = read_u32(bytes, 8);

    if (declared_frame_size < frame_header_size) {
        return std::unexpected{ParseErrorCode::frame_too_small};
    }

    if (declared_frame_size > limits.maximum_frame_size) {
        return std::unexpected{ParseErrorCode::frame_too_large};
    }

    const auto message_type_value = read_u16(bytes, 12);
    const auto flags = read_u16(bytes, 14);

    if (flags != 0) {
        return std::unexpected{ParseErrorCode::reserved_flags_set};
    }

    if (message_type_value !=
        static_cast<std::uint16_t>(MessageType::command_envelope)) {
        return std::unexpected{ParseErrorCode::unknown_message_type};
    }

    return FrameHeader{
        .version = version,
        .header_size = declared_header_size,
        .frame_size = declared_frame_size,
        .message_type =
            static_cast<MessageType>(message_type_value),
        .flags = flags,
    };
}

FrameParseResult parse_frame(
    const std::span<const std::byte> bytes,
    const ProtocolLimits limits)
{
    const auto header_result = decode_frame_header(bytes, limits);

    if (!header_result.has_value()) {
        return std::unexpected{header_result.error()};
    }

    const auto& header = header_result.value();
    const auto declared_size =
        static_cast<std::size_t>(header.frame_size);

    if (bytes.size() < declared_size) {
        return std::unexpected{ParseErrorCode::truncated_frame};
    }

    if (bytes.size() > declared_size) {
        return std::unexpected{ParseErrorCode::trailing_data};
    }

    return FrameView{
        .header = header,
        .payload = bytes.subspan(frame_header_size),
    };
}

}
