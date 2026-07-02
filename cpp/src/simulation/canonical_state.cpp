#include "tickline/simulation/canonical_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tickline::simulation {
namespace {

void append_u16(
    std::vector<std::byte>& output,
    const std::uint16_t value)
{
    output.push_back(
        static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(
        static_cast<std::byte>(value & 0xffU));
}

void append_u64(
    std::vector<std::byte>& output,
    const std::uint64_t value)
{
    constexpr std::array<unsigned int, 8> shifts{
        56U,
        48U,
        40U,
        32U,
        24U,
        16U,
        8U,
        0U,
    };

    for (const auto shift : shifts) {
        output.push_back(
            static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

void append_i64(
    std::vector<std::byte>& output,
    const std::int64_t value)
{
    append_u64(output, static_cast<std::uint64_t>(value));
}

[[nodiscard]] std::uint64_t checked_size(
    const std::size_t value)
{
    if (!std::in_range<std::uint64_t>(value)) {
        throw std::overflow_error{
            "world state collection size cannot be encoded"};
    }

    return static_cast<std::uint64_t>(value);
}

} // namespace

std::vector<std::byte> encode_world_state(const World& world)
{
    constexpr std::array<std::byte, 4> magic{
        std::byte{'T'},
        std::byte{'L'},
        std::byte{'W'},
        std::byte{'S'},
    };

    std::vector<std::byte> output;
    output.insert(output.end(), magic.begin(), magic.end());

    append_u16(output, canonical_state_schema_version);
    append_u16(output, 0);

    const auto tick = world.clock_.current_tick();

    append_u64(output, tick.index);
    append_i64(output, tick.elapsed.count());
    append_i64(output, world.clock_.tick_duration().count());
    append_i64(output, world.limits_.max_abs_position.count());
    append_i64(output, world.limits_.max_abs_velocity.count());

    append_u64(output, checked_size(world.entities_.size()));

    for (const auto& [entity_id, stored] : world.entities_) {
        append_u64(output, entity_id.value());
        append_i64(output, stored.state.position.x.count());
        append_i64(output, stored.state.position.y.count());
        append_i64(output, stored.state.velocity.x.count());
        append_i64(output, stored.state.velocity.y.count());
        append_u64(output, stored.state.last_sequence);
        append_i64(output, stored.residual_x);
        append_i64(output, stored.residual_y);
        append_u64(
            output,
            world.highest_sequence_.at(entity_id));
        append_u64(
            output,
            world.latest_target_tick_.at(entity_id));
    }

    append_u64(
        output,
        checked_size(world.pending_commands_.size()));

    for (const auto& command : world.pending_commands_) {
        append_u64(output, command.target_tick);
        append_u64(output, command.entity_id.value());
        append_u64(output, command.sequence);
        append_i64(output, command.velocity.x.count());
        append_i64(output, command.velocity.y.count());
    }

    return output;
}

StateFingerprint fingerprint_world_state(const World& world)
{
    const auto bytes = encode_world_state(world);
    return fingerprint_bytes(bytes);
}

}
