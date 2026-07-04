#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/command/command_evidence.hpp"
#include "tickline/security/sha256.hpp"
#include "tickline/simulation/canonical_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using tickline::command::AuthoritativeCommandPipeline;
using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandEvidenceEntry;
using tickline::command::CommandEvidenceRecord;
using tickline::command::CommandQueueOutcome;
using tickline::command::CommandRejectionCode;
using tickline::command::CommandSession;
using tickline::command::CommandType;
using tickline::command::CommandValidationPolicy;
using tickline::command::SessionId;
using tickline::command::SetVelocityPayload;
using tickline::security::Sha256Digest;
using tickline::simulation::AddEntityResult;
using tickline::simulation::EntityId;
using tickline::simulation::EntityState;
using tickline::simulation::Microseconds;
using tickline::simulation::Millimeters;
using tickline::simulation::MillimetersPerSecond;
using tickline::simulation::Position2;
using tickline::simulation::StateFingerprint;
using tickline::simulation::Velocity2;
using tickline::simulation::World;
using tickline::simulation::WorldLimits;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{
            std::string{message}};
    }
}

[[nodiscard]] std::string bytes_to_hex(
    const std::span<const std::byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
    };

    std::string output;
    output.reserve(bytes.size() * 2);

    for (const auto byte : bytes) {
        const auto value =
            std::to_integer<std::uint8_t>(
                byte);

        output.push_back(
            digits[
                static_cast<std::size_t>(
                    value >> 4U)]);

        output.push_back(
            digits[
                static_cast<std::size_t>(
                    value & 0x0fU)]);
    }

    return output;
}

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity =
            MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 3)
{
    return CommandEnvelope{
        .schema_version =
            tickline::command::
                command_schema_version,
        .type =
            CommandType::set_velocity,
        .client_id =
            ClientId{7},
        .session_id =
            SessionId{11},
        .sequence =
            sequence,
        .target_tick =
            target_tick,
        .payload =
            SetVelocityPayload{
                .entity_id =
                    EntityId{entity_id},
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{
                                25},
                        .y =
                            MillimetersPerSecond{
                                -40},
                    },
            },
    };
}

[[nodiscard]] World make_world()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position =
                Millimeters{100'000},
            .max_abs_velocity =
                MillimetersPerSecond{1'000},
        }};

    const auto result =
        world.add_entity(
            EntityState{
                .id =
                    EntityId{3},
                .position =
                    Position2{
                        .x =
                            Millimeters{0},
                        .y =
                            Millimeters{0},
                    },
                .velocity =
                    Velocity2{
                        .x =
                            MillimetersPerSecond{
                                0},
                        .y =
                            MillimetersPerSecond{
                                0},
                    },
                .last_sequence = 0,
            });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{
            "evidence test entity could not be added"};
    }

    return world;
}

void test_exact_canonical_encoding()
{
    const CommandEvidenceRecord record{
        .ordinal = 0,
        .previous_record_digest =
            Sha256Digest{},
        .entry =
            CommandEvidenceEntry{
                .world_fingerprint_before =
                    StateFingerprint{
                        0x0102030405060708ULL},
                .observed_tick = 100,
                .envelope =
                    make_command(1, 101),
                .session_sequence_before = 0,
                .session_sequence_after = 1,
                .pending_commands_before = 0,
                .pending_commands_after = 1,
                .rejection_code =
                    CommandRejectionCode::none,
                .queue_outcome =
                    CommandQueueOutcome::accepted,
            },
    };

    const auto encoded =
        tickline::command::
            encode_command_evidence(record);

    const std::string expected{
        "544c434500020000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0102030405060708"
        "0000000000000064"
        "00010001"
        "0000000000000007"
        "000000000000000b"
        "0000000000000001"
        "0000000000000065"
        "0000000000000003"
        "0000000000000019"
        "ffffffffffffffd8"
        "0000000000000000"
        "0000000000000001"
        "0000000000000000"
        "0000000000000001"
        "00000001",
    };

    expect(
        encoded.size() ==
            tickline::command::
                command_evidence_encoded_size,
        "command evidence should have a fixed encoded size");

    expect(
        bytes_to_hex(encoded) ==
            expected,
        "command evidence must match schema version two");

    expect(
        tickline::command::
            digest_command_evidence(record)
                .to_hex() ==
            "f283ab5e334b9725935405fcfee1b7d5"
            "202cdfeb505220040b0db51ba883595d",
        "canonical evidence SHA-256 digest must remain stable");
}

