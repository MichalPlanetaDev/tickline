#include "tickline/command/authoritative_command_pipeline.hpp"
#include "tickline/command/command_evidence_archive.hpp"
#include "tickline/storage/investigation_bundle_exporter.hpp"
#include "tickline/storage/investigation_store.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
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
using tickline::storage::InvestigationBundleExportError;
using tickline::storage::InvestigationBundleExportErrorCode;
using tickline::storage::InvestigationStore;
using tickline::storage::export_investigation_bundle_json;

void expect(
    const bool condition,
    const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

template <typename Function>
void expect_export_error(
    Function function,
    const InvestigationBundleExportErrorCode expected,
    const std::string_view message)
{
    try {
        function();
    } catch (const InvestigationBundleExportError& error) {
        expect(error.code() == expected, message);
        return;
    }

    throw std::runtime_error{std::string{message}};
}

class TemporaryDirectory final {
public:
    TemporaryDirectory()
    {
        const auto seed =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count();

        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ =
                std::filesystem::temp_directory_path() /
                ("tickline-bundle-export-" +
                 std::to_string(seed) + "-" +
                 std::to_string(attempt));

            std::error_code error;

            if (std::filesystem::create_directory(
                    path_,
                    error)) {
                return;
            }
        }

        throw std::runtime_error{
            "temporary exporter directory could not be created"};
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path&
    path() const noexcept
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
        .max_abs_velocity =
            MillimetersPerSecond{1'000},
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

    expect(
        result == AddEntityResult::added,
        "exporter test entity could not be added");

    return world;
}

[[nodiscard]] CommandEnvelope make_command(
    const std::uint64_t sequence,
    const std::uint64_t target_tick,
    const std::uint64_t entity_id = 3)
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
            make_command(1, 3)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3, 99)));

    static_cast<void>(
        pipeline.submit(
            session,
            world,
            make_command(2, 3)));

    return ArchiveFixture{
        .encoded =
            tickline::command::
                encode_command_evidence_archive(
                    pipeline.evidence().records(),
                    pipeline.evidence().head_digest()),
        .head = pipeline.evidence().head_digest(),
    };
}

void set_fixed_import_timestamp(
    const std::filesystem::path& database)
{
    sqlite3* connection = nullptr;

    if (sqlite3_open(
            database.string().c_str(),
            &connection) != SQLITE_OK) {
        if (connection != nullptr) {
            sqlite3_close(connection);
        }

        throw std::runtime_error{
            "test database could not be opened"};
    }

    char* error_message = nullptr;

    const int result = sqlite3_exec(
        connection,
        "UPDATE evidence_archives "
        "SET imported_at_utc = "
        "'2026-07-05 12:00:00'",
        nullptr,
        nullptr,
        &error_message);

    std::string message;

    if (error_message != nullptr) {
        message = error_message;
        sqlite3_free(error_message);
    }

    sqlite3_close(connection);

    if (result != SQLITE_OK) {
        throw std::runtime_error{
            "test timestamp update failed: " + message};
    }
}

[[nodiscard]] std::string make_export()
{
    TemporaryDirectory directory;
    const auto database =
        directory.path() / "investigation.sqlite3";
    const auto fixture = make_archive();

    InvestigationStore store{database};

    const auto imported = store.import_archive(
        fixture.encoded,
        fixture.head,
        "unit-test/export.tlca");

    expect_export_error(
        [&] {
            static_cast<void>(
                export_investigation_bundle_json(
                    store,
                    imported.archive_id));
        },
        InvestigationBundleExportErrorCode::
            replay_result_missing,
        "export should require replay metadata");

    const CommandEvidenceReplayResult replay{
        .code = CommandEvidenceReplayCode::verified,
        .processed_records = 4,
        .failed_ordinal = std::nullopt,
        .performed_tick_advances = 3,
        .final_tick = 3,
        .final_session_sequence = 2,
        .final_pending_command_count = 2,
        .final_world_fingerprint =
            StateFingerprint{0x0102030405060708ULL},
        .replayed_head_digest = fixture.head,
    };

    store.record_replay_result(
        imported.archive_id,
        replay);

    set_fixed_import_timestamp(database);

    expect_export_error(
        [&] {
            static_cast<void>(
                export_investigation_bundle_json(
                    store,
                    999));
        },
        InvestigationBundleExportErrorCode::
            archive_not_found,
        "missing archive should be rejected");

    const auto first =
        export_investigation_bundle_json(
            store,
            imported.archive_id);

    const auto second =
        export_investigation_bundle_json(
            store,
            imported.archive_id);

    expect(
        first == second,
        "repeated export should be deterministic");

    expect(
        first.find("\"schemaVersion\": 1") !=
            std::string::npos,
        "bundle schema version should be exported");

    expect(
        first.find(
            "\"importedAtUtc\": "
            "\"2026-07-05T12:00:00Z\"") !=
            std::string::npos,
        "import timestamp should be normalized");

    expect(
        first.find("\"acceptedCommands\": 2") !=
            std::string::npos,
        "accepted count should be exported");

    expect(
        first.find("\"rejectedCommands\": 2") !=
            std::string::npos,
        "rejected count should be exported");

    return first;
}

[[nodiscard]] std::string read_file(
    const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};

    if (!input) {
        throw std::runtime_error{
            "fixture could not be opened: " +
            path.string()};
    }

    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

void write_file(
    const std::filesystem::path& path,
    const std::string_view contents)
{
    std::ofstream output{
        path,
        std::ios::binary | std::ios::trunc,
    };

    if (!output) {
        throw std::runtime_error{
            "fixture could not be written: " +
            path.string()};
    }

    output.write(
        contents.data(),
        static_cast<std::streamsize>(
            contents.size()));

    if (!output) {
        throw std::runtime_error{
            "fixture write failed"};
    }
}

}

int main(const int argc, char* argv[])
{
    try {
        const auto exported = make_export();

        if (argc == 3 &&
            std::string_view{argv[1]} ==
                "--write-fixture") {
            write_file(argv[2], exported);
            std::cout
                << "wrote investigation bundle fixture: "
                << argv[2] << '\n';
            return 0;
        }

        expect(
            argc == 1,
            "unexpected exporter test arguments");

        const auto fixture =
            read_file(
                TICKLINE_UNITY_BUNDLE_FIXTURE);

        expect(
            fixture == exported,
            "Unity fixture does not match native export");

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "investigation bundle exporter test failed: "
            << error.what() << '\n';
        return 1;
    }
}
