#include "tickline/simulation/world.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using tickline::simulation::AddEntityResult;
using tickline::simulation::EntityId;
using tickline::simulation::EntityState;
using tickline::simulation::Microseconds;
using tickline::simulation::Millimeters;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Position2;
using tickline::simulation::QueueCommandResult;
using tickline::simulation::Velocity2;
using tickline::simulation::VelocityCommand;
using tickline::simulation::World;
using tickline::simulation::WorldLimits;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

template <typename ExpectedException, typename Function>
void expect_throws(Function function, const std::string_view message)
{
    try {
        function();
    } catch (const ExpectedException&) {
        return;
    }

    throw std::runtime_error{std::string{message}};
}

[[nodiscard]] EntityState make_entity(
    const std::uint64_t id,
    const std::int64_t position_x,
    const std::int64_t position_y,
    const std::int64_t velocity_x,
    const std::int64_t velocity_y)
{
    return EntityState{
        .id = EntityId{id},
        .position = Position2{
            .x = Millimeters{position_x},
            .y = Millimeters{position_y},
        },
        .velocity = Velocity2{
            .x = MillimetersPerSecond{velocity_x},
            .y = MillimetersPerSecond{velocity_y},
        },
        .last_sequence = 0,
    };
}

[[nodiscard]] VelocityCommand make_command(
    const std::uint64_t target_tick,
    const std::uint64_t entity_id,
    const std::uint64_t sequence,
    const std::int64_t velocity_x,
    const std::int64_t velocity_y)
{
    return VelocityCommand{
        .target_tick = target_tick,
        .entity_id = EntityId{entity_id},
        .sequence = sequence,
        .velocity = Velocity2{
            .x = MillimetersPerSecond{velocity_x},
            .y = MillimetersPerSecond{velocity_y},
        },
    };
}

void test_world_configuration()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(World{
                Microseconds{100'000},
                WorldLimits{
                    .max_abs_position = Millimeters{0},
                    .max_abs_velocity = MillimetersPerSecond{1'000},
                }});
        },
        "zero position limit should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(World{
                Microseconds{100'000},
                WorldLimits{
                    .max_abs_position = Millimeters{1'000},
                    .max_abs_velocity = MillimetersPerSecond{0},
                }});
        },
        "zero velocity limit should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(World{
                Microseconds{
                    std::numeric_limits<std::int64_t>::max()},
                WorldLimits{
                    .max_abs_position = Millimeters{1'000},
                    .max_abs_velocity = MillimetersPerSecond{2},
                }});
        },
        "unsafe velocity and tick duration combination should be rejected");
}

