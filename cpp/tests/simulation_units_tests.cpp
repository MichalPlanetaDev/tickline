#include "tickline/simulation/tick_clock.hpp"
#include "tickline/simulation/units.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

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

void test_microseconds_conversion()
{
    const auto duration =
        tickline::simulation::Microseconds::from_milliseconds(16);

    expect(
        duration.count() == 16'000,
        "16 milliseconds should equal 16000 microseconds");
}

void test_microseconds_conversion_boundaries()
{
    constexpr auto maximum =
        std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum =
        std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t scale = 1'000;

    expect_throws<std::overflow_error>(
        [] {
            constexpr auto value = maximum / scale + 1;
            static_cast<void>(
                tickline::simulation::Microseconds::from_milliseconds(
                    value));
        },
        "positive millisecond overflow should be rejected");

    expect_throws<std::overflow_error>(
        [] {
            constexpr auto value = minimum / scale - 1;
            static_cast<void>(
                tickline::simulation::Microseconds::from_milliseconds(
                    value));
        },
        "negative millisecond overflow should be rejected");
}

void test_entity_identifier()
{
    const tickline::simulation::EntityId first{1};
    const tickline::simulation::EntityId second{2};

    expect(first.value() == 1, "entity identifier should preserve its value");
    expect(first < second, "entity identifiers should have stable ordering");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(tickline::simulation::EntityId{0});
        },
        "zero entity identifier should be rejected");
}

void test_position_and_velocity_values()
{
    const tickline::simulation::Position2 position{
        .x = tickline::simulation::Millimeters{1'250},
        .y = tickline::simulation::Millimeters{-750},
    };

    const tickline::simulation::Velocity2 velocity{
        .x = tickline::simulation::MillimetersPerSecond{4'000},
        .y = tickline::simulation::MillimetersPerSecond{-2'000},
    };

    expect(position.x.count() == 1'250, "position X should preserve units");
    expect(position.y.count() == -750, "position Y should preserve units");
    expect(
        velocity.x.count() == 4'000,
        "velocity X should preserve units");
    expect(
        velocity.y.count() == -2'000,
        "velocity Y should preserve units");
}

void test_fixed_step_clock_configuration()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(tickline::simulation::FixedStepClock{
                tickline::simulation::Microseconds{0}});
        },
        "zero tick duration should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(tickline::simulation::FixedStepClock{
                tickline::simulation::Microseconds{-1}});
        },
        "negative tick duration should be rejected");
}

void test_fixed_step_clock_progression()
{
    tickline::simulation::FixedStepClock clock{
        tickline::simulation::Microseconds::from_milliseconds(20)};

    expect(
        clock.current_tick() == tickline::simulation::Tick{
                                    .index = 0,
                                    .elapsed =
                                        tickline::simulation::Microseconds{
                                            0}},
        "new clock should begin at tick zero");

    const auto first = clock.advance();
    const auto second = clock.advance();

    expect(
        first == tickline::simulation::Tick{
                     .index = 1,
                     .elapsed =
                         tickline::simulation::Microseconds{20'000}},
        "first tick should advance by one fixed duration");

    expect(
        second == tickline::simulation::Tick{
                      .index = 2,
                      .elapsed =
                          tickline::simulation::Microseconds{40'000}},
        "second tick should preserve deterministic elapsed time");
}

} // namespace

int main()
{
    try {
        test_microseconds_conversion();
        test_microseconds_conversion_boundaries();
        test_entity_identifier();
        test_position_and_velocity_values();
        test_fixed_step_clock_configuration();
        test_fixed_step_clock_progression();
    } catch (const std::exception& error) {
        std::cerr << "simulation unit test failure: "
                  << error.what() << '\n';
        return 1;
    }

    return 0;
}
