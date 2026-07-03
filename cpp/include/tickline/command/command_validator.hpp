#pragma once

#include "tickline/command/command_envelope.hpp"

#include <cstdint>
#include <string_view>

namespace tickline::command {

enum class CommandRejectionCode : std::uint16_t {
    none = 0,
    unsupported_schema_version = 1,
    unsupported_command_type = 2,
    invalid_client_id = 3,
    invalid_session_id = 4,
    client_identity_mismatch = 5,
    session_identity_mismatch = 6,
    sequence_zero = 7,
    stale_target_tick = 8,
    target_tick_too_far_future = 9,
    velocity_out_of_range = 10,
};

struct CommandValidationPolicy final {
    std::uint64_t maximum_future_ticks{8};
    simulation::MillimetersPerSecond max_abs_velocity{1'000'000};
};

struct CommandValidationContext final {
    ClientId authenticated_client_id;
    SessionId active_session_id;
    std::uint64_t current_tick;
};

struct CommandValidationResult final {
    CommandRejectionCode code;

    [[nodiscard]] constexpr bool accepted() const noexcept
    {
        return code == CommandRejectionCode::none;
    }

    friend constexpr bool operator==(
        const CommandValidationResult&,
        const CommandValidationResult&) noexcept = default;
};

class CommandValidator final {
public:
    explicit CommandValidator(
        CommandValidationPolicy policy = {});

    [[nodiscard]] const CommandValidationPolicy& policy() const noexcept;

    [[nodiscard]] CommandValidationResult validate(
        const CommandEnvelope& command,
        const CommandValidationContext& context) const;

private:
    [[nodiscard]] bool velocity_is_valid(
        const simulation::Velocity2& velocity) const noexcept;

    CommandValidationPolicy policy_;
};

[[nodiscard]] constexpr std::string_view command_rejection_code_name(
    const CommandRejectionCode code) noexcept
{
    switch (code) {
    case CommandRejectionCode::none:
        return "none";

    case CommandRejectionCode::unsupported_schema_version:
        return "unsupported_schema_version";

    case CommandRejectionCode::unsupported_command_type:
        return "unsupported_command_type";

    case CommandRejectionCode::invalid_client_id:
        return "invalid_client_id";

    case CommandRejectionCode::invalid_session_id:
        return "invalid_session_id";

    case CommandRejectionCode::client_identity_mismatch:
        return "client_identity_mismatch";

    case CommandRejectionCode::session_identity_mismatch:
        return "session_identity_mismatch";

    case CommandRejectionCode::sequence_zero:
        return "sequence_zero";

    case CommandRejectionCode::stale_target_tick:
        return "stale_target_tick";

    case CommandRejectionCode::target_tick_too_far_future:
        return "target_tick_too_far_future";

    case CommandRejectionCode::velocity_out_of_range:
        return "velocity_out_of_range";
    }

    return "unknown";
}

}
