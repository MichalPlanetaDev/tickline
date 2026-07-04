#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tickline::protocol {

inline constexpr std::array<std::byte, 4> frame_magic{
    std::byte{0x54},
    std::byte{0x49},
    std::byte{0x43},
    std::byte{0x4b},
};

inline constexpr std::uint16_t protocol_version = 1;
inline constexpr std::uint16_t frame_header_size = 16;
inline constexpr std::uint32_t default_max_frame_size = 4'096;

struct ProtocolLimits final {
    std::uint32_t maximum_frame_size{default_max_frame_size};
};

enum class MessageType : std::uint16_t {
    command_envelope = 1,
};

enum class ParseErrorCode : std::uint16_t {
    none = 0,
    truncated_header = 1,
    invalid_magic = 2,
    unsupported_protocol_version = 3,
    invalid_header_size = 4,
    frame_too_small = 5,
    frame_too_large = 6,
    reserved_flags_set = 7,
    unknown_message_type = 8,
    truncated_frame = 9,
    trailing_data = 10,
    payload_size_mismatch = 11,
    invalid_entity_id = 12,
};

struct FrameHeader final {
    std::uint16_t version;
    std::uint16_t header_size;
    std::uint32_t frame_size;
    MessageType message_type;
    std::uint16_t flags;

    friend constexpr bool operator==(
        const FrameHeader&,
        const FrameHeader&) noexcept = default;
};

struct FrameView final {
    FrameHeader header;
    std::span<const std::byte> payload;
};

[[nodiscard]] constexpr std::string_view parse_error_code_name(
    const ParseErrorCode code) noexcept
{
    switch (code) {
    case ParseErrorCode::none:
        return "none";

    case ParseErrorCode::truncated_header:
        return "truncated_header";

    case ParseErrorCode::invalid_magic:
        return "invalid_magic";

    case ParseErrorCode::unsupported_protocol_version:
        return "unsupported_protocol_version";

    case ParseErrorCode::invalid_header_size:
        return "invalid_header_size";

    case ParseErrorCode::frame_too_small:
        return "frame_too_small";

    case ParseErrorCode::frame_too_large:
        return "frame_too_large";

    case ParseErrorCode::reserved_flags_set:
        return "reserved_flags_set";

    case ParseErrorCode::unknown_message_type:
        return "unknown_message_type";

    case ParseErrorCode::truncated_frame:
        return "truncated_frame";

    case ParseErrorCode::trailing_data:
        return "trailing_data";

    case ParseErrorCode::payload_size_mismatch:
        return "payload_size_mismatch";

    case ParseErrorCode::invalid_entity_id:
        return "invalid_entity_id";
    }

    return "unknown";
}

}
