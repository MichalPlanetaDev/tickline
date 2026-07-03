#include "tickline/command/command_validator.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandRejectionCode;
using tickline::command::CommandType;
using tickline::command::CommandValidationContext;
using tickline::command::CommandValidationPolicy;
using tickline::command::CommandValidator;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::simulation::EntityId;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Velocity2;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{
            std::string{message}};
    }
}

template <typename ExpectedException, typename Function>
void expect_throws(
    Function function,
    const std::string_view message)
{
    try {
        function();
    } catch (const ExpectedException&) {
        return;
    }

    throw std::runtime_error{
        std::string{message}};
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t target_tick = 101,
    const std::uint64_t sequence = 1,
    const std::int64_t velocity_x = 25,
    const std::int64_t velocity_y = -25)
{
    return CommandEnvelope{
        .schema_version =
            tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = sequence,
        .target_tick = target_tick,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{3},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{velocity_x},
                .y = MillimetersPerSecond{velocity_y},
            },
        },
    };
}

[[nodiscard]] CommandValidationContext make_context(
    const std::uint64_t current_tick = 100)
{
    return CommandValidationContext{
        .authenticated_client_id = ClientId{7},
        .active_session_id = SessionId{11},
        .current_tick = current_tick,
    };
}

void expect_code(
    const CommandValidator& validator,
    const CommandEnvelope& command,
    const CommandValidationContext& context,
    const CommandRejectionCode expected,
    const std::string_view message)
{
    const auto result =
        validator.validate(command, context);

    expect(result.code == expected, message);

    expect(
        result.accepted() ==
            (expected == CommandRejectionCode::none),
        "accepted state must match the rejection code");
}

void test_identifier_values()
{
    constexpr ClientId invalid_client{0};
    constexpr ClientId first_client{1};
    constexpr ClientId second_client{2};

    constexpr SessionId invalid_session{0};
    constexpr SessionId session{9};

    static_assert(!invalid_client.valid());
    static_assert(first_client.valid());
    static_assert(first_client < second_client);

    static_assert(!invalid_session.valid());
    static_assert(session.valid());

    expect(
        first_client.value() == 1,
        "client ID should preserve its value");

    expect(
        session.value() == 9,
        "session ID should preserve its value");
}

void test_policy_validation()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandValidator{
                    CommandValidationPolicy{
                        .maximum_future_ticks = 0,
                        .max_abs_velocity =
                            MillimetersPerSecond{100},
                    }});
        },
        "zero future window should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandValidator{
                    CommandValidationPolicy{
                        .maximum_future_ticks = 4,
                        .max_abs_velocity =
                            MillimetersPerSecond{0},
                    }});
        },
        "zero velocity limit should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandValidator{
                    CommandValidationPolicy{
                        .maximum_future_ticks = 4,
                        .max_abs_velocity =
                            MillimetersPerSecond{-1},
                    }});
        },
        "negative velocity limit should be rejected");
}

void test_valid_command_is_accepted()
{
    const CommandValidator validator{
        CommandValidationPolicy{
            .maximum_future_ticks = 4,
            .max_abs_velocity =
                MillimetersPerSecond{100},
        }};

    expect_code(
        validator,
        make_command(),
        make_context(),
        CommandRejectionCode::none,
        "valid command should be accepted");
}

void test_schema_and_type_validation_order()
{
    const CommandValidator validator;

    auto command = make_command();

    command.schema_version = 999;
    command.type =
        static_cast<CommandType>(999);

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::unsupported_schema_version,
        "schema validation should run before command-type validation");

    command.schema_version =
        tickline::command::command_schema_version;

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::unsupported_command_type,
        "unknown command type should be rejected");
}

void test_identity_validation()
{
    const CommandValidator validator;

    auto command = make_command();
    command.client_id = ClientId{0};

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::invalid_client_id,
        "zero client ID should be rejected");

    command = make_command();
    command.session_id = SessionId{0};

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::invalid_session_id,
        "zero session ID should be rejected");

    command = make_command();
    command.client_id = ClientId{8};

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::client_identity_mismatch,
        "client identity mismatch should be rejected");

    command = make_command();
    command.session_id = SessionId{12};

    expect_code(
        validator,
        command,
        make_context(),
        CommandRejectionCode::session_identity_mismatch,
        "session identity mismatch should be rejected");
}

