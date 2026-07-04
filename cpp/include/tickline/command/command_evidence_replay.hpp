#pragma once

#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/command/command_evidence_archive.hpp"
#include "tickline/command/command_session.hpp"
#include "tickline/security/sha256.hpp"
#include "tickline/simulation/state_fingerprint.hpp"
#include "tickline/simulation/world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace tickline::command {

enum class CommandEvidenceReplayCode : std::uint16_t {
    verified = 0,
    chain_verification_failed = 1,
    observed_tick_regression = 2,
    tick_advance_limit_exceeded = 3,
    simulation_advance_failed = 4,
    session_state_mismatch = 5,
    pending_command_count_mismatch = 6,
    world_state_mismatch = 7,
    submission_record_mismatch = 8,
};

[[nodiscard]] constexpr std::string_view
command_evidence_replay_code_name(
    const CommandEvidenceReplayCode code) noexcept
{
    switch (code) {
    case CommandEvidenceReplayCode::verified:
        return "verified";

    case CommandEvidenceReplayCode::chain_verification_failed:
        return "chain_verification_failed";

    case CommandEvidenceReplayCode::observed_tick_regression:
        return "observed_tick_regression";

    case CommandEvidenceReplayCode::tick_advance_limit_exceeded:
        return "tick_advance_limit_exceeded";

    case CommandEvidenceReplayCode::simulation_advance_failed:
        return "simulation_advance_failed";

    case CommandEvidenceReplayCode::session_state_mismatch:
        return "session_state_mismatch";

    case CommandEvidenceReplayCode::pending_command_count_mismatch:
        return "pending_command_count_mismatch";

    case CommandEvidenceReplayCode::world_state_mismatch:
        return "world_state_mismatch";

    case CommandEvidenceReplayCode::submission_record_mismatch:
        return "submission_record_mismatch";
    }

    return "unknown";
}

struct CommandEvidenceReplayPolicy final {
    std::uint64_t maximum_tick_advances{1'000'000};
};

struct CommandEvidenceReplayResult final {
    CommandEvidenceReplayCode code;
    std::size_t processed_records;
    std::optional<std::uint64_t> failed_ordinal;
    std::uint64_t performed_tick_advances;
    std::uint64_t final_tick;
    std::uint64_t final_session_sequence;
    std::size_t final_pending_command_count;
    simulation::StateFingerprint final_world_fingerprint;
    security::Sha256Digest replayed_head_digest;

    [[nodiscard]] constexpr bool verified() const noexcept
    {
        return code == CommandEvidenceReplayCode::verified;
    }
};

class CommandEvidenceReplayer final {
public:
    explicit constexpr CommandEvidenceReplayer(
        const CommandEvidenceReplayPolicy policy = {}) noexcept
        : policy_{policy}
    {
    }

    [[nodiscard]] constexpr const CommandEvidenceReplayPolicy&
    policy() const noexcept
    {
        return policy_;
    }

    [[nodiscard]] CommandEvidenceReplayResult replay(
        const CommandSession& initial_session,
        const simulation::World& initial_world,
        std::span<const CommandEvidenceRecord> records,
        security::Sha256Digest trusted_head) const;

    [[nodiscard]] CommandEvidenceReplayResult replay(
        const CommandSession& initial_session,
        const simulation::World& initial_world,
        const CommandEvidenceArchive& archive) const;

private:
    CommandEvidenceReplayPolicy policy_;
};

}
