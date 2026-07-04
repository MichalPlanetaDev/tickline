#include "tickline/protocol/command_envelope_codec.hpp"
#include "tickline/protocol/frame_parser.hpp"
#include "tickline/protocol/stream_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    const std::size_t size)
{
    const auto bytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data),
        size,
    };

    static_cast<void>(
        tickline::protocol::decode_frame_header(bytes));

    static_cast<void>(
        tickline::protocol::parse_frame(bytes));

    static_cast<void>(
        tickline::protocol::decode_command_frame(bytes));

    tickline::protocol::FrameStreamParser stream;

    const auto split = size == 0
        ? std::size_t{0}
        : static_cast<std::size_t>(data[0]) % (size + 1);

    static_cast<void>(stream.push(bytes.first(split)));
    static_cast<void>(stream.push(bytes.subspan(split)));
    static_cast<void>(stream.finish());

    return 0;
}