void test_accepted_and_rejected_submissions_are_chained()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    const auto world_before =
        tickline::simulation::
            fingerprint_world_state(world);

    const auto accepted =
        pipeline.submit(
            session,
            world,
            make_command(1, 2));

    expect(
        accepted.accepted(),
        "first submission should be accepted");

    expect(
        pipeline.evidence().size() == 1,
        "accepted submission should create evidence");

    const auto first =
        pipeline.evidence().records()[0];

    expect(
        first.ordinal == 0,
        "first evidence ordinal should be zero");

    expect(
        first.previous_record_digest ==
            Sha256Digest{},
        "first evidence record should begin with a zero digest");

    expect(
        first.entry.world_fingerprint_before ==
            world_before,
        "evidence should bind the pre-submission world state");

    expect(
        first.entry.session_sequence_before == 0 &&
            first.entry.session_sequence_after == 1,
        "accepted evidence should record session transition");

    expect(
        first.entry.pending_commands_before == 0 &&
            first.entry.pending_commands_after == 1,
        "accepted evidence should record queue transition");

    expect(
        first.entry.rejection_code ==
            CommandRejectionCode::none,
        "accepted evidence should have no rejection code");

    expect(
        first.entry.queue_outcome ==
            CommandQueueOutcome::accepted,
        "accepted evidence should preserve queue outcome");

    const auto first_head =
        pipeline.evidence().head_digest();

    const auto duplicate =
        pipeline.submit(
            session,
            world,
            make_command(1, 3));

    expect(
        !duplicate.accepted(),
        "duplicate submission should be rejected");

    expect(
        duplicate.code() ==
            CommandRejectionCode::
                duplicate_sequence,
        "duplicate rejection should remain explicit");

    expect(
        pipeline.evidence().size() == 2,
        "rejected submission should also create evidence");

    const auto second =
        pipeline.evidence().records()[1];

    expect(
        second.ordinal == 1,
        "second evidence ordinal should be one");

    expect(
        second.previous_record_digest ==
            first_head,
        "second record should reference the first digest");

    expect(
        second.entry.session_sequence_before == 1 &&
            second.entry.session_sequence_after == 1,
        "duplicate rejection should not change session state");

    expect(
        second.entry.pending_commands_before == 1 &&
            second.entry.pending_commands_after == 1,
        "duplicate rejection should not change queue state");

    expect(
        second.entry.rejection_code ==
            CommandRejectionCode::
                duplicate_sequence,
        "evidence should preserve duplicate rejection");

    expect(
        second.entry.queue_outcome ==
            CommandQueueOutcome::not_attempted,
        "session rejection should not reach the world queue");

    expect(
        pipeline.evidence().verify(),
        "untampered evidence chain should verify");
}

void test_world_rejection_is_recorded()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    const auto result =
        pipeline.submit(
            session,
            world,
            make_command(1, 2, 99));

    expect(
        !result.accepted(),
        "unknown entity should be rejected");

    expect(
        pipeline.evidence().size() == 1,
        "world rejection should create evidence");

    const auto& record =
        pipeline.evidence().records()[0];

    expect(
        record.entry.rejection_code ==
            CommandRejectionCode::unknown_entity,
        "evidence should preserve mapped world rejection");

    expect(
        record.entry.queue_outcome ==
            CommandQueueOutcome::unknown_entity,
        "evidence should preserve raw queue outcome");

    expect(
        record.entry.session_sequence_before == 0 &&
            record.entry.session_sequence_after == 0,
        "world rejection should not commit session state");

    expect(
        record.entry.pending_commands_before == 0 &&
            record.entry.pending_commands_after == 0,
        "world rejection should not change pending commands");

    expect(
        pipeline.evidence().verify(),
        "world-rejection evidence should verify");
}

