#include "tickline/command/command_evidence_archive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tickline::command {
namespace {

constexpr std::array<std::byte, 4> archive_magic{
    std::byte{'T'},
    std::byte{'L'},
    std::byte{'C'},
    std::byte{'A'},
};

class ArchiveWriter final {
public:
    explicit ArchiveWriter(
        std::vector<std::byte>& output)
        : output_{output}
    {
    }

    void append_byte(const std::byte value)
    {
        output_.push_back(value);
    }

    void append_u16(const std::uint16_t value)
    {
        append_byte(
            static_cast<std::byte>(
                (value >> 8U) & 0xffU));

        append_byte(
            static_cast<std::byte>(
                value & 0xffU));
    }

    void append_u32(const std::uint32_t value)
    {
        constexpr std::array<unsigned int, 4> shifts{
            24U,
            16U,
            8U,
            0U,
        };

        for (const auto shift : shifts) {
            append_byte(
                static_cast<std::byte>(
                    (value >> shift) &
                    0xffU));
        }
    }

    void append_u64(const std::uint64_t value)
    {
        constexpr std::array<unsigned int, 8> shifts{
            56U,
            48U,
            40U,
            32U,
            24U,
            16U,
            8U,
            0U,
        };

        for (const auto shift : shifts) {
            append_byte(
                static_cast<std::byte>(
                    (value >> shift) &
                    0xffU));
        }
    }

    void append_digest(
        const security::Sha256Digest& digest)
    {
        output_.insert(
            output_.end(),
            digest.bytes().begin(),
            digest.bytes().end());
    }

private:
    std::vector<std::byte>& output_;
};

class ArchiveReader final {
public:
    explicit ArchiveReader(
        const std::span<const std::byte> input) noexcept
        : input_{input}
    {
    }

    [[nodiscard]] std::byte read_byte() noexcept
    {
        const auto value = input_[offset_];
        ++offset_;

        return value;
    }

    [[nodiscard]] std::uint16_t read_u16() noexcept
    {
        const auto first =
            std::to_integer<std::uint16_t>(
                read_byte());

        const auto second =
            std::to_integer<std::uint16_t>(
                read_byte());

        return static_cast<std::uint16_t>(
            (first << 8U) | second);
    }

    [[nodiscard]] std::uint32_t read_u32() noexcept
    {
        std::uint32_t value = 0;

        for (int index = 0; index < 4; ++index) {
            value =
                (value << 8U) |
                std::to_integer<std::uint32_t>(
                    read_byte());
        }

        return value;
    }

    [[nodiscard]] std::uint64_t read_u64() noexcept
    {
        std::uint64_t value = 0;

        for (int index = 0; index < 8; ++index) {
            value =
                (value << 8U) |
                std::to_integer<std::uint64_t>(
                    read_byte());
        }

        return value;
    }

    [[nodiscard]] security::Sha256Digest
    read_digest() noexcept
    {
        security::Sha256Digest::Storage bytes{};

        for (auto& byte : bytes) {
            byte = read_byte();
        }

        return security::Sha256Digest{bytes};
    }

private:
    std::span<const std::byte> input_;
    std::size_t offset_{0};
};

[[noreturn]] void throw_archive_error(
    const CommandEvidenceArchiveErrorCode code,
    const std::string& message)
{
    throw CommandEvidenceArchiveError{
        code,
        message,
    };
}

[[nodiscard]] std::size_t checked_archive_size(
    const std::uint64_t record_count)
{
    if (!std::in_range<std::size_t>(
            record_count)) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                record_count_overflow,
            "evidence archive record count cannot be represented");
    }

    const auto count =
        static_cast<std::size_t>(
            record_count);

    constexpr auto maximum =
        std::numeric_limits<std::size_t>::max();

    if (count >
        (maximum -
         command_evidence_archive_header_size) /
            command_evidence_encoded_size) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                record_count_overflow,
            "evidence archive encoded size overflows");
    }

    return
        command_evidence_archive_header_size +
        count * command_evidence_encoded_size;
}

}

CommandEvidenceArchiveError::
CommandEvidenceArchiveError(
    const CommandEvidenceArchiveErrorCode code,
    std::string message)
    : std::runtime_error{std::move(message)},
      code_{code}
{
}

CommandEvidenceArchiveErrorCode
CommandEvidenceArchiveError::code() const noexcept
{
    return code_;
}

std::vector<std::byte>
encode_command_evidence_archive(
    const std::span<
        const CommandEvidenceRecord> records,
    const security::Sha256Digest head_digest)
{
    if (!verify_command_evidence_chain(
            records,
            head_digest)) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                chain_verification_failed,
            "command evidence records do not match the supplied chain head");
    }

    if (!std::in_range<std::uint64_t>(
            records.size())) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                record_count_overflow,
            "command evidence record count cannot be encoded");
    }

    const auto encoded_size =
        checked_archive_size(
            static_cast<std::uint64_t>(
                records.size()));

    std::vector<std::byte> output;
    output.reserve(encoded_size);

    ArchiveWriter writer{output};

    for (const auto byte : archive_magic) {
        writer.append_byte(byte);
    }

    writer.append_u16(
        command_evidence_archive_schema_version);

    writer.append_u16(
        command_evidence_schema_version);

    writer.append_u32(
        static_cast<std::uint32_t>(
            command_evidence_encoded_size));

    writer.append_u32(0);

    writer.append_u64(
        static_cast<std::uint64_t>(
            records.size()));

    writer.append_digest(head_digest);

    for (const auto& record : records) {
        const auto encoded_record =
            encode_command_evidence(record);

        output.insert(
            output.end(),
            encoded_record.begin(),
            encoded_record.end());
    }

    if (output.size() != encoded_size) {
        throw std::logic_error{
            "command evidence archive encoded size is inconsistent"};
    }

    return output;
}