void test_entity_admission()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position = Millimeters{1'000},
            .max_abs_velocity = MillimetersPerSecond{100},
        }};

    expect(
        world.add_entity(make_entity(2, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "valid entity should be added");

    expect(
        world.add_entity(make_entity(2, 0, 0, 0, 0)) ==
            AddEntityResult::duplicate_id,
        "duplicate entity identifier should be rejected");

    expect(
        world.add_entity(make_entity(3, 1'001, 0, 0, 0)) ==
            AddEntityResult::position_out_of_range,
        "out-of-range position should be rejected");

    expect(
        world.add_entity(make_entity(4, 0, 0, 101, 0)) ==
            AddEntityResult::velocity_out_of_range,
        "out-of-range velocity should be rejected");

    expect(world.entity_count() == 1, "only one entity should exist");
}

void test_command_rejection()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position = Millimeters{10'000},
            .max_abs_velocity = MillimetersPerSecond{1'000},
        }};

    expect(
        world.add_entity(make_entity(1, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "test entity should be added");

    expect(
        world.queue_velocity(make_command(0, 1, 1, 10, 0)) ==
            QueueCommandResult::stale_tick,
        "current tick command should be rejected");

    expect(
        world.queue_velocity(make_command(1, 99, 1, 10, 0)) ==
            QueueCommandResult::unknown_entity,
        "unknown entity command should be rejected");

    expect(
        world.queue_velocity(make_command(2, 1, 1, 10, 0)) ==
            QueueCommandResult::accepted,
        "first valid command should be accepted");

    expect(
        world.queue_velocity(make_command(3, 1, 1, 10, 0)) ==
            QueueCommandResult::sequence_not_increasing,
        "duplicate sequence should be rejected");

    expect(
        world.queue_velocity(make_command(1, 1, 2, 10, 0)) ==
            QueueCommandResult::target_tick_regression,
        "target tick regression should be rejected");

    expect(
        world.queue_velocity(make_command(3, 1, 2, 1'001, 0)) ==
            QueueCommandResult::velocity_out_of_range,
        "out-of-range command velocity should be rejected");

    expect(
        world.pending_command_count() == 1,
        "only the accepted command should remain queued");
}

void test_fractional_integration()
{
    World world{Microseconds{100'000}};

    expect(
        world.add_entity(make_entity(1, 0, 0, 1, -1)) ==
            AddEntityResult::added,
        "fractional integration entity should be added");

    for (int tick = 0; tick < 9; ++tick) {
        static_cast<void>(world.advance());
    }

    const auto before_whole_millimeter = world.entity(EntityId{1});

    expect(before_whole_millimeter.has_value(), "entity should exist");

    expect(
        before_whole_millimeter->position ==
            Position2{
                .x = Millimeters{0},
                .y = Millimeters{0},
            },
        "sub-millimeter residual should not be discarded");

    static_cast<void>(world.advance());

    const auto after_ten_ticks = world.entity(EntityId{1});

    expect(
        after_ten_ticks.has_value(),
        "entity should exist after advance");

    expect(
        after_ten_ticks->position ==
            Position2{
                .x = Millimeters{1},
                .y = Millimeters{-1},
            },
        "ten 100 ms ticks at one millimeter per second should move one millimeter");
}

void test_scheduled_commands()
{
    World world{Microseconds{100'000}};

    expect(
        world.add_entity(make_entity(1, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "scheduled-command entity should be added");

    expect(
        world.queue_velocity(make_command(2, 1, 1, 10, 0)) ==
            QueueCommandResult::accepted,
        "future command should be accepted");

    static_cast<void>(world.advance());

    auto state = world.entity(EntityId{1});

    expect(state.has_value(), "entity should exist after first tick");

    expect(
        state->position.x == Millimeters{0},
        "future command must not apply early");

    expect(
        state->last_sequence == 0,
        "future command sequence must not apply early");

    static_cast<void>(world.advance());

    state = world.entity(EntityId{1});

    expect(state.has_value(), "entity should exist after second tick");

    expect(
        state->position.x == Millimeters{1},
        "command should apply before integration on its target tick");

    expect(
        state->velocity.x == MillimetersPerSecond{10},
        "accepted command should update authoritative velocity");

    expect(
        state->last_sequence == 1,
        "accepted command sequence should be recorded");

    expect(
        world.pending_command_count() == 0,
        "processed command should be removed");
}

void test_stable_snapshot_order()
{
    World world{Microseconds{100'000}};

    expect(
        world.add_entity(make_entity(30, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "entity 30 should be added");

    expect(
        world.add_entity(make_entity(10, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "entity 10 should be added");

    expect(
        world.add_entity(make_entity(20, 0, 0, 0, 0)) ==
            AddEntityResult::added,
        "entity 20 should be added");

    const auto snapshot = world.snapshot();

    expect(snapshot.size() == 3, "snapshot should contain three entities");
    expect(snapshot[0].id == EntityId{10}, "first entity should have lowest id");
    expect(snapshot[1].id == EntityId{20}, "second entity should have middle id");
    expect(snapshot[2].id == EntityId{30}, "third entity should have highest id");
}

void test_replay_is_independent_of_insertion_order()
{
    World first{Microseconds{100'000}};
    World second{Microseconds{100'000}};

    expect(
        first.add_entity(make_entity(2, 100, -100, 0, 0)) ==
            AddEntityResult::added,
        "first world entity 2 should be added");

    expect(
        first.add_entity(make_entity(1, -100, 100, 0, 0)) ==
            AddEntityResult::added,
        "first world entity 1 should be added");

    expect(
        second.add_entity(make_entity(1, -100, 100, 0, 0)) ==
            AddEntityResult::added,
        "second world entity 1 should be added");

    expect(
        second.add_entity(make_entity(2, 100, -100, 0, 0)) ==
            AddEntityResult::added,
        "second world entity 2 should be added");

    expect(
        first.queue_velocity(make_command(2, 2, 1, -25, 40)) ==
            QueueCommandResult::accepted,
        "first world entity 2 command should be accepted");

    expect(
        first.queue_velocity(make_command(1, 1, 1, 30, -20)) ==
            QueueCommandResult::accepted,
        "first world entity 1 command should be accepted");

    expect(
        second.queue_velocity(make_command(1, 1, 1, 30, -20)) ==
            QueueCommandResult::accepted,
        "second world entity 1 command should be accepted");

    expect(
        second.queue_velocity(make_command(2, 2, 1, -25, 40)) ==
            QueueCommandResult::accepted,
        "second world entity 2 command should be accepted");

    for (int tick = 0; tick < 20; ++tick) {
        static_cast<void>(first.advance());
        static_cast<void>(second.advance());
    }

    expect(
        first.current_tick() == second.current_tick(),
        "replayed worlds should reach the same tick");

    expect(
        first.snapshot() == second.snapshot(),
        "entity and command insertion order must not affect final state");
}

void test_failed_advance_preserves_state()
{
    World world{
        Microseconds{1'000'000},
        WorldLimits{
            .max_abs_position = Millimeters{10},
            .max_abs_velocity = MillimetersPerSecond{10},
        }};

    expect(
        world.add_entity(make_entity(1, 10, 0, 1, 0)) ==
            AddEntityResult::added,
        "boundary entity should be added");

    const auto tick_before = world.current_tick();
    const auto snapshot_before = world.snapshot();

    expect_throws<std::overflow_error>(
        [&world] {
            static_cast<void>(world.advance());
        },
        "advance beyond the world limit should fail");

    expect(
        world.current_tick() == tick_before,
        "failed advance must not mutate the clock");

    expect(
        world.snapshot() == snapshot_before,
        "failed advance must not mutate entity state");
}

} // namespace

int main()
{
    try {
        test_world_configuration();
        test_entity_admission();
        test_command_rejection();
        test_fractional_integration();
        test_scheduled_commands();
        test_stable_snapshot_order();
        test_replay_is_independent_of_insertion_order();
        test_failed_advance_preserves_state();
    } catch (const std::exception& error) {
        std::cerr << "world test failure: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
