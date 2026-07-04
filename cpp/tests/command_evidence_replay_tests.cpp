#include "tickline/command/command_evidence_replay.hpp"
#include "tickline/simulation/canonical_state.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using tickline::command::AuthoritativeCommandPipeline;
using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandEvidenceArchive;
using tickline::command::CommandEvidenceRecord;
using tickline::command::CommandEvidenceReplayCode;
using tickline::command::CommandEvidenceReplayPolicy;
using tickline::command::CommandEvidenceReplayer;
using tickline::command::CommandSession;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::security::Sha256Digest;
using tickline::simulation::AddEntityResult;
using tickline::simulation::EntityId;
using tickline::simulation::EntityState;
using tickline::simulation::Microseconds;
using tickline::simulation::Millimeters;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Position2;
using tickline::simulation::StateFingerprint;
using tickline::simulation::Velocity2;
using tickline::simulation::World;
using tickline::simulation::WorldLimits;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{
            std::string{message}};
    }
}

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity =
            MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] CommandSession make_session()
{
    return CommandSession{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };
}

[[nodiscard]] World make_world(
    const std::int64_t initial_position = 0,
    const std::int64_t initial_velocity = 0)
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position =
                Millimeters{100'000},
            .max_abs_velocity =
                MillimetersPerSecond{1'000},
        }};

    const auto result =
        world.add_entity(
            EntityState{
                .id = EntityId{3},
                .position =
                    Position2{
                        .x =
                            Millimeters{
                                initial_position},
                        .y =
                            Millimeters{0},
                    },
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{
                                initial_velocity},
                        .y =
                            MillimetersPerSecond{0},
                    },
                .last_sequence = 0,
            });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "replay test entity could not be added"};
    }

    return world;
}

[[nodiscard]] World make_failing_world()
{
    World world{
        Microseconds{1'000'000},
        WorldLimits{
            .max_abs_position =
                Millimeters{100},
            .max_abs_velocity =
                MillimetersPerSecond{100},
        }};

    const auto result =
        world.add_entity(
            EntityState{
                .id = EntityId{3},
                .position =
                    Position2{
                        .x = Millimeters{100},
                        .y = Millimeters{0},
                    },
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{100},
                        .y =
                            MillimetersPerSecond{0},
                    },
                .last_sequence = 0,
            });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "failing replay entity could not be added"};
    }

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 3)
{
    return CommandEnvelope{
        .schema_version =
            tickline::command::
                command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = sequence,
        .target_tick = target_tick,
        .payload =
            SetVelocityPayload{
                .entity_id =
                    EntityId{entity_id},
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{25},
                        .y =
                            MillimetersPerSecond{-40},
                    },
            },
    };
}

void execute_history(
    AuthoritativeCommandPipeline& pipeline,
    CommandSession& session,
    World& world)
{
    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    static_cast<void>(world.advance());

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3, 99)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3)));
}

[[nodiscard]] Sha256Digest rebuild_chain(
    std::vector<CommandEvidenceRecord>& records)
{
    Sha256Digest previous{};

    for (std::size_t index = 0;
         index < records.size();
         ++index) {
        records[index].ordinal =
            static_cast<std::uint64_t>(index);

        records[index].previous_record_digest =
            previous;

        previous =
            tickline::command::
                digest_command_evidence(
                    records[index]);
    }

    return previous;
}

void test_valid_history_replays_exactly()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            pipeline.evidence().records(),
            pipeline.evidence().head_digest());

    expect(
        result.verified(),
        "valid evidence history should replay successfully");

    expect(
        result.code ==
            CommandEvidenceReplayCode::verified,
        "successful replay should use the verified code");

    expect(
        result.processed_records == 4,
        "replay should process all evidence records");

    expect(
        !result.failed_ordinal.has_value(),
        "successful replay should not identify a failed ordinal");

    expect(
        result.performed_tick_advances == 1,
        "replay should reconstruct the observed tick transition");

    expect(
        result.final_tick == 1,
        "replay should finish at the last observed tick");

    expect(
        result.final_session_sequence == 2,
        "replay should reconstruct committed session state");

    expect(
        result.final_pending_command_count == 2,
        "replay should reconstruct the authoritative queue");

    expect(
        result.final_world_fingerprint ==
            tickline::simulation::
                fingerprint_world_state(
                    execution_world),
        "replayed world should match the original execution");

    expect(
        result.replayed_head_digest ==
            pipeline.evidence().head_digest(),
        "replay should regenerate the trusted evidence head");

    expect(
        initial_session.highest_accepted_sequence() == 0,
        "replay must not mutate the supplied initial session");

    expect(
        initial_world.current_tick().index == 0 &&
            initial_world.pending_command_count() == 0,
        "replay must not mutate the supplied initial world");
}

void test_archive_overload()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const auto source =
        pipeline.evidence().records();

    const CommandEvidenceArchive archive{
        .records =
            std::vector<CommandEvidenceRecord>{
                source.begin(),
                source.end(),
            },
        .head_digest =
            pipeline.evidence().head_digest(),
    };

    const CommandEvidenceReplayer replayer;

    expect(
        replayer.replay(
                    initial_session,
                    initial_world,
                    archive)
            .verified(),
        "decoded archive object should replay successfully");
}

void test_invalid_chain_is_rejected_before_replay()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const auto source =
        pipeline.evidence().records();

    std::vector<CommandEvidenceRecord> tampered{
        source.begin(),
        source.end(),
    };

    tampered[0].entry.envelope.sequence = 999;

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            tampered,
            pipeline.evidence().head_digest());

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                chain_verification_failed,
        "invalid chain should fail before semantic replay");

    expect(
        result.processed_records == 0,
        "invalid chain should process no records");
}

