#include "tickline/protocol/command_envelope_codec.hpp"
#include "tickline/protocol/stream_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
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
using tickline::protocol::FrameStreamParser;
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

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence = 1)
{
    return CommandEnvelope{
        .schema_version = tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = sequence,
        .target_tick = sequence + 1,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{3},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{
                    static_cast<std::int64_t>(sequence * 10)},
                .y = MillimetersPerSecond{
                    -static_cast<std::int64_t>(sequence * 10)},
            },
        },
    };
}

void expect_decoded_command(
    const tickline::protocol::OwnedFrame& frame,
    const CommandEnvelope& expected)
{
    const auto decoded =
        tickline::protocol::decode_command_envelope(frame.payload);

    expect(decoded.has_value(), "stream frame payload should decode");
    expect(decoded.value() == expected, "stream frame should preserve command");
}

void test_every_split_position()
{
    const auto command = make_command();
    const auto frame =
        tickline::protocol::encode_command_frame(command);

    for (std::size_t split = 0; split <= frame.size(); ++split) {
        FrameStreamParser parser;

        const auto first = parser.push(
            std::span<const std::byte>{frame}.first(split));

        expect(first.has_value(), "first stream fragment should be accepted");

        const auto expected_first_count =
            split == frame.size() ? std::size_t{1} : std::size_t{0};

        expect(
            first->size() == expected_first_count,
            "first fragment should emit only when it is complete");

        const auto second = parser.push(
            std::span<const std::byte>{frame}.subspan(split));

        expect(second.has_value(), "second stream fragment should be accepted");

        const auto total_count = first->size() + second->size();
        expect(total_count == 1, "split frame should emit exactly once");

        if (!first->empty()) {
            expect_decoded_command(first->front(), command);
        } else {
            expect_decoded_command(second->front(), command);
        }
        expect(parser.buffered_size() == 0, "completed frame should clear buffer");
        expect(parser.finish().has_value(), "complete stream should finish cleanly");
    }
}

void test_byte_by_byte_delivery()
{
    const auto command = make_command();
    const auto frame =
        tickline::protocol::encode_command_frame(command);

    FrameStreamParser parser;
    std::vector<tickline::protocol::OwnedFrame> emitted;

    for (const auto value : frame) {
        const auto result = parser.push(
            std::span<const std::byte>{&value, 1});

        expect(result.has_value(), "single stream byte should be accepted");

        emitted.insert(
            emitted.end(),
            result->begin(),
            result->end());
    }

    expect(emitted.size() == 1, "byte stream should emit one frame");
    expect_decoded_command(emitted.front(), command);
    expect(parser.finish().has_value(), "byte stream should finish cleanly");
}

void test_multiple_frames_in_one_chunk()
{
    const auto first_command = make_command(1);
    const auto second_command = make_command(2);

    const auto first_frame =
        tickline::protocol::encode_command_frame(first_command);

    const auto second_frame =
        tickline::protocol::encode_command_frame(second_command);

    std::vector<std::byte> bytes;
    bytes.insert(bytes.end(), first_frame.begin(), first_frame.end());
    bytes.insert(bytes.end(), second_frame.begin(), second_frame.end());

    FrameStreamParser parser;
    const auto result = parser.push(bytes);

    expect(result.has_value(), "concatenated frames should parse");
    expect(result->size() == 2, "one chunk should emit both frames");
    expect_decoded_command(result->at(0), first_command);
    expect_decoded_command(result->at(1), second_command);
    expect(parser.finish().has_value(), "multi-frame stream should finish cleanly");
}

