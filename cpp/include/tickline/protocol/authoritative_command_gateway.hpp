#pragma once

#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/protocol/command_envelope_codec.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace tickline::protocol {

class ProtocolSubmissionResult final {
public:
    [[nodiscard]] static ProtocolSubmissionResult parse_rejection(
        ParseErrorCode code);

    [[nodiscard]] static ProtocolSubmissionResult command_submission(
        command::CommandSubmissionResult result);

    [[nodiscard]] bool parsed() const noexcept;

    [[nodiscard]] bool accepted() const noexcept;

    [[nodiscard]] ParseErrorCode parse_error() const noexcept;

    [[nodiscard]] const std::optional<command::CommandSubmissionResult>&
    command_result() const noexcept;

private:
    ProtocolSubmissionResult(
        ParseErrorCode parse_error,
        std::optional<command::CommandSubmissionResult> command_result);

    ParseErrorCode parse_error_;
    std::optional<command::CommandSubmissionResult> command_result_;
};

class AuthoritativeCommandGateway final {
public:
    explicit AuthoritativeCommandGateway(
        ProtocolLimits limits = {});

    [[nodiscard]] ProtocolSubmissionResult submit(
        command::CommandSession& session,
        simulation::World& world,
        std::span<const std::byte> bytes);

    [[nodiscard]] const ProtocolLimits& limits() const noexcept;

    [[nodiscard]] const command::CommandEvidenceLog&
    evidence() const noexcept;

private:
    ProtocolLimits limits_;
    command::AuthoritativeCommandPipeline pipeline_;
};

}
