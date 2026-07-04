#pragma once

#include "tickline/command/command_session.hpp"
#include "tickline/simulation/world.hpp"

#include <optional>

namespace tickline::command {

[[nodiscard]] constexpr CommandRejectionCode
command_rejection_from_queue_result(
    const simulation::QueueCommandResult result) noexcept
{
    switch (result) {
    case simulation::QueueCommandResult::accepted:
        return CommandRejectionCode::none;

    case simulation::QueueCommandResult::stale_tick:
        return CommandRejectionCode::stale_target_tick;

    case simulation::QueueCommandResult::unknown_entity:
        return CommandRejectionCode::unknown_entity;

    case simulation::QueueCommandResult::sequence_not_increasing:
        return CommandRejectionCode::entity_sequence_not_increasing;

    case simulation::QueueCommandResult::target_tick_regression:
        return CommandRejectionCode::target_tick_regression;

    case simulation::QueueCommandResult::velocity_out_of_range:
        return CommandRejectionCode::velocity_out_of_range;
    }

    return CommandRejectionCode::simulation_rejection_unknown;
}

class CommandSubmissionResult final {
public:
    [[nodiscard]] static CommandSubmissionResult reject(
        CommandRejectionCode code);

    [[nodiscard]] static CommandSubmissionResult from_queue_result(
        simulation::QueueCommandResult queue_result);

    [[nodiscard]] bool accepted() const noexcept;

    [[nodiscard]] CommandRejectionCode code() const noexcept;

    [[nodiscard]] const std::optional<simulation::QueueCommandResult>&
    queue_result() const noexcept;

private:
    CommandSubmissionResult(
        CommandRejectionCode code,
        std::optional<simulation::QueueCommandResult> queue_result);

    CommandRejectionCode code_;
    std::optional<simulation::QueueCommandResult> queue_result_;
};

class AuthoritativeCommandPipeline final {
public:
    [[nodiscard]] CommandSubmissionResult submit(
        CommandSession& session,
        simulation::World& world,
        const CommandEnvelope& command) const;
};

}
