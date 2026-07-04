#pragma once

#include "tickline/command/command_validator.hpp"
#include "tickline/simulation/world.hpp"

#include <cstdint>
#include <optional>

namespace tickline::command {

class AuthoritativeCommandPipeline;

class CommandAdmissionResult final {
public:
    [[nodiscard]] static CommandAdmissionResult accept(
        simulation::VelocityCommand command);

    [[nodiscard]] static CommandAdmissionResult reject(
        CommandRejectionCode code);

    [[nodiscard]] bool accepted() const noexcept;

    [[nodiscard]] CommandRejectionCode code() const noexcept;

    [[nodiscard]] const std::optional<simulation::VelocityCommand>&
    command() const noexcept;

private:
    CommandAdmissionResult(
        CommandRejectionCode code,
        std::optional<simulation::VelocityCommand> command);

    CommandRejectionCode code_;
    std::optional<simulation::VelocityCommand> command_;
};

class CommandSession final {
public:
    explicit CommandSession(
        ClientId client_id,
        SessionId session_id,
        CommandValidationPolicy policy = {});

    [[nodiscard]] ClientId client_id() const noexcept;

    [[nodiscard]] SessionId session_id() const noexcept;

    [[nodiscard]] std::uint64_t highest_accepted_sequence() const noexcept;

    [[nodiscard]] const CommandValidationPolicy& policy() const noexcept;

    [[nodiscard]] CommandAdmissionResult evaluate(
        const CommandEnvelope& command,
        std::uint64_t current_tick) const;

private:
    [[nodiscard]] static simulation::VelocityCommand translate(
        const CommandEnvelope& command);

    void commit_accepted_sequence(
        std::uint64_t sequence) noexcept;

    friend class AuthoritativeCommandPipeline;

    ClientId client_id_;
    SessionId session_id_;
    CommandValidator validator_;
    std::uint64_t highest_accepted_sequence_{0};
};

}
