#include "tickline/command/command_session.hpp"

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
    const SessionId session_id = SessionId{11},
    const std::uint64_t entity_id = 3,
    const std::int64_t velocity_x = 25,
    const std::int64_t velocity_y = -40)
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
            .entity_id = EntityId{entity_id},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{velocity_x},
                .y = MillimetersPerSecond{velocity_y},
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
        "rejected admission must not contain a simulation command");
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
        "zero session client identifier should be rejected");

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
        "session should preserve the client identity");

    expect(
        session.session_id() == SessionId{11},
        "session should preserve the session identity");

    expect(
        session.highest_accepted_sequence() == 0,
        "new session should begin without an accepted sequence");

    expect(
        session.policy().maximum_future_ticks == 8,
        "session should expose its validation policy");
}

void test_accepted_command_is_translated()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    const auto result = session.admit(
        make_command(1, 101),
        100);

    expect(
        result.accepted(),
        "valid session command should be admitted");

    expect(
        result.code() == CommandRejectionCode::none,
        "admitted command should use the none rejection code");

    expect(
        result.command().has_value(),
        "admitted command should contain a simulation command");

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
        "command envelope should translate without losing fields");

    expect(
        session.highest_accepted_sequence() == 1,
        "admission should advance the replay-protection sequence");
}

void test_duplicate_sequence_is_rejected()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect(
        session.admit(
                   make_command(5, 101),
                   100)
            .accepted(),
        "initial command should be admitted");

    const auto duplicate = session.admit(
        make_command(5, 102),
        100);

    expect_rejected(
        duplicate,
        CommandRejectionCode::duplicate_sequence,
        "duplicate sequence should be rejected");

    expect(
        session.highest_accepted_sequence() == 5,
        "duplicate rejection must not change sequence state");
}

void test_sequence_regression_is_rejected()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect(
        session.admit(
                   make_command(10, 101),
                   100)
            .accepted(),
        "initial command should be admitted");

    const auto regression = session.admit(
        make_command(9, 102),
        100);

    expect_rejected(
        regression,
        CommandRejectionCode::sequence_regression,
        "lower sequence should be rejected as regression");

    expect(
        session.highest_accepted_sequence() == 10,
        "regression rejection must not change sequence state");
}

void test_sequence_gaps_are_allowed()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    expect(
        session.admit(
                   make_command(1, 101),
                   100)
            .accepted(),
        "first command should be admitted");

    expect(
        session.admit(
                   make_command(100, 102),
                   100)
            .accepted(),
        "strictly increasing sequence gaps should be admitted");

    expect(
        session.highest_accepted_sequence() == 100,
        "highest sequence should track the admitted gap");
}

void test_stateless_rejection_does_not_consume_sequence()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    const auto too_far = session.admit(
        make_command(1, 109),
        100);

    expect_rejected(
        too_far,
        CommandRejectionCode::target_tick_too_far_future,
        "out-of-window command should be rejected");

    expect(
        session.highest_accepted_sequence() == 0,
        "validation rejection must not consume a sequence");

    const auto corrected = session.admit(
        make_command(1, 108),
        100);

    expect(
        corrected.accepted(),
        "corrected command may reuse a sequence that was never admitted");

    expect(
        session.highest_accepted_sequence() == 1,
        "corrected command should advance sequence state");
}

void test_identity_rejection_does_not_consume_sequence()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    const auto wrong_client = session.admit(
        make_command(
            1,
            101,
            ClientId{8},
            SessionId{11}),
        100);

    expect_rejected(
        wrong_client,
        CommandRejectionCode::client_identity_mismatch,
        "wrong client identity should be rejected");

    const auto wrong_session = session.admit(
        make_command(
            1,
            101,
            ClientId{7},
            SessionId{12}),
        100);

    expect_rejected(
        wrong_session,
        CommandRejectionCode::session_identity_mismatch,
        "wrong session identity should be rejected");

    expect(
        session.highest_accepted_sequence() == 0,
        "identity failures must not consume sequence state");
}

void test_sessions_have_independent_sequence_spaces()
{
    CommandSession first{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    CommandSession second{
        ClientId{7},
        SessionId{12},
        make_policy(),
    };

    expect(
        first.admit(
                 make_command(
                     1,
                     101,
                     ClientId{7},
                     SessionId{11}),
                 100)
            .accepted(),
        "first session should admit sequence one");

    expect(
        second.admit(
                  make_command(
                      1,
                      101,
                      ClientId{7},
                      SessionId{12}),
                  100)
            .accepted(),
        "second session should have an independent sequence space");

    expect(
        first.highest_accepted_sequence() == 1,
        "first session sequence should remain independent");

    expect(
        second.highest_accepted_sequence() == 1,
        "second session sequence should remain independent");
}

void test_maximum_sequence_is_supported()
{
    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    constexpr auto maximum =
        std::numeric_limits<std::uint64_t>::max();

    expect(
        session.admit(
                   make_command(maximum, 101),
                   100)
            .accepted(),
        "maximum sequence value should be admitted");

    expect(
        session.highest_accepted_sequence() == maximum,
        "maximum sequence should be recorded exactly");

    expect_rejected(
        session.admit(
            make_command(maximum, 102),
            100),
        CommandRejectionCode::duplicate_sequence,
        "replayed maximum sequence should be rejected");

    expect_rejected(
        session.admit(
            make_command(maximum - 1, 102),
            100),
        CommandRejectionCode::sequence_regression,
        "sequence below maximum should be a regression");
}

void test_admission_result_invariant()
{
    expect_throws<std::invalid_argument>(
        [] {
            static_cast<void>(
                CommandAdmissionResult::reject(
                    CommandRejectionCode::none));
        },
        "none rejection code must not create a rejected result");
}

void test_rejection_code_contract()
{
    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::duplicate_sequence) == 11);

    static_assert(
        static_cast<
            std::underlying_type_t<CommandRejectionCode>>(
            CommandRejectionCode::sequence_regression) == 12);

    expect(
        tickline::command::command_rejection_code_name(
            CommandRejectionCode::duplicate_sequence) ==
            "duplicate_sequence",
        "duplicate sequence code should have a stable name");

    expect(
        tickline::command::command_rejection_code_name(
            CommandRejectionCode::sequence_regression) ==
            "sequence_regression",
        "sequence regression code should have a stable name");
}

}

int main()
{
    try {
        test_session_configuration();
        test_accepted_command_is_translated();
        test_duplicate_sequence_is_rejected();
        test_sequence_regression_is_rejected();
        test_sequence_gaps_are_allowed();
        test_stateless_rejection_does_not_consume_sequence();
        test_identity_rejection_does_not_consume_sequence();
        test_sessions_have_independent_sequence_spaces();
        test_maximum_sequence_is_supported();
        test_admission_result_invariant();
        test_rejection_code_contract();
    } catch (const std::exception& error) {
        std::cerr
            << "command session test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
