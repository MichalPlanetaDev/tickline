#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/simulation/canonical_state.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using tickline::command::AuthoritativeCommandPipeline;
using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandRejectionCode;
using tickline::command::CommandSession;
using tickline::command::CommandSubmissionResult;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::simulation::AddEntityResult;
using tickline::simulation::EntityId;
using tickline::simulation::EntityState;
using tickline::simulation::Microseconds;
using tickline::simulation::Millimeters;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Position2;
using tickline::simulation::QueueCommandResult;
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

[[nodiscard]] CommandValidationPolicy make_policy(
    const std::int64_t velocity_limit = 1'000)
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity =
            MillimetersPerSecond{velocity_limit},
    };
}

[[nodiscard]] World make_world(
    const std::int64_t velocity_limit = 1'000)
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position = Millimeters{100'000},
            .max_abs_velocity =
                MillimetersPerSecond{velocity_limit},
        }};

    const auto result = world.add_entity(
        EntityState{
            .id = EntityId{1},
            .position = Position2{
                .x = Millimeters{0},
                .y = Millimeters{0},
            },
            .velocity = Velocity2{
                .x = MillimetersPerSecond{0},
                .y = MillimetersPerSecond{0},
            },
            .last_sequence = 0,
        });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "pipeline test entity could not be added"};
    }

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 1,
    const std::int64_t velocity_x = 25,
    const ClientId client_id = ClientId{7},
    const SessionId session_id = SessionId{11})
{
    return CommandEnvelope{
        .schema_version =
            tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = client_id,
        .session_id = session_id,
        .sequence = sequence,
        .target_tick = target_tick,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{entity_id},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{velocity_x},
                .y = MillimetersPerSecond{0},
            },
        },
    };
}

void expect_rejected(
    const CommandSubmissionResult& result,
    const CommandRejectionCode expected,
    const std::string_view message)
{
    expect(!result.accepted(), message);
    expect(result.code() == expected, message);
}

void test_accepted_submission_commits_both_states()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    const auto result = pipeline.submit(
        session,
        world,
        make_command(1, 2));

    expect(
        result.accepted(),
        "valid submission should be accepted");

    expect(
        result.code() == CommandRejectionCode::none,
        "accepted submission should have no rejection code");

    expect(
        result.queue_result() ==
            std::optional<QueueCommandResult>{
                QueueCommandResult::accepted},
        "accepted submission should preserve the queue result");

    expect(
        session.highest_accepted_sequence() == 1,
        "accepted submission should commit replay state");

    expect(
        world.pending_command_count() == 1,
        "accepted submission should enqueue the simulation command");
}

void test_validation_rejection_mutates_neither_state()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto result = pipeline.submit(
        session,
        world,
        make_command(1, 9));

    expect_rejected(
        result,
        CommandRejectionCode::target_tick_too_far_future,
        "out-of-window submission should be rejected");

    expect(
        !result.queue_result().has_value(),
        "pre-queue rejection should not expose a queue result");

    expect(
        session.highest_accepted_sequence() == 0,
        "validation rejection must not consume sequence state");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "validation rejection must not mutate world state");
}

void test_unknown_entity_does_not_consume_sequence()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto result = pipeline.submit(
        session,
        world,
        make_command(1, 2, 99));

    expect_rejected(
        result,
        CommandRejectionCode::unknown_entity,
        "unknown entity should be mapped to a stable rejection code");

    expect(
        result.queue_result() ==
            std::optional<QueueCommandResult>{
                QueueCommandResult::unknown_entity},
        "world rejection should preserve its queue result");

    expect(
        session.highest_accepted_sequence() == 0,
        "world rejection must not consume session sequence");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "unknown-entity rejection must not mutate world state");

    expect(
        pipeline.submit(
                    session,
                    world,
                    make_command(1, 2))
            .accepted(),
        "sequence rejected by the world should remain reusable");
}

void test_target_tick_regression_is_transactional()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    expect(
        pipeline.submit(
                    session,
                    world,
                    make_command(1, 4))
            .accepted(),
        "initial command should be accepted");

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto result = pipeline.submit(
        session,
        world,
        make_command(2, 3));

    expect_rejected(
        result,
        CommandRejectionCode::target_tick_regression,
        "world target regression should be mapped");

    expect(
        session.highest_accepted_sequence() == 1,
        "target regression must not consume sequence two");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "target regression must not mutate world state");

    expect(
        pipeline.submit(
                    session,
                    world,
                    make_command(2, 5))
            .accepted(),
        "corrected command should reuse the uncommitted sequence");
}

