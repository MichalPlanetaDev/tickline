#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>

namespace tickline::security {

class Sha256Digest final {
public:
    using Storage = std::array<std::byte, 32>;

    constexpr Sha256Digest() noexcept = default;

    constexpr explicit Sha256Digest(
        const Storage bytes) noexcept
        : bytes_{bytes}
    {
    }

    [[nodiscard]] constexpr const Storage&
    bytes() const noexcept
    {
        return bytes_;
    }

    [[nodiscard]] std::string to_hex() const;

    friend constexpr bool operator==(
        const Sha256Digest&,
        const Sha256Digest&) noexcept = default;

private:
    Storage bytes_{};
};

[[nodiscard]] Sha256Digest sha256(
    std::span<const std::byte> bytes) noexcept;

}
