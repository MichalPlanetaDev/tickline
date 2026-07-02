#include "tickline/simulation/state_fingerprint.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tickline::simulation {

std::string StateFingerprint::to_hex() const
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    std::string result(16, '0');
    auto remaining = value_;

    for (std::size_t offset = 0; offset < result.size(); ++offset) {
        const auto result_index = result.size() - 1 - offset;
        const auto digit_index =
            static_cast<std::size_t>(remaining & 0x0fU);

        result[result_index] = digits[digit_index];
        remaining >>= 4U;
    }

    return result;
}

StateFingerprint fingerprint_bytes(
    const std::span<const std::byte> bytes) noexcept
{
    constexpr std::uint64_t offset_basis =
        14'695'981'039'346'656'037ULL;

    constexpr std::uint64_t prime =
        1'099'511'628'211ULL;

    auto hash = offset_basis;

    for (const auto byte : bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= prime;
    }

    return StateFingerprint{hash};
}

}
