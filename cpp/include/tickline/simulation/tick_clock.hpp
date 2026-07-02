#pragma once

#include "tickline/simulation/units.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tickline::simulation {

struct Tick final {
    std::uint64_t index;
    Microseconds elapsed;

    friend constexpr bool operator==(
        const Tick&,
        const Tick&) noexcept = default;
};

class FixedStepClock final {
public:
    explicit FixedStepClock(const Microseconds tick_duration)
        : tick_duration_{tick_duration}
    {
        if (tick_duration_.count() <= 0) {
            throw std::invalid_argument{
                "tick duration must be greater than zero"};
        }
    }

    [[nodiscard]] constexpr Microseconds tick_duration() const noexcept
    {
        return tick_duration_;
    }

    [[nodiscard]] constexpr Tick current_tick() const noexcept
    {
        return Tick{
            .index = tick_index_,
            .elapsed = elapsed_,
        };
    }

    [[nodiscard]] Tick advance()
    {
        if (tick_index_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error{"tick index overflow"};
        }

        const auto duration = tick_duration_.count();
        const auto elapsed = elapsed_.count();
        const auto maximum = std::numeric_limits<std::int64_t>::max();

        if (elapsed > maximum - duration) {
            throw std::overflow_error{"simulation elapsed time overflow"};
        }

        ++tick_index_;
        elapsed_ = Microseconds{elapsed + duration};

        return current_tick();
    }

private:
    Microseconds tick_duration_;
    std::uint64_t tick_index_{0};
    Microseconds elapsed_{Microseconds::zero()};
};

}
