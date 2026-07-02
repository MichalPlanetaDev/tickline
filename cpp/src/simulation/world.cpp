#include "tickline/simulation/world.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tickline::simulation {
namespace {

constexpr std::int64_t microseconds_per_second = 1'000'000;

[[nodiscard]] bool within_symmetric_limit(
    const std::int64_t value,
    const std::int64_t limit) noexcept
{
    return value >= -limit && value <= limit;
}

[[nodiscard]] std::int64_t checked_add(
    const std::int64_t left,
    const std::int64_t right)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();

    if (right > 0 && left > maximum - right) {
        throw std::overflow_error{"signed position addition overflow"};
    }

    if (right < 0 && left < minimum - right) {
        throw std::overflow_error{"signed position addition overflow"};
    }

    return left + right;
}

struct AxisIntegration final {
    std::int64_t position;
    std::int64_t residual;
};

[[nodiscard]] AxisIntegration integrate_axis(
    const std::int64_t position,
    const std::int64_t velocity,
    const std::int64_t residual,
    const std::int64_t tick_duration,
    const std::int64_t max_abs_position)
{
    const auto scaled_displacement =
        velocity * tick_duration + residual;

    const auto whole_millimeters =
        scaled_displacement / microseconds_per_second;

    const auto next_residual =
        scaled_displacement % microseconds_per_second;

    const auto next_position =
        checked_add(position, whole_millimeters);

    if (!within_symmetric_limit(
            next_position,
            max_abs_position)) {
        throw std::overflow_error{
            "integrated position exceeds configured world limit"};
    }

    return AxisIntegration{
        .position = next_position,
        .residual = next_residual,
    };
}

[[nodiscard]] bool command_order(
    const VelocityCommand& left,
    const VelocityCommand& right) noexcept
{
    if (left.target_tick != right.target_tick) {
        return left.target_tick < right.target_tick;
    }

    if (left.entity_id != right.entity_id) {
        return left.entity_id < right.entity_id;
    }

    return left.sequence < right.sequence;
}

} // namespace

World::World(
    const Microseconds tick_duration,
    const WorldLimits limits)
    : clock_{tick_duration},
      limits_{limits}
{
    const auto position_limit =
        limits_.max_abs_position.count();

    const auto velocity_limit =
        limits_.max_abs_velocity.count();

    if (position_limit <= 0) {
        throw std::invalid_argument{
            "maximum absolute position must be positive"};
    }

    if (velocity_limit <= 0) {
        throw std::invalid_argument{
            "maximum absolute velocity must be positive"};
    }

    const auto duration = tick_duration.count();
    constexpr auto maximum =
        std::numeric_limits<std::int64_t>::max();

    const auto safe_velocity_limit =
        (maximum - (microseconds_per_second - 1)) / duration;

    if (velocity_limit > safe_velocity_limit) {
        throw std::invalid_argument{
            "velocity limit and tick duration can overflow integration"};
    }
}

Tick World::current_tick() const noexcept
{
    return clock_.current_tick();
}

Microseconds World::tick_duration() const noexcept
{
    return clock_.tick_duration();
}

const WorldLimits& World::limits() const noexcept
{
    return limits_;
}

AddEntityResult World::add_entity(EntityState entity)
{
    if (entities_.contains(entity.id)) {
        return AddEntityResult::duplicate_id;
    }

    if (!position_is_valid(entity.position)) {
        return AddEntityResult::position_out_of_range;
    }

    if (!velocity_is_valid(entity.velocity)) {
        return AddEntityResult::velocity_out_of_range;
    }

    entity.last_sequence = 0;

    const auto entity_id = entity.id;

    entities_.emplace(
        entity_id,
        StoredEntity{
            .state = std::move(entity),
            .residual_x = 0,
            .residual_y = 0,
        });

    highest_sequence_.emplace(entity_id, 0);
    latest_target_tick_.emplace(
        entity_id,
        current_tick().index);

    return AddEntityResult::added;
}

