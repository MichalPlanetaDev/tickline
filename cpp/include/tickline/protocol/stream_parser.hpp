#pragma once

#include "tickline/protocol/frame_parser.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace tickline::protocol {

struct OwnedFrame final {
    FrameHeader header;
    std::vector<std::byte> payload;

    friend bool operator==(
        const OwnedFrame&,
        const OwnedFrame&) = default;
};

using StreamParseResult =
    std::expected<std::vector<OwnedFrame>, ParseErrorCode>;

using StreamFinishResult =
    std::expected<void, ParseErrorCode>;

class FrameStreamParser final {
public:
    explicit FrameStreamParser(ProtocolLimits limits = {});

    [[nodiscard]] StreamParseResult push(
        std::span<const std::byte> bytes);

    [[nodiscard]] StreamFinishResult finish();

    void reset() noexcept;

    [[nodiscard]] const ProtocolLimits& limits() const noexcept;

    [[nodiscard]] std::size_t buffered_size() const noexcept;

    [[nodiscard]] bool failed() const noexcept;

    [[nodiscard]] std::optional<ParseErrorCode>
    failure() const noexcept;

private:
    [[nodiscard]] StreamParseResult fail(ParseErrorCode code);

    [[nodiscard]] StreamFinishResult finish_with_error(
        ParseErrorCode code);

    ProtocolLimits limits_;
    std::vector<std::byte> buffer_;
    std::optional<FrameHeader> current_header_;
    std::optional<ParseErrorCode> failure_;
};

}
