#pragma once

#include "tickline/simulation/state_fingerprint.hpp"
#include "tickline/simulation/world.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tickline::simulation {

inline constexpr std::uint16_t canonical_state_schema_version = 1;

[[nodiscard]] std::vector<std::byte> encode_world_state(
    const World& world);

[[nodiscard]] StateFingerprint fingerprint_world_state(
    const World& world);

}
