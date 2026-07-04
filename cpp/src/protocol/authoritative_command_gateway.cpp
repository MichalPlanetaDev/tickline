#include "tickline/protocol/authoritative_command_gateway.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace tickline::protocol {

ProtocolSubmissionResult ProtocolSubmissionResult::parse_rejection(
    const ParseErrorCode code)
{
    if (code == ParseErrorCode::none) {
        throw std::invalid_argument{
            "protocol rejection requires a parse error"};
    }

    return ProtocolSubmissionResult{
        code,
        std::nullopt,
    };
}

ProtocolSubmissionResult ProtocolSubmissionResult::command_submission(
    command::CommandSubmissionResult result)
{
    return ProtocolSubmissionResult{
        ParseErrorCode::none,
        std::optional<command::CommandSubmissionResult>{
            std::move(result)},
    };
}

bool ProtocolSubmissionResult::parsed() const noexcept
{
    return command_result_.has_value();
}

bool ProtocolSubmissionResult::accepted() const noexcept
{
    return command_result_.has_value() &&
           command_result_->accepted();
}

ParseErrorCode ProtocolSubmissionResult::parse_error() const noexcept
{
    return parse_error_;
}

const std::optional<command::CommandSubmissionResult>&
ProtocolSubmissionResult::command_result() const noexcept
{
    return command_result_;
}

ProtocolSubmissionResult::ProtocolSubmissionResult(
    const ParseErrorCode parse_error,
    std::optional<command::CommandSubmissionResult> command_result)
    : parse_error_{parse_error},
      command_result_{std::move(command_result)}
{
    const auto parsed = command_result_.has_value();
    const auto has_parse_error =
        parse_error_ != ParseErrorCode::none;

    if (parsed == has_parse_error) {
        throw std::invalid_argument{
            "protocol submission result has inconsistent state"};
    }
}

AuthoritativeCommandGateway::AuthoritativeCommandGateway(
    const ProtocolLimits limits)
    : limits_{limits}
{
    if (limits_.maximum_frame_size < frame_header_size) {
        throw std::invalid_argument{
            "maximum frame size must include the fixed frame header"};
    }
}

ProtocolSubmissionResult AuthoritativeCommandGateway::submit(
    command::CommandSession& session,
    simulation::World& world,
    const std::span<const std::byte> bytes)
{
    const auto envelope_result =
        decode_command_frame(bytes, limits_);

    if (!envelope_result.has_value()) {
        return ProtocolSubmissionResult::parse_rejection(
            envelope_result.error());
    }

    return ProtocolSubmissionResult::command_submission(
        pipeline_.submit(
            session,
            world,
            envelope_result.value()));
}

const ProtocolLimits&
AuthoritativeCommandGateway::limits() const noexcept
{
    return limits_;
}

const command::CommandEvidenceLog&
AuthoritativeCommandGateway::evidence() const noexcept
{
    return pipeline_.evidence();
}

}
