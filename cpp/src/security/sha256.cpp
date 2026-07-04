#include "tickline/security/sha256.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace tickline::security {
namespace {

constexpr std::size_t block_size = 64;

using HashState = std::array<std::uint32_t, 8>;
using MessageSchedule = std::array<std::uint32_t, 64>;

constexpr std::array<std::uint32_t, 64> round_constants{
    0x428a2f98U,
    0x71374491U,
    0xb5c0fbcfU,
    0xe9b5dba5U,
    0x3956c25bU,
    0x59f111f1U,
    0x923f82a4U,
    0xab1c5ed5U,
    0xd807aa98U,
    0x12835b01U,
    0x243185beU,
    0x550c7dc3U,
    0x72be5d74U,
    0x80deb1feU,
    0x9bdc06a7U,
    0xc19bf174U,
    0xe49b69c1U,
    0xefbe4786U,
    0x0fc19dc6U,
    0x240ca1ccU,
    0x2de92c6fU,
    0x4a7484aaU,
    0x5cb0a9dcU,
    0x76f988daU,
    0x983e5152U,
    0xa831c66dU,
    0xb00327c8U,
    0xbf597fc7U,
    0xc6e00bf3U,
    0xd5a79147U,
    0x06ca6351U,
    0x14292967U,
    0x27b70a85U,
    0x2e1b2138U,
    0x4d2c6dfcU,
    0x53380d13U,
    0x650a7354U,
    0x766a0abbU,
    0x81c2c92eU,
    0x92722c85U,
    0xa2bfe8a1U,
    0xa81a664bU,
    0xc24b8b70U,
    0xc76c51a3U,
    0xd192e819U,
    0xd6990624U,
    0xf40e3585U,
    0x106aa070U,
    0x19a4c116U,
    0x1e376c08U,
    0x2748774cU,
    0x34b0bcb5U,
    0x391c0cb3U,
    0x4ed8aa4aU,
    0x5b9cca4fU,
    0x682e6ff3U,
    0x748f82eeU,
    0x78a5636fU,
    0x84c87814U,
    0x8cc70208U,
    0x90befffaU,
    0xa4506cebU,
    0xbef9a3f7U,
    0xc67178f2U,
};

[[nodiscard]] constexpr std::uint32_t choose(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t z) noexcept
{
    return (x & y) ^ (~x & z);
}

[[nodiscard]] constexpr std::uint32_t majority(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t z) noexcept
{
    return (x & y) ^ (x & z) ^ (y & z);
}

[[nodiscard]] constexpr std::uint32_t large_sigma_zero(
    const std::uint32_t value) noexcept
{
    return std::rotr(value, 2) ^
           std::rotr(value, 13) ^
           std::rotr(value, 22);
}

[[nodiscard]] constexpr std::uint32_t large_sigma_one(
    const std::uint32_t value) noexcept
{
    return std::rotr(value, 6) ^
           std::rotr(value, 11) ^
           std::rotr(value, 25);
}

[[nodiscard]] constexpr std::uint32_t small_sigma_zero(
    const std::uint32_t value) noexcept
{
    return std::rotr(value, 7) ^
           std::rotr(value, 18) ^
           (value >> 3U);
}

[[nodiscard]] constexpr std::uint32_t small_sigma_one(
    const std::uint32_t value) noexcept
{
    return std::rotr(value, 17) ^
           std::rotr(value, 19) ^
           (value >> 10U);
}

[[nodiscard]] std::uint32_t read_word(
    const std::span<const std::byte, block_size> block,
    const std::size_t offset) noexcept
{
    return
        (std::to_integer<std::uint32_t>(
             block[offset]) << 24U) |
        (std::to_integer<std::uint32_t>(
             block[offset + 1]) << 16U) |
        (std::to_integer<std::uint32_t>(
             block[offset + 2]) << 8U) |
        std::to_integer<std::uint32_t>(
            block[offset + 3]);
}

void process_block(
    const std::span<const std::byte, block_size> block,
    HashState& state) noexcept
{
    MessageSchedule schedule{};

    for (std::size_t index = 0;
         index < 16;
         ++index) {
        schedule[index] =
            read_word(block, index * 4);
    }

    for (std::size_t index = 16;
         index < schedule.size();
         ++index) {
        schedule[index] =
            schedule[index - 16] +
            small_sigma_zero(
                schedule[index - 15]) +
            schedule[index - 7] +
            small_sigma_one(
                schedule[index - 2]);
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    auto f = state[5];
    auto g = state[6];
    auto h = state[7];

    for (std::size_t index = 0;
         index < schedule.size();
         ++index) {
        const auto first =
            h +
            large_sigma_one(e) +
            choose(e, f, g) +
            round_constants[index] +
            schedule[index];

        const auto second =
            large_sigma_zero(a) +
            majority(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void write_word(
    Sha256Digest::Storage& output,
    const std::size_t offset,
    const std::uint32_t value) noexcept
{
    output[offset] =
        static_cast<std::byte>(
            (value >> 24U) & 0xffU);

    output[offset + 1] =
        static_cast<std::byte>(
            (value >> 16U) & 0xffU);

    output[offset + 2] =
        static_cast<std::byte>(
            (value >> 8U) & 0xffU);

    output[offset + 3] =
        static_cast<std::byte>(
            value & 0xffU);
}

}

std::string Sha256Digest::to_hex() const
{
    constexpr std::array<char, 16> digits{
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
    };

    std::string result(bytes_.size() * 2, '0');

    for (std::size_t index = 0;
         index < bytes_.size();
         ++index) {
        const auto value =
            std::to_integer<std::uint8_t>(
                bytes_[index]);

        result[index * 2] =
            digits[
                static_cast<std::size_t>(
                    value >> 4U)];

        result[index * 2 + 1] =
            digits[
                static_cast<std::size_t>(
                    value & 0x0fU)];
    }

    return result;
}

Sha256Digest sha256(
    const std::span<const std::byte> bytes) noexcept
{
    static_assert(
        sizeof(std::size_t) <=
        sizeof(std::uint64_t));

    HashState state{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
    };

    const auto complete_blocks =
        bytes.size() / block_size;

    for (std::size_t block_index = 0;
         block_index < complete_blocks;
         ++block_index) {
        const auto offset =
            block_index * block_size;

        const std::span<
            const std::byte,
            block_size> block{
                bytes.data() + offset,
                block_size,
            };

        process_block(block, state);
    }

    std::array<std::byte, block_size * 2> tail{};

    const auto consumed =
        complete_blocks * block_size;

    const auto remainder =
        bytes.size() - consumed;

    for (std::size_t index = 0;
         index < remainder;
         ++index) {
        tail[index] =
            bytes[consumed + index];
    }

    tail[remainder] =
        static_cast<std::byte>(0x80U);

    const auto final_size =
        remainder < 56
            ? block_size
            : block_size * 2;

    const auto bit_length =
        static_cast<std::uint64_t>(
            bytes.size()) *
        8U;

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

    for (std::size_t index = 0;
         index < shifts.size();
         ++index) {
        tail[final_size - shifts.size() + index] =
            static_cast<std::byte>(
                (bit_length >> shifts[index]) &
                0xffU);
    }

    process_block(
        std::span<const std::byte, block_size>{
            tail.data(),
            block_size,
        },
        state);

    if (final_size == block_size * 2) {
        process_block(
            std::span<const std::byte, block_size>{
                tail.data() + block_size,
                block_size,
            },
            state);
    }

    Sha256Digest::Storage digest{};

    for (std::size_t index = 0;
         index < state.size();
         ++index) {
        write_word(
            digest,
            index * 4,
            state[index]);
    }

    return Sha256Digest{digest};
}

}
