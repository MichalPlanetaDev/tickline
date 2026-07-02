#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace tickline::simulation {

class StateFingerprint final {
public:
    constexpr explicit StateFingerprint(const std::uint64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] std::string to_hex() const;

    friend constexpr bool operator==(
        const StateFingerprint&,
        const StateFingerprint&) noexcept = default;

private:
    std::uint64_t value_;
};

[[nodiscard]] StateFingerprint fingerprint_bytes(
    std::span<const std::byte> bytes) noexcept;

}
