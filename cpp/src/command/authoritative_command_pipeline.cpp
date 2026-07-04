#include "tickline/command/authoritative_command_pipeline.hpp"

#include "tickline/simulation/canonical_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tickline::command {
namespace {

[[nodiscard]] std::uint64_t checked_count(
    const std::size_t value)
{
    if (!std::in_range<std::uint64_t>(value)) {
        throw std::overflow_error{
            "command count cannot be represented in evidence"};
    }

    return static_cast<std::uint64_t>(value);
}

} // namespace

CommandSubmissionResult CommandSubmissionResult::reject(
    const CommandRejectionCode code)
{
    if (code == CommandRejectionCode::none) {
        throw std::invalid_argument{
            "rejected submission requires a rejection code"};
    }

    return CommandSubmissionResult{
        code,
        std::nullopt,
    };
}

CommandSubmissionResult CommandSubmissionResult::from_queue_result(
    const simulation::QueueCommandResult queue_result)
{
    return CommandSubmissionResult{
        command_rejection_from_queue_result(queue_result),
        queue_result,
    };
}

bool CommandSubmissionResult::accepted() const noexcept
{
    return code_ == CommandRejectionCode::none;
}

CommandRejectionCode CommandSubmissionResult::code() const noexcept
{
    return code_;
}

const std::optional<simulation::QueueCommandResult>&
CommandSubmissionResult::queue_result() const noexcept
{
    return queue_result_;
}

CommandSubmissionResult::CommandSubmissionResult(
    const CommandRejectionCode code,
    std::optional<simulation::QueueCommandResult> queue_result)
    : code_{code},
      queue_result_{queue_result}
{
    if (!queue_result_.has_value()) {
        if (code_ == CommandRejectionCode::none) {
            throw std::invalid_argument{
                "accepted submission requires an accepted queue result"};
        }

        return;
    }

    const auto mapped_code =
        command_rejection_from_queue_result(
            queue_result_.value());

    if (mapped_code != code_) {
        throw std::invalid_argument{
            "submission result does not match its queue result"};
    }
}

CommandSubmissionResult AuthoritativeCommandPipeline::submit(
    CommandSession& session,
    simulation::World& world,
    const CommandEnvelope& command)
{
    evidence_.prepare_append();

    const auto observed_tick =
        world.current_tick().index;

    const auto world_fingerprint_before =
        simulation::fingerprint_world_state(world);

    const auto session_sequence_before =
        session.highest_accepted_sequence();

    const auto pending_commands_before =
        checked_count(
            world.pending_command_count());

    const auto admission = session.evaluate(
        command,
        observed_tick);

    if (!admission.accepted()) {
        auto result =
            CommandSubmissionResult::reject(
                admission.code());

        evidence_.append(
            CommandEvidenceEntry{
                .world_fingerprint_before =
                    world_fingerprint_before,
                .observed_tick = observed_tick,
                .envelope = command,
                .session_sequence_before =
                    session_sequence_before,
                .session_sequence_after =
                    session.highest_accepted_sequence(),
                .pending_commands_before =
                    pending_commands_before,
                .pending_commands_after =
                    checked_count(
                        world.pending_command_count()),
                .rejection_code = result.code(),
                .queue_outcome =
                    CommandQueueOutcome::not_attempted,
            });

        return result;
    }

    if (!admission.command().has_value()) {
        throw std::logic_error{
            "accepted command admission omitted its simulation command"};
    }

    const auto queue_result = world.queue_velocity(
        admission.command().value());

    auto result =
        CommandSubmissionResult::from_queue_result(
            queue_result);

    if (result.accepted()) {
        session.commit_accepted_sequence(
            command.sequence);
    }

    evidence_.append(
        CommandEvidenceEntry{
            .world_fingerprint_before =
                world_fingerprint_before,
            .observed_tick = observed_tick,
            .envelope = command,
            .session_sequence_before =
                session_sequence_before,
            .session_sequence_after =
                session.highest_accepted_sequence(),
            .pending_commands_before =
                pending_commands_before,
            .pending_commands_after =
                checked_count(
                    world.pending_command_count()),
            .rejection_code = result.code(),
            .queue_outcome =
                command_queue_outcome(queue_result),
        });

    return result;
}

const CommandEvidenceLog&
AuthoritativeCommandPipeline::evidence() const noexcept
{
    return evidence_;
}

}
