#pragma once

#include "tickline/simulation/tick_clock.hpp"
#include "tickline/simulation/units.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tickline::simulation {

struct WorldLimits final {
    Millimeters max_abs_position{10'000'000'000};
    MillimetersPerSecond max_abs_velocity{1'000'000};
};

struct EntityState final {
    EntityId id;
    Position2 position;
    Velocity2 velocity;
    std::uint64_t last_sequence{0};

    friend constexpr bool operator==(
        const EntityState&,
        const EntityState&) noexcept = default;
};

struct VelocityCommand final {
    std::uint64_t target_tick;
    EntityId entity_id;
    std::uint64_t sequence;
    Velocity2 velocity;

    friend constexpr bool operator==(
        const VelocityCommand&,
        const VelocityCommand&) noexcept = default;
};

enum class AddEntityResult {
    added,
    duplicate_id,
    position_out_of_range,
    velocity_out_of_range,
};

enum class QueueCommandResult {
    accepted,
    stale_tick,
    unknown_entity,
    sequence_not_increasing,
    target_tick_regression,
    velocity_out_of_range,
};

class World final {
public:
    explicit World(
        Microseconds tick_duration,
        WorldLimits limits = {});

    [[nodiscard]] Tick current_tick() const noexcept;

    [[nodiscard]] Microseconds tick_duration() const noexcept;

    [[nodiscard]] const WorldLimits& limits() const noexcept;

    [[nodiscard]] AddEntityResult add_entity(EntityState entity);

    [[nodiscard]] QueueCommandResult queue_velocity(
        VelocityCommand command);

    [[nodiscard]] Tick advance();

    [[nodiscard]] std::optional<EntityState> entity(
        EntityId entity_id) const;

    [[nodiscard]] std::vector<EntityState> snapshot() const;

    [[nodiscard]] std::size_t entity_count() const noexcept;

    [[nodiscard]] std::size_t pending_command_count() const noexcept;

private:
    struct StoredEntity final {
        EntityState state;
        std::int64_t residual_x{0};
        std::int64_t residual_y{0};
    };

    [[nodiscard]] bool position_is_valid(
        const Position2& position) const noexcept;

    [[nodiscard]] bool velocity_is_valid(
        const Velocity2& velocity) const noexcept;

    friend std::vector<std::byte> encode_world_state(
        const World& world);

    FixedStepClock clock_;
    WorldLimits limits_;
    std::map<EntityId, StoredEntity> entities_;
    std::map<EntityId, std::uint64_t> highest_sequence_;
    std::map<EntityId, std::uint64_t> latest_target_tick_;
    std::vector<VelocityCommand> pending_commands_;
};

}
