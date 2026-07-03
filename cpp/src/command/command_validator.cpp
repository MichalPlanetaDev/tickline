#include "tickline/command/command_validator.hpp"

#include <cstdint>
#include <stdexcept>

namespace tickline::command {
namespace {

[[nodiscard]] constexpr CommandValidationResult reject(
    const CommandRejectionCode code) noexcept
{
    return CommandValidationResult{
        .code = code,
    };
}

[[nodiscard]] constexpr bool within_symmetric_limit(
    const std::int64_t value,
    const std::int64_t limit) noexcept
{
    return value >= -limit && value <= limit;
}

}

CommandValidator::CommandValidator(
    const CommandValidationPolicy policy)
    : policy_{policy}
{
    if (policy_.maximum_future_ticks == 0) {
        throw std::invalid_argument{
            "maximum future command window must be greater than zero"};
    }

    if (policy_.max_abs_velocity.count() <= 0) {
        throw std::invalid_argument{
            "maximum absolute command velocity must be positive"};
    }
}

const CommandValidationPolicy& CommandValidator::policy() const noexcept
{
    return policy_;
}

CommandValidationResult CommandValidator::validate(
    const CommandEnvelope& command,
    const CommandValidationContext& context) const
{
    if (!context.authenticated_client_id.valid()) {
        throw std::invalid_argument{
            "authenticated client identifier must be non-zero"};
    }

    if (!context.active_session_id.valid()) {
        throw std::invalid_argument{
            "active session identifier must be non-zero"};
    }

    if (command.schema_version != command_schema_version) {
        return reject(
            CommandRejectionCode::unsupported_schema_version);
    }

    if (command.type != CommandType::set_velocity) {
        return reject(
            CommandRejectionCode::unsupported_command_type);
    }

    if (!command.client_id.valid()) {
        return reject(
            CommandRejectionCode::invalid_client_id);
    }

    if (!command.session_id.valid()) {
        return reject(
            CommandRejectionCode::invalid_session_id);
    }

    if (command.client_id != context.authenticated_client_id) {
        return reject(
            CommandRejectionCode::client_identity_mismatch);
    }

    if (command.session_id != context.active_session_id) {
        return reject(
            CommandRejectionCode::session_identity_mismatch);
    }

    if (command.sequence == 0) {
        return reject(
            CommandRejectionCode::sequence_zero);
    }

    if (command.target_tick <= context.current_tick) {
        return reject(
            CommandRejectionCode::stale_target_tick);
    }

    const auto future_distance =
        command.target_tick - context.current_tick;

    if (future_distance > policy_.maximum_future_ticks) {
        return reject(
            CommandRejectionCode::target_tick_too_far_future);
    }

    if (!velocity_is_valid(command.payload.velocity)) {
        return reject(
            CommandRejectionCode::velocity_out_of_range);
    }

    return reject(CommandRejectionCode::none);
}

bool CommandValidator::velocity_is_valid(
    const simulation::Velocity2& velocity) const noexcept
{
    const auto limit =
        policy_.max_abs_velocity.count();

    return within_symmetric_limit(
               velocity.x.count(),
               limit) &&
           within_symmetric_limit(
               velocity.y.count(),
               limit);
}

}
