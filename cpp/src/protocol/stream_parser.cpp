#include "tickline/protocol/stream_parser.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tickline::protocol {

FrameStreamParser::FrameStreamParser(const ProtocolLimits limits)
    : limits_{limits}
{
    if (limits_.maximum_frame_size < frame_header_size) {
        throw std::invalid_argument{
            "maximum frame size must include the fixed frame header"};
    }

    buffer_.reserve(limits_.maximum_frame_size);
}

StreamParseResult FrameStreamParser::push(
    const std::span<const std::byte> bytes)
{
    if (failure_.has_value()) {
        return std::unexpected{failure_.value()};
    }

    std::vector<OwnedFrame> frames;
    std::size_t offset = 0;

    while (offset < bytes.size() || current_header_.has_value()) {
        if (!current_header_.has_value()) {
            const auto required =
                static_cast<std::size_t>(frame_header_size) -
                buffer_.size();

            const auto available = bytes.size() - offset;
            const auto count = std::min(required, available);

            buffer_.insert(
                buffer_.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));

            offset += count;

            if (buffer_.size() < frame_header_size) {
                break;
            }

            const auto header_result =
                decode_frame_header(buffer_, limits_);

            if (!header_result.has_value()) {
                return fail(header_result.error());
            }

            current_header_ = header_result.value();
        }

        const auto expected_size = static_cast<std::size_t>(
            current_header_->frame_size);

        const auto required = expected_size - buffer_.size();
        const auto available = bytes.size() - offset;
        const auto count = std::min(required, available);

        buffer_.insert(
            buffer_.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));

        offset += count;

        if (buffer_.size() < expected_size) {
            break;
        }

        const auto parsed = parse_frame(buffer_, limits_);

        if (!parsed.has_value()) {
            return fail(parsed.error());
        }

        frames.push_back(
            OwnedFrame{
                .header = parsed->header,
                .payload = std::vector<std::byte>{
                    parsed->payload.begin(),
                    parsed->payload.end(),
                },
            });

        buffer_.clear();
        current_header_.reset();
    }

    return frames;
}

StreamFinishResult FrameStreamParser::finish()
{
    if (failure_.has_value()) {
        return std::unexpected{failure_.value()};
    }

    if (buffer_.empty()) {
        return {};
    }

    if (buffer_.size() < frame_header_size) {
        return finish_with_error(ParseErrorCode::truncated_header);
    }

    return finish_with_error(ParseErrorCode::truncated_frame);
}

void FrameStreamParser::reset() noexcept
{
    buffer_.clear();
    current_header_.reset();
    failure_.reset();
}

const ProtocolLimits& FrameStreamParser::limits() const noexcept
{
    return limits_;
}

std::size_t FrameStreamParser::buffered_size() const noexcept
{
    return buffer_.size();
}

bool FrameStreamParser::failed() const noexcept
{
    return failure_.has_value();
}

std::optional<ParseErrorCode>
FrameStreamParser::failure() const noexcept
{
    return failure_;
}

StreamParseResult FrameStreamParser::fail(
    const ParseErrorCode code)
{
    failure_ = code;
    return std::unexpected{code};
}

StreamFinishResult FrameStreamParser::finish_with_error(
    const ParseErrorCode code)
{
    failure_ = code;
    return std::unexpected{code};
}

}