CommandEvidenceArchive
decode_command_evidence_archive(
    const std::span<const std::byte> encoded,
    const security::Sha256Digest trusted_head)
{
    if (encoded.size() <
        command_evidence_archive_header_size) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                truncated_header,
            "command evidence archive header is truncated");
    }

    ArchiveReader reader{
        encoded.first(
            command_evidence_archive_header_size)};

    for (const auto expected : archive_magic) {
        if (reader.read_byte() != expected) {
            throw_archive_error(
                CommandEvidenceArchiveErrorCode::
                    invalid_magic,
                "command evidence archive has an invalid magic value");
        }
    }

    const auto archive_schema =
        reader.read_u16();

    if (archive_schema !=
        command_evidence_archive_schema_version) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                unsupported_archive_schema,
            "command evidence archive schema is unsupported");
    }

    const auto record_schema =
        reader.read_u16();

    if (record_schema !=
        command_evidence_schema_version) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                unsupported_record_schema,
            "command evidence record schema is unsupported");
    }

    const auto record_size =
        reader.read_u32();

    if (record_size !=
        command_evidence_encoded_size) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                invalid_record_size,
            "command evidence archive declares an invalid record size");
    }

    if (reader.read_u32() != 0) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                nonzero_reserved_field,
            "command evidence archive reserved field must be zero");
    }

    const auto record_count =
        reader.read_u64();

    const auto declared_head =
        reader.read_digest();

    const auto expected_size =
        checked_archive_size(record_count);

    if (encoded.size() < expected_size) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                truncated_records,
            "command evidence archive records are truncated");
    }

    if (encoded.size() > expected_size) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                trailing_data,
            "command evidence archive contains trailing data");
    }

    const auto count =
        static_cast<std::size_t>(
            record_count);

    std::vector<CommandEvidenceRecord> records;
    records.reserve(count);

    auto offset =
        command_evidence_archive_header_size;

    for (std::size_t index = 0;
         index < count;
         ++index) {
        const auto record_bytes =
            encoded.subspan(
                offset,
                command_evidence_encoded_size);

        records.push_back(
            decode_command_evidence(
                record_bytes));

        offset +=
            command_evidence_encoded_size;
    }

    if (!verify_command_evidence_chain(
            records,
            declared_head)) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                chain_verification_failed,
            "command evidence archive chain verification failed");
    }

    if (declared_head != trusted_head) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                trusted_head_mismatch,
            "command evidence archive does not match the trusted chain head");
    }

    return CommandEvidenceArchive{
        .records = std::move(records),
        .head_digest = declared_head,
    };
}

void write_command_evidence_archive(
    const std::filesystem::path& path,
    const std::span<
        const CommandEvidenceRecord> records,
    const security::Sha256Digest head_digest)
{
    const auto encoded =
        encode_command_evidence_archive(
            records,
            head_digest);

    std::ofstream output{
        path,
        std::ios::binary |
            std::ios::trunc,
    };

    if (!output.is_open()) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive could not be opened for writing");
    }

    output.write(
        reinterpret_cast<const char*>(
            encoded.data()),
        static_cast<std::streamsize>(
            encoded.size()));

    output.flush();

    if (!output) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive could not be written");
    }

    output.close();

    if (!output) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive could not be closed");
    }
}

void write_command_evidence_archive(
    const std::filesystem::path& path,
    const CommandEvidenceLog& evidence)
{
    write_command_evidence_archive(
        path,
        evidence.records(),
        evidence.head_digest());
}

CommandEvidenceArchive
read_command_evidence_archive(
    const std::filesystem::path& path,
    const security::Sha256Digest trusted_head)
{
    std::ifstream input{
        path,
        std::ios::binary |
            std::ios::ate,
    };

    if (!input.is_open()) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive could not be opened for reading");
    }

    const auto end_position =
        input.tellg();

    if (end_position < 0) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive size could not be determined");
    }

    const auto file_size =
        static_cast<std::uintmax_t>(
            static_cast<std::streamoff>(
                end_position));

    if (file_size >
            std::numeric_limits<std::size_t>::max() ||
        file_size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<
                    std::streamsize>::max())) {
        throw_archive_error(
            CommandEvidenceArchiveErrorCode::
                io_error,
            "command evidence archive is too large to read");
    }

    std::vector<std::byte> encoded(
        static_cast<std::size_t>(
            file_size));

    input.seekg(0, std::ios::beg);

    if (!encoded.empty()) {
        input.read(
            reinterpret_cast<char*>(
                encoded.data()),
            static_cast<std::streamsize>(
                encoded.size()));

        if (input.gcount() !=
            static_cast<std::streamsize>(
                encoded.size())) {
            throw_archive_error(
                CommandEvidenceArchiveErrorCode::
                    io_error,
                "command evidence archive could not be read completely");
        }
    }

    return decode_command_evidence_archive(
        encoded,
        trusted_head);
}

}
