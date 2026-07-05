#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/command/command_evidence_archive.hpp"
#include "tickline/storage/investigation_store.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using tickline::command::AuthoritativeCommandPipeline;
using tickline::command::ClientId;
using tickline::command::CommandEnvelope;
using tickline::command::CommandEvidenceReplayCode;
using tickline::command::CommandEvidenceReplayResult;
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
using tickline::storage::EvidenceQuery;
using tickline::storage::InvestigationStore;
using tickline::storage::InvestigationStoreError;
using tickline::storage::InvestigationStoreErrorCode;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

template <typename Function>
void expect_store_error(
    Function function,
    const InvestigationStoreErrorCode expected,
    const std::string_view message)
{
    try {
        function();
    } catch (const InvestigationStoreError& error) {
        expect(error.code() == expected, message);
        return;
    }

    throw std::runtime_error{std::string{message}};
}

class TemporaryDirectory final {
public:
    TemporaryDirectory()
    {
        const auto seed = std::chrono::high_resolution_clock::now()
                              .time_since_epoch()
                              .count();

        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                ("tickline-investigation-" + std::to_string(seed) + "-" +
                 std::to_string(attempt));

            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) {
                return;
            }
        }

        throw std::runtime_error{
            "temporary investigation directory could not be created"};
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] CommandValidationPolicy make_policy()
{
    return CommandValidationPolicy{
        .maximum_future_ticks = 8,
        .max_abs_velocity = MillimetersPerSecond{1'000},
    };
}

[[nodiscard]] World make_world()
{
    World world{
        Microseconds{100'000},
        WorldLimits{
            .max_abs_position = Millimeters{100'000},
            .max_abs_velocity = MillimetersPerSecond{1'000},
        }};

    const auto result = world.add_entity(
        EntityState{
            .id = EntityId{3},
            .position = Position2{
                .x = Millimeters{0},
                .y = Millimeters{0},
            },
            .velocity = Velocity2{
                .x = MillimetersPerSecond{0},
                .y = MillimetersPerSecond{0},
            },
            .last_sequence = 0,
        });

    if (result != AddEntityResult::added) {
        throw std::runtime_error{"investigation test entity could not be added"};
    }

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 3)
{
    return CommandEnvelope{
        .schema_version = tickline::command::command_schema_version,
        .type = CommandType::set_velocity,
        .client_id = ClientId{7},
        .session_id = SessionId{11},
        .sequence = sequence,
        .target_tick = target_tick,
        .payload = SetVelocityPayload{
            .entity_id = EntityId{entity_id},
            .velocity = Velocity2{
                .x = MillimetersPerSecond{25},
                .y = MillimetersPerSecond{-40},
            },
        },
    };
}

struct ArchiveFixture final {
    std::vector<std::byte> encoded;
    Sha256Digest head;
};

[[nodiscard]] ArchiveFixture make_archive()
{
    AuthoritativeCommandPipeline pipeline;
    CommandSession session{ClientId{7}, SessionId{11}, make_policy()};
    auto world = make_world();

    static_cast<void>(pipeline.submit(session, world, make_command(1, 2)));
    static_cast<void>(pipeline.submit(session, world, make_command(1, 3)));
    static_cast<void>(pipeline.submit(session, world, make_command(2, 3, 99)));
    static_cast<void>(pipeline.submit(session, world, make_command(2, 3)));

    return ArchiveFixture{
        .encoded = tickline::command::encode_command_evidence_archive(
            pipeline.evidence().records(),
            pipeline.evidence().head_digest()),
        .head = pipeline.evidence().head_digest(),
    };
}

void test_migration_import_and_persistence()
{
    TemporaryDirectory directory;
    const auto database = directory.path() / "investigations.sqlite3";
    const auto fixture = make_archive();
    std::int64_t archive_id = 0;

    {
        InvestigationStore store{database};

        expect(
            store.schema_version() ==
                tickline::storage::investigation_schema_version,
            "store should apply the current migration");

        const auto imported = store.import_archive(
            fixture.encoded,
            fixture.head,
            "unit-test/archive.tlca");

        archive_id = imported.archive_id;

        expect(imported.inserted, "first archive import should insert rows");
        expect(imported.record_count == 4, "archive should contain four records");
        expect(store.archive_count() == 1, "store should contain one archive");
        expect(
            store.evidence_record_count() == 4,
            "store should contain four evidence records");

        const auto duplicate = store.import_archive(
            fixture.encoded,
            fixture.head,
            "unit-test/duplicate.tlca");

        expect(!duplicate.inserted, "duplicate archive should be idempotent");
        expect(
            duplicate.archive_id == archive_id,
            "duplicate archive should resolve to the original identifier");
        expect(store.archive_count() == 1, "duplicate should not add an archive");
        expect(
            store.evidence_record_count() == 4,
            "duplicate should not add evidence records");
    }

    {
        InvestigationStore reopened{database};
        expect(reopened.archive_count() == 1, "archive should persist after reopen");
        expect(
            reopened.evidence_record_count() == 4,
            "evidence should persist after reopen");

        const auto archives = reopened.list_archives();
        expect(archives.size() == 1, "archive listing should return persisted row");
        expect(
            archives.front().archive_id == archive_id,
            "archive listing should preserve the identifier");
        expect(
            archives.front().source == "unit-test/archive.tlca",
            "idempotent import should preserve original provenance");
    }
}

void test_query_filters_pagination_and_sessions()
{
    TemporaryDirectory directory;
    InvestigationStore store{directory.path() / "queries.sqlite3"};
    const auto fixture = make_archive();
    const auto imported = store.import_archive(
        fixture.encoded,
        fixture.head,
        "unit-test/query.tlca");

    const auto all = store.query_evidence(imported.archive_id);
    expect(all.size() == 4, "unfiltered query should return all records");
    expect(all[0].record.ordinal == 0, "query should order by ordinal");
    expect(all[3].record.ordinal == 3, "query should include final ordinal");

    EvidenceQuery accepted;
    accepted.rejection_code = CommandRejectionCode::none;
    accepted.queue_outcome = CommandQueueOutcome::accepted;

    const auto accepted_records = store.query_evidence(
        imported.archive_id,
        accepted);
    expect(
        accepted_records.size() == 2,
        "accepted filter should return two admitted commands");

    EvidenceQuery page;
    page.after_ordinal = 1;
    page.limit = 1;

    const auto page_records = store.query_evidence(imported.archive_id, page);
    expect(page_records.size() == 1, "page should respect its limit");
    expect(
        page_records.front().record.ordinal == 2,
        "page cursor should be exclusive");

    EvidenceQuery wrong_identity;
    wrong_identity.client_id = ClientId{99};
    expect(
        store.query_evidence(imported.archive_id, wrong_identity).empty(),
        "identity filter should reject nonmatching records");

    const auto sessions = store.list_sessions(imported.archive_id);
    expect(sessions.size() == 1, "archive should have one session summary");
    expect(
        sessions.front().client_id == ClientId{7},
        "session summary should preserve client identity");
    expect(
        sessions.front().session_id == SessionId{11},
        "session summary should preserve session identity");
    expect(
        sessions.front().accepted_count == 2,
        "session summary should count accepted submissions");
    expect(
        sessions.front().rejected_count == 2,
        "session summary should count rejected submissions");
}

void test_corruption_rolls_back_without_partial_rows()
{
    TemporaryDirectory directory;
    InvestigationStore store{directory.path() / "rollback.sqlite3"};
    auto fixture = make_archive();

    fixture.encoded.back() ^= std::byte{0x01};

    expect_store_error(
        [&] {
            static_cast<void>(store.import_archive(
                fixture.encoded,
                fixture.head,
                "unit-test/corrupt.tlca"));
        },
        InvestigationStoreErrorCode::archive_verification_failed,
        "corrupt archive should fail before persistence");

    expect(store.archive_count() == 0, "failed import should not add archive rows");
    expect(
        store.evidence_record_count() == 0,
        "failed import should not add evidence rows");
}

void test_replay_metadata_round_trip()
{
    TemporaryDirectory directory;
    InvestigationStore store{directory.path() / "replay.sqlite3"};
    const auto fixture = make_archive();
    const auto imported = store.import_archive(
        fixture.encoded,
        fixture.head,
        "unit-test/replay.tlca");

    expect(
        !store.replay_result(imported.archive_id).has_value(),
        "new archive should not have replay metadata");

    const CommandEvidenceReplayResult result{
        .code = CommandEvidenceReplayCode::verified,
        .processed_records = 4,
        .failed_ordinal = std::nullopt,
        .performed_tick_advances = 3,
        .final_tick = 3,
        .final_session_sequence = 2,
        .final_pending_command_count = 2,
        .final_world_fingerprint = StateFingerprint{0x0102030405060708ULL},
        .replayed_head_digest = fixture.head,
    };

    store.record_replay_result(imported.archive_id, result);

    const auto stored = store.replay_result(imported.archive_id);
    expect(stored.has_value(), "replay metadata should be queryable");
    expect(stored->code == result.code, "replay code should round-trip");
    expect(
        stored->processed_records == result.processed_records,
        "processed record count should round-trip");
    expect(
        stored->failed_ordinal == result.failed_ordinal,
        "failed ordinal should round-trip");
    expect(
        stored->performed_tick_advances == result.performed_tick_advances,
        "tick advance count should round-trip");
    expect(stored->final_tick == result.final_tick, "final tick should round-trip");
    expect(
        stored->final_session_sequence == result.final_session_sequence,
        "final session sequence should round-trip");
    expect(
        stored->final_pending_command_count ==
            result.final_pending_command_count,
        "final pending command count should round-trip");
    expect(
        stored->final_world_fingerprint == result.final_world_fingerprint,
        "final world fingerprint should round-trip");
    expect(
        stored->replayed_head_digest == result.replayed_head_digest,
        "replayed head digest should round-trip");

    expect_store_error(
        [&] { store.record_replay_result(999, result); },
        InvestigationStoreErrorCode::archive_not_found,
        "replay metadata should require an existing archive");
}

void test_archive_pagination_and_validation()
{
    TemporaryDirectory directory;
    InvestigationStore store{directory.path() / "pagination.sqlite3"};
    const auto fixture = make_archive();

    const auto first = store.import_archive(
        fixture.encoded,
        fixture.head,
        "unit-test/full.tlca");

    const auto empty_encoded =
        tickline::command::encode_command_evidence_archive(
            std::span<const tickline::command::CommandEvidenceRecord>{},
            Sha256Digest{});

    const auto second = store.import_archive(
        empty_encoded,
        Sha256Digest{},
        "unit-test/empty.tlca");

    const auto newest = store.list_archives(1);
    expect(newest.size() == 1, "archive page should respect limit");
    expect(
        newest.front().archive_id == second.archive_id,
        "archive listing should return newest first");

    const auto older = store.list_archives(10, second.archive_id);
    expect(older.size() == 1, "archive cursor should return older rows");
    expect(
        older.front().archive_id == first.archive_id,
        "archive cursor should be exclusive");

    EvidenceQuery invalid_range;
    invalid_range.minimum_observed_tick = 5;
    invalid_range.maximum_observed_tick = 4;

    expect_store_error(
        [&] {
            static_cast<void>(store.query_evidence(
                first.archive_id,
                invalid_range));
        },
        InvestigationStoreErrorCode::invalid_argument,
        "invalid tick range should be rejected");

    expect_store_error(
        [&] { static_cast<void>(store.list_archives(0)); },
        InvestigationStoreErrorCode::invalid_argument,
        "zero query limit should be rejected");
}

}

int main()
{
    try {
        test_migration_import_and_persistence();
        test_query_filters_pagination_and_sessions();
        test_corruption_rolls_back_without_partial_rows();
        test_replay_metadata_round_trip();
        test_archive_pagination_and_validation();
    } catch (const std::exception& error) {
        std::cerr << "investigation store test failure: " << error.what() << '\n';
        return 1;
    }

    std::cout << "investigation store tests passed\n";
    return 0;
}
