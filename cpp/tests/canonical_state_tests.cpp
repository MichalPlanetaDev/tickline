#include "tickline/simulation/canonical_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
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

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

[[nodiscard]] std::string bytes_to_hex(
    const std::span<const std::byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    std::string output;
    output.reserve(bytes.size() * 2);

    for (const auto byte : bytes) {
        const auto value = std::to_integer<std::uint8_t>(byte);

        output.push_back(
            digits[static_cast<std::size_t>(value >> 4U)]);

        output.push_back(
            digits[static_cast<std::size_t>(value & 0x0fU)]);
    }

    return output;
}

[[nodiscard]] EntityState make_entity(
    const std::uint64_t id,
    const std::int64_t position_x,
    const std::int64_t position_y)
{
    return EntityState{
        .id = EntityId{id},
        .position = Position2{
            .x = Millimeters{position_x},
            .y = Millimeters{position_y},
        },
        .velocity = Velocity2{
            .x = MillimetersPerSecond{0},
            .y = MillimetersPerSecond{0},
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

void test_byte_fingerprint_reference_value()
{
    constexpr std::array<std::byte, 3> input{
        std::byte{'a'},
        std::byte{'b'},
        std::byte{'c'},
    };

    const auto fingerprint =
        tickline::simulation::fingerprint_bytes(input);

    expect(
        fingerprint.to_hex() == "e71fa2190541574b",
        "FNV-1a reference fingerprint should remain stable");
}

void test_empty_world_exact_layout()
{
    const World world{Microseconds{100'000}};

    const auto encoded =
        tickline::simulation::encode_world_state(world);

    const std::string expected{
        "544c575300010000"
        "0000000000000000"
        "0000000000000000"
        "00000000000186a0"
        "00000002540be400"
        "00000000000f4240"
        "0000000000000000"
        "0000000000000000"};

    expect(
        encoded.size() == 64,
        "empty world encoding should be 64 bytes");

    expect(
        bytes_to_hex(encoded) == expected,
        "empty world encoding must match the canonical byte layout");

    expect(
        tickline::simulation::fingerprint_world_state(world).to_hex() ==
            "bffe210b0983669d",
        "empty world fingerprint must remain stable");
}

void test_insertion_order_does_not_change_encoding()
{
    World first{Microseconds{100'000}};
    World second{Microseconds{100'000}};

    expect(
        first.add_entity(make_entity(2, 100, -100)) ==
            AddEntityResult::added,
        "first world entity 2 should be added");

    expect(
        first.add_entity(make_entity(1, -100, 100)) ==
            AddEntityResult::added,
        "first world entity 1 should be added");

    expect(
        second.add_entity(make_entity(1, -100, 100)) ==
            AddEntityResult::added,
        "second world entity 1 should be added");

    expect(
        second.add_entity(make_entity(2, 100, -100)) ==
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

    expect(
        tickline::simulation::encode_world_state(first) ==
            tickline::simulation::encode_world_state(second),
        "canonical encoding must not depend on insertion order");

    expect(
        tickline::simulation::fingerprint_world_state(first) ==
            tickline::simulation::fingerprint_world_state(second),
        "canonical fingerprints must not depend on insertion order");
}

void test_pending_commands_are_encoded()
{
    World without_command{Microseconds{100'000}};
    World with_command{Microseconds{100'000}};

    expect(
        without_command.add_entity(make_entity(1, 0, 0)) ==
            AddEntityResult::added,
        "baseline entity should be added");

    expect(
        with_command.add_entity(make_entity(1, 0, 0)) ==
            AddEntityResult::added,
        "command world entity should be added");

    expect(
        with_command.queue_velocity(make_command(3, 1, 1, 25, 0)) ==
            QueueCommandResult::accepted,
        "future command should be accepted");

    expect(
        without_command.snapshot() == with_command.snapshot(),
        "public entity state should still match before command execution");

    expect(
        tickline::simulation::fingerprint_world_state(without_command) !=
            tickline::simulation::fingerprint_world_state(with_command),
        "pending command state must affect the canonical fingerprint");
}

void test_replay_fixture_has_stable_fingerprint()
{
    World world{Microseconds{100'000}};

    expect(
        world.add_entity(make_entity(2, 100, -100)) ==
            AddEntityResult::added,
        "entity 2 should be added");

    expect(
        world.add_entity(make_entity(1, -100, 100)) ==
            AddEntityResult::added,
        "entity 1 should be added");

    expect(
        world.queue_velocity(make_command(2, 2, 1, -25, 40)) ==
            QueueCommandResult::accepted,
        "entity 2 command should be accepted");

    expect(
        world.queue_velocity(make_command(1, 1, 1, 30, -20)) ==
            QueueCommandResult::accepted,
        "entity 1 command should be accepted");

    for (int tick = 0; tick < 20; ++tick) {
        static_cast<void>(world.advance());
    }

    expect(
        tickline::simulation::fingerprint_world_state(world).to_hex() ==
            "f43233d01de9f030",
        "replay fixture fingerprint must remain stable");
}

} // namespace

int main()
{
    try {
        test_byte_fingerprint_reference_value();
        test_empty_world_exact_layout();
        test_insertion_order_does_not_change_encoding();
        test_pending_commands_are_encoded();
        test_replay_fixture_has_stable_fingerprint();
    } catch (const std::exception& error) {
        std::cerr
            << "canonical state test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