void test_recomputed_chain_does_not_hide_world_tampering()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const auto source =
        pipeline.evidence().records();

    std::vector<CommandEvidenceRecord> tampered{
        source.begin(),
        source.end(),
    };

    tampered[0]
        .entry
        .world_fingerprint_before =
        StateFingerprint{123};

    const auto recomputed_head =
        rebuild_chain(tampered);

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            tampered,
            recomputed_head);

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                world_state_mismatch,
        "semantic replay should detect a forged world fingerprint");

    expect(
        result.failed_ordinal ==
            std::optional<std::uint64_t>{0},
        "world mismatch should identify the first record");
}

void test_recomputed_chain_does_not_hide_outcome_tampering()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const auto source =
        pipeline.evidence().records();

    std::vector<CommandEvidenceRecord> tampered{
        source.begin(),
        source.end(),
    };

    tampered[1].entry.rejection_code =
        tickline::command::
            CommandRejectionCode::
                sequence_regression;

    const auto recomputed_head =
        rebuild_chain(tampered);

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            tampered,
            recomputed_head);

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                submission_record_mismatch,
        "replay should detect a forged submission outcome");

    expect(
        result.processed_records == 1,
        "replay should stop before counting the mismatched record");

    expect(
        result.failed_ordinal ==
            std::optional<std::uint64_t>{1},
        "outcome mismatch should identify its evidence ordinal");
}

void test_observed_tick_regression_is_rejected()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    static_cast<void>(
        execution_world.advance());

    AuthoritativeCommandPipeline pipeline;

    static_cast<void>(
        pipeline.submit(
            execution_session,
            execution_world,
            make_command(1, 2)));

    static_cast<void>(
        pipeline.submit(
            execution_session,
            execution_world,
            make_command(2, 3)));

    const auto source =
        pipeline.evidence().records();

    std::vector<CommandEvidenceRecord> tampered{
        source.begin(),
        source.end(),
    };

    tampered[1].entry.observed_tick = 0;

    const auto recomputed_head =
        rebuild_chain(tampered);

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            tampered,
            recomputed_head);

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                observed_tick_regression,
        "observed ticks must not regress during replay");

    expect(
        result.processed_records == 1,
        "tick regression should stop after the first record");
}

void test_tick_advance_budget_is_enforced()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    static_cast<void>(
        execution_world.advance());

    AuthoritativeCommandPipeline pipeline;

    static_cast<void>(
        pipeline.submit(
            execution_session,
            execution_world,
            make_command(1, 2)));

    const CommandEvidenceReplayer replayer{
        CommandEvidenceReplayPolicy{
            .maximum_tick_advances = 0,
        }};

    const auto result =
        replayer.replay(
            initial_session,
            initial_world,
            pipeline.evidence().records(),
            pipeline.evidence().head_digest());

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                tick_advance_limit_exceeded,
        "replay should enforce its total tick-advance budget");

    expect(
        result.performed_tick_advances == 0,
        "rejected tick advance must not consume budget");
}

void test_wrong_initial_world_is_rejected()
{
    const auto initial_session =
        make_session();

    const auto initial_world =
        make_world();

    auto execution_session =
        initial_session;

    auto execution_world =
        initial_world;

    AuthoritativeCommandPipeline pipeline;

    execute_history(
        pipeline,
        execution_session,
        execution_world);

    const auto wrong_initial_world =
        make_world(50);

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            wrong_initial_world,
            pipeline.evidence().records(),
            pipeline.evidence().head_digest());

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                world_state_mismatch,
        "replay should reject an incorrect initial world");
}

void test_simulation_advance_failure_is_reported()
{
    const auto initial_session =
        make_session();

    auto safe_world =
        make_world();

    static_cast<void>(
        safe_world.advance());

    auto execution_session =
        initial_session;

    AuthoritativeCommandPipeline pipeline;

    static_cast<void>(
        pipeline.submit(
            execution_session,
            safe_world,
            make_command(1, 2)));

    const auto failing_world =
        make_failing_world();

    const CommandEvidenceReplayer replayer;

    const auto result =
        replayer.replay(
            initial_session,
            failing_world,
            pipeline.evidence().records(),
            pipeline.evidence().head_digest());

    expect(
        result.code ==
            CommandEvidenceReplayCode::
                simulation_advance_failed,
        "simulation failure during replay should be reported");
}

void test_replay_code_names()
{
    expect(
        tickline::command::
            command_evidence_replay_code_name(
                CommandEvidenceReplayCode::
                    verified) ==
            "verified",
        "verified replay code should have a stable name");

    expect(
        tickline::command::
            command_evidence_replay_code_name(
                CommandEvidenceReplayCode::
                    submission_record_mismatch) ==
            "submission_record_mismatch",
        "record mismatch should have a stable name");

    expect(
        tickline::command::
            command_evidence_replay_code_name(
                static_cast<
                    CommandEvidenceReplayCode>(
                    999)) ==
            "unknown",
        "unknown replay code should fail closed");
}

}

int main()
{
    try {
        test_valid_history_replays_exactly();
        test_archive_overload();
        test_invalid_chain_is_rejected_before_replay();
        test_recomputed_chain_does_not_hide_world_tampering();
        test_recomputed_chain_does_not_hide_outcome_tampering();
        test_observed_tick_regression_is_rejected();
        test_tick_advance_budget_is_enforced();
        test_wrong_initial_world_is_rejected();
        test_simulation_advance_failure_is_reported();
        test_replay_code_names();
    } catch (const std::exception& error) {
        std::cerr
            << "command evidence replay test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