void test_partial_second_frame_is_retained()
{
    const auto first_command = make_command(1);
    const auto second_command = make_command(2);

    const auto first_frame =
        tickline::protocol::encode_command_frame(first_command);

    const auto second_frame =
        tickline::protocol::encode_command_frame(second_command);

    constexpr std::size_t second_prefix_size = 23;

    std::vector<std::byte> first_chunk;
    first_chunk.insert(
        first_chunk.end(),
        first_frame.begin(),
        first_frame.end());

    first_chunk.insert(
        first_chunk.end(),
        second_frame.begin(),
        second_frame.begin() +
            static_cast<std::ptrdiff_t>(second_prefix_size));

    FrameStreamParser parser;
    const auto first_result = parser.push(first_chunk);

    expect(first_result.has_value(), "mixed complete and partial chunk should parse");
    expect(first_result->size() == 1, "first chunk should emit only complete frame");
    expect_decoded_command(first_result->front(), first_command);
    expect(
        parser.buffered_size() == second_prefix_size,
        "partial second frame should remain buffered");

    const auto second_result = parser.push(
        std::span<const std::byte>{second_frame}.subspan(
            second_prefix_size));

    expect(second_result.has_value(), "second frame suffix should parse");
    expect(second_result->size() == 1, "suffix should complete second frame");
    expect_decoded_command(second_result->front(), second_command);
}

void test_invalid_header_is_fail_closed_until_reset()
{
    auto invalid =
        tickline::protocol::encode_command_frame(make_command());

    invalid[0] = std::byte{0};

    FrameStreamParser parser;
    const auto rejected = parser.push(invalid);

    expect(!rejected.has_value(), "invalid stream header should fail");
    expect(
        rejected.error() == ParseErrorCode::invalid_magic,
        "invalid stream header should preserve parser error");
    expect(parser.failed(), "stream parser should enter failed state");
    expect(
        parser.failure() == ParseErrorCode::invalid_magic,
        "failed parser should expose stable failure");

    const auto valid =
        tickline::protocol::encode_command_frame(make_command());

    const auto repeated = parser.push(valid);

    expect(!repeated.has_value(), "failed parser should remain fail-closed");
    expect(
        repeated.error() == ParseErrorCode::invalid_magic,
        "failed parser should repeat original error");

    parser.reset();

    expect(!parser.failed(), "reset should clear failed state");
    expect(parser.buffered_size() == 0, "reset should clear buffered bytes");

    const auto recovered = parser.push(valid);

    expect(recovered.has_value(), "reset parser should accept valid input");
    expect(recovered->size() == 1, "reset parser should emit valid frame");
}

void test_finish_classifies_incomplete_stream()
{
    FrameStreamParser empty_parser;
    expect(empty_parser.finish().has_value(), "empty stream should finish cleanly");

    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    FrameStreamParser header_parser;
    const auto header_push = header_parser.push(
        std::span<const std::byte>{frame}.first(8));

    expect(header_push.has_value(), "partial header should be buffered");

    const auto header_finish = header_parser.finish();

    expect(!header_finish.has_value(), "partial header should fail at EOF");
    expect(
        header_finish.error() == ParseErrorCode::truncated_header,
        "partial header EOF should use truncated-header code");

    FrameStreamParser frame_parser;
    const auto frame_push = frame_parser.push(
        std::span<const std::byte>{frame}.first(frame.size() - 1));

    expect(frame_push.has_value(), "partial frame should be buffered");

    const auto frame_finish = frame_parser.finish();

    expect(!frame_finish.has_value(), "partial frame should fail at EOF");
    expect(
        frame_finish.error() == ParseErrorCode::truncated_frame,
        "partial frame EOF should use truncated-frame code");
}

void test_configured_limit_applies_to_stream_headers()
{
    const auto frame =
        tickline::protocol::encode_command_frame(make_command());

    FrameStreamParser parser{
        ProtocolLimits{
            .maximum_frame_size =
                static_cast<std::uint32_t>(frame.size() - 1),
        }};

    const auto result = parser.push(frame);

    expect(!result.has_value(), "stream frame over configured limit should fail");
    expect(
        result.error() == ParseErrorCode::frame_too_large,
        "stream parser should preserve configured-limit error");
}

}

int main()
{
    try {
        test_every_split_position();
        test_byte_by_byte_delivery();
        test_multiple_frames_in_one_chunk();
        test_partial_second_frame_is_retained();
        test_invalid_header_is_fail_closed_until_reset();
        test_finish_classifies_incomplete_stream();
        test_configured_limit_applies_to_stream_headers();
    } catch (const std::exception& error) {
        std::cerr
            << "protocol stream parser test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