void test_duplicate_and_regressed_sequences_skip_world()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    expect(
        pipeline.submit(
                    session,
                    world,
                    make_command(5, 3))
            .accepted(),
        "initial sequence should be accepted");

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto duplicate = pipeline.submit(
        session,
        world,
        make_command(5, 4));

    expect_rejected(
        duplicate,
        CommandRejectionCode::duplicate_sequence,
        "duplicate session sequence should be rejected");

    expect(
        !duplicate.queue_result().has_value(),
        "duplicate should be rejected before world admission");

    const auto regression = pipeline.submit(
        session,
        world,
        make_command(4, 4));

    expect_rejected(
        regression,
        CommandRejectionCode::sequence_regression,
        "regressed session sequence should be rejected");

    expect(
        !regression.queue_result().has_value(),
        "regression should be rejected before world admission");

    expect(
        session.highest_accepted_sequence() == 5,
        "replay rejection must preserve the committed sequence");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "replay rejection must not mutate world state");
}

void test_world_sequence_conflict_between_sessions()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession first{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    CommandSession second{
        ClientId{8},
        SessionId{12},
        make_policy(),
    };

    auto world = make_world();

    expect(
        pipeline.submit(
                    first,
                    world,
                    make_command(
                        10,
                        3,
                        1,
                        25,
                        ClientId{7},
                        SessionId{11}))
            .accepted(),
        "first session command should be accepted");

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto result = pipeline.submit(
        second,
        world,
        make_command(
            1,
            4,
            1,
            30,
            ClientId{8},
            SessionId{12}));

    expect_rejected(
        result,
        CommandRejectionCode::entity_sequence_not_increasing,
        "world entity sequence conflict should be mapped");

    expect(
        second.highest_accepted_sequence() == 0,
        "world sequence conflict must not commit second session state");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "world sequence conflict must not mutate world state");
}

void test_world_velocity_rejection_is_transactional()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(2'000),
    };

    auto world = make_world(1'000);

    const auto before =
        tickline::simulation::encode_world_state(world);

    const auto result = pipeline.submit(
        session,
        world,
        make_command(1, 2, 1, 1'500));

    expect_rejected(
        result,
        CommandRejectionCode::velocity_out_of_range,
        "world velocity limit should be mapped");

    expect(
        session.highest_accepted_sequence() == 0,
        "world velocity rejection must not consume sequence");

    expect(
        tickline::simulation::encode_world_state(world) == before,
        "world velocity rejection must not mutate world state");
}

void test_queue_result_mapping_contract()
{
    using tickline::command::
        command_rejection_from_queue_result;

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::accepted) ==
            CommandRejectionCode::none,
        "accepted queue result should map to none");

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::stale_tick) ==
            CommandRejectionCode::stale_target_tick,
        "stale queue result should map consistently");

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::unknown_entity) ==
            CommandRejectionCode::unknown_entity,
        "unknown entity should map consistently");

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::sequence_not_increasing) ==
            CommandRejectionCode::entity_sequence_not_increasing,
        "entity sequence rejection should map consistently");

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::target_tick_regression) ==
            CommandRejectionCode::target_tick_regression,
        "target regression should map consistently");

    expect(
        command_rejection_from_queue_result(
            QueueCommandResult::velocity_out_of_range) ==
            CommandRejectionCode::velocity_out_of_range,
        "velocity rejection should map consistently");

    expect(
        command_rejection_from_queue_result(
            static_cast<QueueCommandResult>(999)) ==
            CommandRejectionCode::simulation_rejection_unknown,
        "unknown queue result should fail closed");
}

void test_rejection_code_contract()
{
    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::unknown_entity) == 13);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::entity_sequence_not_increasing) ==
        14);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::target_tick_regression) == 15);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::simulation_rejection_unknown) ==
        16);
}

}

int main()
{
    try {
        test_accepted_submission_commits_both_states();
        test_validation_rejection_mutates_neither_state();
        test_unknown_entity_does_not_consume_sequence();
        test_target_tick_regression_is_transactional();
        test_duplicate_and_regressed_sequences_skip_world();
        test_world_sequence_conflict_between_sessions();
        test_world_velocity_rejection_is_transactional();
        test_queue_result_mapping_contract();
        test_rejection_code_contract();
    } catch (const std::exception& error) {
        std::cerr
            << "authoritative pipeline test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
