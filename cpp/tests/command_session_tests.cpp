#include "tickline/command/command_session.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using tickline::command::ClientId;
using tickline::command::CommandAdmissionResult;
using tickline::command::CommandEnvelope;
using tickline::command::CommandRejectionCode;
using tickline::command::CommandSession;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::simulation::EntityId;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Velocity2;
using tickline::simulation::VelocityCommand;

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

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity = MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
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
            .entity_id = EntityId{3},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{25},
                .y = MillimetersPerSecond{-40},
            },
        },
    };
}

void expect_rejected(
    const CommandAdmissionResult& result,
    const CommandRejectionCode expected,
    const std::string_view message)
{
    expect(!result.accepted(), message);
    expect(result.code() == expected, message);

    expect(
        !result.command().has_value(),
        "rejected evaluation must not contain a simulation command");
}

void test_session_configuration()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandSession{
                    ClientId{0},
                    SessionId{11},
                });
        },
        "zero client identifier should be rejected");

    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandSession{
                    ClientId{7},
                    SessionId{0},
                });
        },
        "zero session identifier should be rejected");

    const CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect(
        session.client_id() == ClientId{7},
        "session should preserve the client identifier");

    expect(
        session.session_id() == SessionId{11},
        "session should preserve the session identifier");

    expect(
        session.highest_accepted_sequence() == 0,
        "new session should begin at sequence zero");
}

void test_evaluation_translates_without_committing()
{
    const CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    const auto result = session.evaluate(
        make_command(1, 101),
        100);

    expect(
        result.accepted(),
        "valid command should pass session evaluation");

    expect(
        result.command().has_value(),
        "successful evaluation should contain a simulation command");

    expect(
        result.command().value() ==
            VelocityCommand{
                .target_tick = 101,
                .entity_id = EntityId{3},
                .sequence = 1,
                .velocity = Velocity2{
                    .x = MillimetersPerSecond{25},
                    .y = MillimetersPerSecond{-40},
                },
            },
        "evaluation should translate all command fields");

    expect(
        session.highest_accepted_sequence() == 0,
        "evaluation alone must not mutate replay state");
}

void test_validation_rejection_is_non_mutating()
{
    const CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect_rejected(
        session.evaluate(
            make_command(1, 109),
            100),
        CommandRejectionCode::target_tick_too_far_future,
        "out-of-window command should be rejected");

    expect(
        session.highest_accepted_sequence() == 0,
        "rejected evaluation must not mutate replay state");
}

void test_identity_rejection_is_non_mutating()
{
    const CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect_rejected(
        session.evaluate(
            make_command(
                1,
                101,
                ClientId{8},
                SessionId{11}),
            100),
        CommandRejectionCode::client_identity_mismatch,
        "wrong client should be rejected");

    expect_rejected(
        session.evaluate(
            make_command(
                1,
                101,
                ClientId{7},
                SessionId{12}),
            100),
        CommandRejectionCode::session_identity_mismatch,
        "wrong session should be rejected");

    expect(
        session.highest_accepted_sequence() == 0,
        "identity rejection must not mutate replay state");
}

void test_admission_result_invariant()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandAdmissionResult::reject(
                    CommandRejectionCode::none));
        },
        "none must not construct a rejected result");
}

}

int main()
{
    try {
        test_session_configuration();
        test_evaluation_translates_without_committing();
        test_validation_rejection_is_non_mutating();
        test_identity_rejection_is_non_mutating();
        test_admission_result_invariant();
    } catch (const std::exception& error) {
        std::cerr
            << "command session test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
