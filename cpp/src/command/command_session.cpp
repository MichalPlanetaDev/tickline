#include "tickline/command/command_session.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace tickline::command {

CommandAdmissionResult CommandAdmissionResult::accept(
    simulation::VelocityCommand command)
{
    return CommandAdmissionResult{
        CommandRejectionCode::none,
        std::optional<simulation::VelocityCommand>{
            std::move(command)},
    };
}

CommandAdmissionResult CommandAdmissionResult::reject(
    const CommandRejectionCode code)
{
    if (code == CommandRejectionCode::none) {
        throw std::invalid_argument{
            "accepted command result requires a simulation command"};
    }

    return CommandAdmissionResult{
        code,
        std::nullopt,
    };
}

bool CommandAdmissionResult::accepted() const noexcept
{
    return code_ == CommandRejectionCode::none;
}

CommandRejectionCode CommandAdmissionResult::code() const noexcept
{
    return code_;
}

const std::optional<simulation::VelocityCommand>&
CommandAdmissionResult::command() const noexcept
{
    return command_;
}

CommandAdmissionResult::CommandAdmissionResult(
    const CommandRejectionCode code,
    std::optional<simulation::VelocityCommand> command)
    : code_{code},
      command_{std::move(command)}
{
    const auto has_accepted_code =
        code_ == CommandRejectionCode::none;

    if (has_accepted_code != command_.has_value()) {
        throw std::invalid_argument{
            "command admission result has inconsistent state"};
    }
}

CommandSession::CommandSession(
    const ClientId client_id,
    const SessionId session_id,
    const CommandValidationPolicy policy)
    : client_id_{client_id},
      session_id_{session_id},
      validator_{policy}
{
    if (!client_id_.valid()) {
        throw std::invalid_argument{
            "command session client identifier must be non-zero"};
    }

    if (!session_id_.valid()) {
        throw std::invalid_argument{
            "command session identifier must be non-zero"};
    }
}

ClientId CommandSession::client_id() const noexcept
{
    return client_id_;
}

SessionId CommandSession::session_id() const noexcept
{
    return session_id_;
}

std::uint64_t CommandSession::highest_accepted_sequence() const noexcept
{
    return highest_accepted_sequence_;
}

const CommandValidationPolicy& CommandSession::policy() const noexcept
{
    return validator_.policy();
}

CommandAdmissionResult CommandSession::evaluate(
    const CommandEnvelope& command,
    const std::uint64_t current_tick) const
{
    const auto validation = validator_.validate(
        command,
        CommandValidationContext{
            .authenticated_client_id = client_id_,
            .active_session_id = session_id_,
            .current_tick = current_tick,
        });

    if (!validation.accepted()) {
        return CommandAdmissionResult::reject(
            validation.code);
    }

    if (command.sequence == highest_accepted_sequence_) {
        return CommandAdmissionResult::reject(
            CommandRejectionCode::duplicate_sequence);
    }

    if (command.sequence < highest_accepted_sequence_) {
        return CommandAdmissionResult::reject(
            CommandRejectionCode::sequence_regression);
    }

    return CommandAdmissionResult::accept(
        translate(command));
}

simulation::VelocityCommand CommandSession::translate(
    const CommandEnvelope& command)
{
    return simulation::VelocityCommand{
        .target_tick = command.target_tick,
        .entity_id = command.payload.entity_id,
        .sequence = command.sequence,
        .velocity = command.payload.velocity,
    };
}

void CommandSession::commit_accepted_sequence(
    const std::uint64_t sequence) noexcept
{
    highest_accepted_sequence_ = sequence;
}

}
