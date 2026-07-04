#pragma once

#include "tickline/protocol/frame.hpp"

#include <cstddef>
#include <expected>
#include <span>

namespace tickline::protocol {

using FrameHeaderDecodeResult =
    std::expected<FrameHeader, ParseErrorCode>;

using FrameParseResult =
    std::expected<FrameView, ParseErrorCode>;

[[nodiscard]] FrameHeaderDecodeResult decode_frame_header(
    std::span<const std::byte> bytes,
    ProtocolLimits limits = {});

[[nodiscard]] FrameParseResult parse_frame(
    std::span<const std::byte> bytes,
    ProtocolLimits limits = {});

}
