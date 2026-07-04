#include "tickline/command/command_evidence_replay.hpp"

#include "tickline/simulation/canonical_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace tickline::command {
namespace {

[[nodiscard]] CommandEvidenceReplayResult make_result(
    const CommandEvidenceReplayCode code,
    const std::size_t processed_records,
    const std::optional<std::uint64_t> failed_ordinal,
    const std::uint64_t performed_tick_advances,
    const CommandSession& session,
    const simulation::World& world,
    const AuthoritativeCommandPipeline& pipeline)
{
    return CommandEvidenceReplayResult{
        .code = code,
        .processed_records = processed_records,
        .failed_ordinal = failed_ordinal,
        .performed_tick_advances = performed_tick_advances,
        .final_tick = world.current_tick().index,
        .final_session_sequence =
            session.highest_accepted_sequence(),
        .final_pending_command_count =
            world.pending_command_count(),
        .final_world_fingerprint =
            simulation::fingerprint_world_state(world),
        .replayed_head_digest =
            pipeline.evidence().head_digest(),
    };
}

}

CommandEvidenceReplayResult CommandEvidenceReplayer::replay(
    const CommandSession& initial_session,
    const simulation::World& initial_world,
    const std::span<const CommandEvidenceRecord> records,
    const security::Sha256Digest trusted_head) const
{
    auto session = initial_session;
    auto world = initial_world;

    AuthoritativeCommandPipeline pipeline;

    std::uint64_t performed_tick_advances = 0;

    if (!verify_command_evidence_chain(
            records,
            trusted_head)) {
        return make_result(
            CommandEvidenceReplayCode::
                chain_verification_failed,
            0,
            std::nullopt,
            performed_tick_advances,
            session,
            world,
            pipeline);
    }

    for (std::size_t index = 0;
         index < records.size();
         ++index) {
        const auto& expected_record =
            records[index];

        const auto failed_ordinal =
            std::optional<std::uint64_t>{
                expected_record.ordinal};

        const auto current_tick =
            world.current_tick().index;

        if (expected_record.entry.observed_tick <
            current_tick) {
            return make_result(
                CommandEvidenceReplayCode::
                    observed_tick_regression,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        const auto required_advances =
            expected_record.entry.observed_tick -
            current_tick;

        const auto remaining_budget =
            policy_.maximum_tick_advances -
            performed_tick_advances;

        if (required_advances > remaining_budget) {
            return make_result(
                CommandEvidenceReplayCode::
                    tick_advance_limit_exceeded,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        try {
            for (std::uint64_t step = 0;
                 step < required_advances;
                 ++step) {
                static_cast<void>(
                    world.advance());
            }
        } catch (const std::overflow_error&) {
            return make_result(
                CommandEvidenceReplayCode::
                    simulation_advance_failed,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        } catch (const std::logic_error&) {
            return make_result(
                CommandEvidenceReplayCode::
                    simulation_advance_failed,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        performed_tick_advances +=
            required_advances;

        if (session.highest_accepted_sequence() !=
            expected_record.entry
                .session_sequence_before) {
            return make_result(
                CommandEvidenceReplayCode::
                    session_state_mismatch,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        if (world.pending_command_count() !=
            expected_record.entry
                .pending_commands_before) {
            return make_result(
                CommandEvidenceReplayCode::
                    pending_command_count_mismatch,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        if (simulation::fingerprint_world_state(world) !=
            expected_record.entry
                .world_fingerprint_before) {
            return make_result(
                CommandEvidenceReplayCode::
                    world_state_mismatch,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }

        static_cast<void>(
            pipeline.submit(
                session,
                world,
                expected_record.entry.envelope));

        const auto generated_records =
            pipeline.evidence().records();

        if (generated_records.size() !=
                index + 1 ||
            generated_records[index] !=
                expected_record) {
            return make_result(
                CommandEvidenceReplayCode::
                    submission_record_mismatch,
                index,
                failed_ordinal,
                performed_tick_advances,
                session,
                world,
                pipeline);
        }
    }

    return make_result(
        CommandEvidenceReplayCode::verified,
        records.size(),
        std::nullopt,
        performed_tick_advances,
        session,
        world,
        pipeline);
}

CommandEvidenceReplayResult CommandEvidenceReplayer::replay(
    const CommandSession& initial_session,
    const simulation::World& initial_world,
    const CommandEvidenceArchive& archive) const
{
    return replay(
        initial_session,
        initial_world,
        archive.records,
        archive.head_digest);
}

}