void test_identical_histories_produce_identical_evidence()
{
    AuthoritativeCommandPipeline first_pipeline;
    AuthoritativeCommandPipeline second_pipeline;

    CommandSession first_session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    CommandSession second_session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto first_world = make_world();
    auto second_world = make_world();

    static_cast<void>(
        first_pipeline.submit(
            first_session,
            first_world,
            make_command(1, 2)));

    static_cast<void>(
        first_pipeline.submit(
            first_session,
            first_world,
            make_command(2, 3, 99)));

    static_cast<void>(
        first_pipeline.submit(
            first_session,
            first_world,
            make_command(2, 3)));

    static_cast<void>(
        second_pipeline.submit(
            second_session,
            second_world,
            make_command(1, 2)));

    static_cast<void>(
        second_pipeline.submit(
            second_session,
            second_world,
            make_command(2, 3, 99)));

    static_cast<void>(
        second_pipeline.submit(
            second_session,
            second_world,
            make_command(2, 3)));

    const auto first_records =
        first_pipeline.evidence().records();

    const auto second_records =
        second_pipeline.evidence().records();

    expect(
        first_records.size() ==
            second_records.size(),
        "identical histories should create equal record counts");

    for (std::size_t index = 0;
         index < first_records.size();
         ++index) {
        expect(
            first_records[index] ==
                second_records[index],
            "identical histories should create identical records");
    }

    expect(
        first_pipeline.evidence().head_digest() ==
            second_pipeline.evidence().head_digest(),
        "identical histories should create identical chain heads");
}

void test_tampering_invalidates_expected_chain_head()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3)));

    const auto original_head =
        pipeline.evidence().head_digest();

    const auto source =
        pipeline.evidence().records();

    std::vector<CommandEvidenceRecord> tampered{
        source.begin(),
        source.end(),
    };

    tampered.back().entry.envelope.sequence = 999;

    expect(
        !tickline::command::
            verify_command_evidence_chain(
                tampered,
                original_head),
        "modified evidence should not match the trusted chain head");

    tampered.assign(
        source.begin(),
        source.end());

    std::swap(
        tampered[0],
        tampered[1]);

    expect(
        !tickline::command::
            verify_command_evidence_chain(
                tampered,
                original_head),
        "reordered evidence should not verify");

    tampered.assign(
        source.begin(),
        source.end());

    tampered[1].previous_record_digest =
        Sha256Digest{};

    expect(
        !tickline::command::
            verify_command_evidence_chain(
                tampered,
                original_head),
        "broken digest linkage should not verify");
}

void test_wrong_expected_head_is_rejected()
{
    AuthoritativeCommandPipeline pipeline;

    CommandSession session{
        ClientId{7},
        SessionId{11},
        make_policy(),
    };

    auto world = make_world();

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(1, 2)));

    expect(
        !tickline::command::
            verify_command_evidence_chain(
                pipeline.evidence().records(),
                Sha256Digest{}),
        "a chain must not verify against an untrusted wrong head");
}

void test_queue_outcome_contract()
{
    expect(
        tickline::command::
            command_queue_outcome_name(
                CommandQueueOutcome::
                    not_attempted) ==
            "not_attempted",
        "not-attempted outcome should have a stable name");

    expect(
        tickline::command::
            command_queue_outcome_name(
                CommandQueueOutcome::
                    accepted) ==
            "accepted",
        "accepted outcome should have a stable name");

    expect(
        tickline::command::
            command_queue_outcome_name(
                static_cast<
                    CommandQueueOutcome>(
                    1234)) ==
            "unknown",
        "invalid queue outcome should fail closed");
}

}

int main()
{
    try {
        test_exact_canonical_encoding();
        test_accepted_and_rejected_submissions_are_chained();
        test_world_rejection_is_recorded();
        test_identical_histories_produce_identical_evidence();
        test_tampering_invalidates_expected_chain_head();
        test_wrong_expected_head_is_rejected();
        test_queue_outcome_contract();
    } catch (const std::exception& error) {
        std::cerr
            << "command evidence test failure: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}
