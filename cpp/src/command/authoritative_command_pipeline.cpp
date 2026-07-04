#include "tickline/command/authoritative_command_pipeline.hpp"

#include <optional>
#include <stdexcept>

namespace tickline::command {

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
    const CommandEnvelope& command) const
{
    const auto admission = session.evaluate(
        command,
        world.current_tick().index);

    if (!admission.accepted()) {
        return CommandSubmissionResult::reject(
            admission.code());
    }

    if (!admission.command().has_value()) {
        throw std::logic_error{
            "accepted command admission omitted its simulation command"};
    }

    const auto queue_result = world.queue_velocity(
        admission.command().value());

    auto submission =
        CommandSubmissionResult::from_queue_result(
            queue_result);

    if (submission.accepted()) {
        session.commit_accepted_sequence(
            command.sequence);
    }

    return submission;
}

}
