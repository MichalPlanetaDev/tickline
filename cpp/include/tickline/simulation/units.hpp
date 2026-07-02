#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tickline::simulation {

class Microseconds final {
public:
    constexpr explicit Microseconds(const std::int64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] static constexpr Microseconds zero() noexcept
    {
        return Microseconds{0};
    }

    [[nodiscard]] static Microseconds from_milliseconds(
        const std::int64_t milliseconds)
    {
        constexpr std::int64_t scale = 1'000;
        constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
        constexpr auto minimum = std::numeric_limits<std::int64_t>::min();

        if (milliseconds > maximum / scale ||
            milliseconds < minimum / scale) {
            throw std::overflow_error{
                "millisecond value cannot be represented as microseconds"};
        }

        return Microseconds{milliseconds * scale};
    }

    [[nodiscard]] constexpr std::int64_t count() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const Microseconds&,
        const Microseconds&) noexcept = default;

private:
    std::int64_t value_;
};

class Millimeters final {
public:
    constexpr explicit Millimeters(const std::int64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] constexpr std::int64_t count() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const Millimeters&,
        const Millimeters&) noexcept = default;

private:
    std::int64_t value_;
};

class MillimetersPerSecond final {
public:
    constexpr explicit MillimetersPerSecond(
        const std::int64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] constexpr std::int64_t count() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const MillimetersPerSecond&,
        const MillimetersPerSecond&) noexcept = default;

private:
    std::int64_t value_;
};

class EntityId final {
public:
    explicit EntityId(const std::uint64_t value)
        : value_{value}
    {
        if (value_ == 0) {
            throw std::invalid_argument{
                "entity identifier must be non-zero"};
        }
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(
        const EntityId&,
        const EntityId&) noexcept = default;

    friend constexpr auto operator<=>(
        const EntityId&,
        const EntityId&) noexcept = default;

private:
    std::uint64_t value_;
};

struct Position2 final {
    Millimeters x;
    Millimeters y;

    friend constexpr bool operator==(
        const Position2&,
        const Position2&) noexcept = default;
};

struct Velocity2 final {
    MillimetersPerSecond x;
    MillimetersPerSecond y;

    friend constexpr bool operator==(
        const Velocity2&,
        const Velocity2&) noexcept = default;
};

}