void test_sequence_and_tick_validation()
{
    const CommandValidator validator{
        CommandValidationPolicy{
            .maximum_future_ticks = 4,
            .max_abs_velocity =
                MillimetersPerSecond{100},
        }};

    expect_code(
        validator,
        make_command(101, 0),
        make_context(),
        CommandRejectionCode::sequence_zero,
        "zero sequence should be rejected");

    expect_code(
        validator,
        make_command(100),
        make_context(),
        CommandRejectionCode::stale_target_tick,
        "current tick should be stale");

    expect_code(
        validator,
        make_command(99),
        make_context(),
        CommandRejectionCode::stale_target_tick,
        "past tick should be stale");

    expect_code(
        validator,
        make_command(104),
        make_context(),
        CommandRejectionCode::none,
        "future-window boundary should be accepted");

    expect_code(
        validator,
        make_command(105),
        make_context(),
        CommandRejectionCode::target_tick_too_far_future,
        "command beyond future window should be rejected");

    constexpr auto maximum =
        std::numeric_limits<std::uint64_t>::max();

    expect_code(
        validator,
        make_command(maximum),
        make_context(maximum - 4),
        CommandRejectionCode::none,
        "maximum tick should be validated without unsigned overflow");
}

void test_velocity_validation()
{
    const CommandValidator validator{
        CommandValidationPolicy{
            .maximum_future_ticks = 4,
            .max_abs_velocity =
                MillimetersPerSecond{100},
        }};

    expect_code(
        validator,
        make_command(101, 1, 100, -100),
        make_context(),
        CommandRejectionCode::none,
        "velocity limits should be inclusive");

    expect_code(
        validator,
        make_command(101, 1, 101, 0),
        make_context(),
        CommandRejectionCode::velocity_out_of_range,
        "positive out-of-range velocity should be rejected");

    expect_code(
        validator,
        make_command(101, 1, -101, 0),
        make_context(),
        CommandRejectionCode::velocity_out_of_range,
        "negative out-of-range velocity should be rejected");

    expect_code(
        validator,
        make_command(
            101,
            1,
            std::numeric_limits<std::int64_t>::min(),
            0),
        make_context(),
        CommandRejectionCode::velocity_out_of_range,
        "minimum signed velocity should be rejected safely");
}

void test_invalid_authoritative_context_is_rejected()
{
    const CommandValidator validator;

    expect_throws<std::invalid_argument>(
        [&validator] {
            static_cast<void>(
                validator.validate(
                    make_command(),
                    CommandValidationContext{
                        .authenticated_client_id =
                            ClientId{0},
                        .active_session_id =
                            SessionId{11},
                        .current_tick = 100,
                    }));
        },
        "invalid authenticated client context should throw");

    expect_throws<std::invalid_argument>(
        [&validator] {
            static_cast<void>(
                validator.validate(
                    make_command(),
                    CommandValidationContext{
                        .authenticated_client_id =
                            ClientId{7},
                        .active_session_id =
                            SessionId{0},
                        .current_tick = 100,
                    }));
        },
        "invalid active session context should throw");
}

void test_rejection_code_contract()
{
    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::none) == 0);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::unsupported_schema_version) ==
        1);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::velocity_out_of_range) ==
        10);

    expect(
        tickline::command::command_rejection_code_name(
            CommandRejectionCode::client_identity_mismatch) ==
            "client_identity_mismatch",
        "rejection code should expose a stable machine name");

    expect(
        tickline::command::command_rejection_code_name(
            static_cast<CommandRejectionCode>(999)) ==
            "unknown",
        "unknown rejection code should have a stable fallback name");
}

}

int main()
{
    try {
        test_identifier_values();
        test_policy_validation();
        test_valid_command_is_accepted();
        test_schema_and_type_validation_order();
        test_identity_validation();
        test_sequence_and_tick_validation();
        test_velocity_validation();
        test_invalid_authoritative_context_is_rejected();
        test_rejection_code_contract();
    } catch (const std::exception& error) {
        std::cerr
            << "command validator test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
