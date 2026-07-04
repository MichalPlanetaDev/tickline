#pragma once

#include "tickline/simulation/units.hpp"

#include <compare>
#include <cstdint>

namespace tickline::command {

inline constexpr std::uint16_t command_schema_version = 1;

enum class CommandType : std::uint16_t {
    set_velocity = 1,
};

class ClientId final {
public:
    constexpr explicit ClientId(const std::uint64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return value_ != 0;
    }

    friend constexpr bool operator==(
        const ClientId&,
        const ClientId&) noexcept = default;

    friend constexpr auto operator<=>(
        const ClientId&,
        const ClientId&) noexcept = default;

private:
    std::uint64_t value_;
};

class SessionId final {
public:
    constexpr explicit SessionId(const std::uint64_t value) noexcept
        : value_{value}
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return value_ != 0;
    }

    friend constexpr bool operator==(
        const SessionId&,
        const SessionId&) noexcept = default;

    friend constexpr auto operator<=>(
        const SessionId&,
        const SessionId&) noexcept = default;

private:
    std::uint64_t value_;
};

struct SetVelocityPayload final {
    simulation::EntityId entity_id;
    simulation::Velocity2 velocity;

    friend constexpr bool operator==(
        const SetVelocityPayload&,
        const SetVelocityPayload&) noexcept = default;
};

struct CommandEnvelope final {
    std::uint16_t schema_version{command_schema_version};
    CommandType type{CommandType::set_velocity};
    ClientId client_id;
    SessionId session_id;
    std::uint64_t sequence;
    std::uint64_t target_tick;
    SetVelocityPayload payload;

    friend constexpr bool operator==(
        const CommandEnvelope&,
        const CommandEnvelope&) noexcept = default;
};

}
