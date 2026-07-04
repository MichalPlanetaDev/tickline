#include "tickline/protocol/command_envelope_codec.hpp"
#include "tickline/protocol/frame_parser.hpp"
#include "tickline/protocol/stream_parser.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using tickline::protocol::ParseErrorCode;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

[[nodiscard]] std::vector<std::byte> read_file(
    const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};

    if (!input) {
        throw std::runtime_error{
            "could not open corpus file: " + path.string()};
    }

    const std::vector<char> characters{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };

    std::vector<std::byte> bytes;
    bytes.reserve(characters.size());

    for (const auto character : characters) {
        bytes.push_back(
            std::byte{static_cast<unsigned char>(character)});
    }

    return bytes;
}

void test_corpus(const std::filesystem::path& root)
{
    const std::map<std::string, ParseErrorCode> frame_expectations{
        {"invalid-magic.bin", ParseErrorCode::invalid_magic},
        {"oversized-frame.bin", ParseErrorCode::frame_too_large},
        {"trailing-data.bin", ParseErrorCode::trailing_data},
        {"truncated-frame.bin", ParseErrorCode::truncated_frame},
        {"truncated-header.bin", ParseErrorCode::truncated_header},
    };

    std::size_t file_count = 0;

    for (const auto& entry : std::filesystem::directory_iterator{root}) {
        if (!entry.is_regular_file() || entry.path().extension() != ".bin") {
            continue;
        }

        ++file_count;

        const auto name = entry.path().filename().string();
        const auto bytes = read_file(entry.path());
        const auto frame_result = tickline::protocol::parse_frame(bytes);
        const auto command_result =
            tickline::protocol::decode_command_frame(bytes);

        tickline::protocol::FrameStreamParser stream;
        const auto stream_result = stream.push(bytes);

        expect(
            stream_result.has_value() || stream.failed(),
            "stream parser should return frames or enter failed state");

        const auto expected = frame_expectations.find(name);

        if (expected != frame_expectations.end()) {
            expect(!frame_result.has_value(), "malformed corpus frame should fail");
            expect(
                frame_result.error() == expected->second,
                "corpus frame should preserve expected parser error");
            continue;
        }

        if (name == "valid-command.bin") {
            expect(frame_result.has_value(), "valid corpus frame should parse");
            expect(command_result.has_value(), "valid corpus command should decode");
            continue;
        }

        if (name == "zero-entity.bin") {
            expect(frame_result.has_value(), "zero-entity frame is structurally framed");
            expect(!command_result.has_value(), "zero entity should fail command decoding");
            expect(
                command_result.error() == ParseErrorCode::invalid_entity_id,
                "zero entity corpus should preserve structural error");
            continue;
        }

        throw std::runtime_error{"unexpected corpus file: " + name};
    }

    expect(file_count == 7, "protocol regression corpus should contain seven seeds");
}

}

int main(const int argc, const char* const argv[])
{
    try {
        if (argc != 2) {
            throw std::runtime_error{
                "expected protocol corpus directory argument"};
        }

        test_corpus(std::filesystem::path{argv[1]});
    } catch (const std::exception& error) {
        std::cerr
            << "protocol fuzz corpus test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