QueueCommandResult World::queue_velocity(
    VelocityCommand command)
{
    const auto current_index = current_tick().index;

    if (command.target_tick <= current_index) {
        return QueueCommandResult::stale_tick;
    }

    if (!entities_.contains(command.entity_id)) {
        return QueueCommandResult::unknown_entity;
    }

    const auto highest_sequence =
        highest_sequence_.at(command.entity_id);

    if (command.sequence <= highest_sequence) {
        return QueueCommandResult::sequence_not_increasing;
    }

    const auto latest_target_tick =
        latest_target_tick_.at(command.entity_id);

    if (command.target_tick < latest_target_tick) {
        return QueueCommandResult::target_tick_regression;
    }

    if (!velocity_is_valid(command.velocity)) {
        return QueueCommandResult::velocity_out_of_range;
    }

    highest_sequence_.at(command.entity_id) =
        command.sequence;

    latest_target_tick_.at(command.entity_id) =
        command.target_tick;

    pending_commands_.push_back(std::move(command));

    std::sort(
        pending_commands_.begin(),
        pending_commands_.end(),
        command_order);

    return QueueCommandResult::accepted;
}

Tick World::advance()
{
    auto next_clock = clock_;
    const auto next_tick = next_clock.advance();

    auto next_entities = entities_;

    for (const auto& command : pending_commands_) {
        if (command.target_tick < next_tick.index) {
            throw std::logic_error{
                "pending command became stale inside the simulation"};
        }

        if (command.target_tick > next_tick.index) {
            break;
        }

        auto& stored =
            next_entities.at(command.entity_id);

        stored.state.velocity = command.velocity;
        stored.state.last_sequence = command.sequence;
    }

    const auto duration = tick_duration().count();
    const auto position_limit =
        limits_.max_abs_position.count();

    for (auto& [entity_id, stored] : next_entities) {
        static_cast<void>(entity_id);

        const auto integrated_x = integrate_axis(
            stored.state.position.x.count(),
            stored.state.velocity.x.count(),
            stored.residual_x,
            duration,
            position_limit);

        const auto integrated_y = integrate_axis(
            stored.state.position.y.count(),
            stored.state.velocity.y.count(),
            stored.residual_y,
            duration,
            position_limit);

        stored.state.position = Position2{
            .x = Millimeters{integrated_x.position},
            .y = Millimeters{integrated_y.position},
        };

        stored.residual_x = integrated_x.residual;
        stored.residual_y = integrated_y.residual;
    }

    clock_ = next_clock;
    entities_ = std::move(next_entities);

    const auto first_unprocessed = std::remove_if(
        pending_commands_.begin(),
        pending_commands_.end(),
        [next_index = next_tick.index](
            const VelocityCommand& command) {
            return command.target_tick == next_index;
        });

    pending_commands_.erase(
        first_unprocessed,
        pending_commands_.end());

    return next_tick;
}

std::optional<EntityState> World::entity(
    const EntityId entity_id) const
{
    const auto iterator = entities_.find(entity_id);

    if (iterator == entities_.end()) {
        return std::nullopt;
    }

    return iterator->second.state;
}

std::vector<EntityState> World::snapshot() const
{
    std::vector<EntityState> result;
    result.reserve(entities_.size());

    for (const auto& [entity_id, stored] : entities_) {
        static_cast<void>(entity_id);
        result.push_back(stored.state);
    }

    return result;
}

std::size_t World::entity_count() const noexcept
{
    return entities_.size();
}

std::size_t World::pending_command_count() const noexcept
{
    return pending_commands_.size();
}

bool World::position_is_valid(
    const Position2& position) const noexcept
{
    const auto limit = limits_.max_abs_position.count();

    return within_symmetric_limit(
               position.x.count(),
               limit) &&
           within_symmetric_limit(
               position.y.count(),
               limit);
}

bool World::velocity_is_valid(
    const Velocity2& velocity) const noexcept
{
    const auto limit = limits_.max_abs_velocity.count();

    return within_symmetric_limit(
               velocity.x.count(),
               limit) &&
           within_symmetric_limit(
               velocity.y.count(),
               limit);
}

}
