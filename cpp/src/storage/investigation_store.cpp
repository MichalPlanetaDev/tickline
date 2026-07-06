#include "tickline/storage/investigation_store.hpp"

#include <sqlite3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace tickline::storage {
namespace {

using U64Bytes = std::array<std::byte, 8>;

[[nodiscard]] U64Bytes encode_u64(const std::uint64_t value) noexcept
{
    U64Bytes encoded{};

    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const auto shift =
            static_cast<unsigned int>((encoded.size() - index - 1U) * 8U);

        encoded[index] = static_cast<std::byte>((value >> shift) & 0xffU);
    }

    return encoded;
}

[[nodiscard]] std::uint64_t decode_u64(
    const void* data,
    const int size,
    const std::string_view column_name)
{
    if (data == nullptr || size != 8) {
        throw InvestigationStoreError{
            InvestigationStoreErrorCode::sqlite_failure,
            std::string{column_name} + " must contain exactly eight bytes"};
    }

    const auto* bytes = static_cast<const unsigned char*>(data);
    std::uint64_t value = 0;

    for (int index = 0; index < size; ++index) {
        value = (value << 8U) | bytes[index];
    }

    return value;
}

[[nodiscard]] security::Sha256Digest decode_digest(
    const void* data,
    const int size,
    const std::string_view column_name)
{
    if (data == nullptr || size != 32) {
        throw InvestigationStoreError{
            InvestigationStoreErrorCode::sqlite_failure,
            std::string{column_name} + " must contain exactly 32 bytes"};
    }

    security::Sha256Digest::Storage bytes{};
    std::memcpy(bytes.data(), data, bytes.size());
    return security::Sha256Digest{bytes};
}

[[nodiscard]] std::int64_t checked_int64(
    const std::size_t value,
    const std::string_view name)
{
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<std::int64_t>::max())) {
        throw InvestigationStoreError{
            InvestigationStoreErrorCode::invalid_argument,
            std::string{name} + " exceeds SQLite integer range"};
    }

    return static_cast<std::int64_t>(value);
}

[[nodiscard]] std::size_t checked_size(
    const sqlite3_int64 value,
    const std::string_view name)
{
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())) {
        throw InvestigationStoreError{
            InvestigationStoreErrorCode::sqlite_failure,
            std::string{name} + " is outside the local size range"};
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::size_t checked_size_u64(
    const std::uint64_t value,
    const std::string_view name)
{
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max())) {
        throw InvestigationStoreError{
            InvestigationStoreErrorCode::sqlite_failure,
            std::string{name} + " is outside the local size range"};
    }

    return static_cast<std::size_t>(value);
}


[[noreturn]] void throw_sqlite(
    sqlite3* database,
    const InvestigationStoreErrorCode code,
    const std::string_view operation)
{
    std::string message{operation};
    message += ": ";
    message += database == nullptr
        ? "SQLite database is unavailable"
        : sqlite3_errmsg(database);

    throw InvestigationStoreError{code, std::move(message)};
}

void execute_sql(
    sqlite3* database,
    const std::string_view sql,
    const InvestigationStoreErrorCode code,
    const std::string_view operation)
{
    char* error_message = nullptr;
    const std::string statement{sql};

    const int result = sqlite3_exec(
        database,
        statement.c_str(),
        nullptr,
        nullptr,
        &error_message);

    if (result == SQLITE_OK) {
        return;
    }

    std::string message{operation};
    message += ": ";
    message += error_message == nullptr
        ? sqlite3_errmsg(database)
        : error_message;

    sqlite3_free(error_message);
    throw InvestigationStoreError{code, std::move(message)};
}

class Statement final {
public:
    Statement(
        sqlite3* database,
        const std::string_view sql,
        const std::string_view operation)
        : database_{database},
          operation_{operation}
    {
        const std::string statement{sql};
        const int result = sqlite3_prepare_v2(
            database_,
            statement.c_str(),
            -1,
            &statement_,
            nullptr);

        if (result != SQLITE_OK) {
            throw_sqlite(
                database_,
                InvestigationStoreErrorCode::sqlite_failure,
                operation_);
        }
    }

    ~Statement()
    {
        sqlite3_finalize(statement_);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bind_int(const int index, const int value)
    {
        check_bind(sqlite3_bind_int(statement_, index, value));
    }

    void bind_int64(const int index, const std::int64_t value)
    {
        check_bind(sqlite3_bind_int64(statement_, index, value));
    }

    void bind_text(const int index, const std::string_view value)
    {
        check_bind(sqlite3_bind_text(
            statement_,
            index,
            value.data(),
            static_cast<int>(value.size()),
            SQLITE_TRANSIENT));
    }

    void bind_blob(
        const int index,
        const std::span<const std::byte> value)
    {
        if (value.size() > static_cast<std::size_t>(
                               std::numeric_limits<int>::max())) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "SQLite blob exceeds binding range"};
        }

        check_bind(sqlite3_bind_blob(
            statement_,
            index,
            value.data(),
            static_cast<int>(value.size()),
            SQLITE_TRANSIENT));
    }

    void bind_u64(const int index, const std::uint64_t value)
    {
        const auto encoded = encode_u64(value);
        bind_blob(index, encoded);
    }

    void bind_digest(
        const int index,
        const security::Sha256Digest& digest)
    {
        bind_blob(index, digest.bytes());
    }

    void bind_null(const int index)
    {
        check_bind(sqlite3_bind_null(statement_, index));
    }

    [[nodiscard]] bool step_row()
    {
        const int result = sqlite3_step(statement_);

        if (result == SQLITE_ROW) {
            return true;
        }

        if (result == SQLITE_DONE) {
            return false;
        }

        throw_sqlite(
            database_,
            InvestigationStoreErrorCode::sqlite_failure,
            operation_);
    }

    void step_done()
    {
        const int result = sqlite3_step(statement_);

        if (result != SQLITE_DONE) {
            throw_sqlite(
                database_,
                InvestigationStoreErrorCode::sqlite_failure,
                operation_);
        }

        if (sqlite3_reset(statement_) != SQLITE_OK ||
            sqlite3_clear_bindings(statement_) != SQLITE_OK) {
            throw_sqlite(
                database_,
                InvestigationStoreErrorCode::sqlite_failure,
                operation_);
        }
    }

    [[nodiscard]] sqlite3_int64 column_int64(const int index) const noexcept
    {
        return sqlite3_column_int64(statement_, index);
    }

    [[nodiscard]] int column_int(const int index) const noexcept
    {
        return sqlite3_column_int(statement_, index);
    }

    [[nodiscard]] bool column_is_null(const int index) const noexcept
    {
        return sqlite3_column_type(statement_, index) == SQLITE_NULL;
    }

    [[nodiscard]] std::string column_text(const int index) const
    {
        const auto* text = sqlite3_column_text(statement_, index);
        const int size = sqlite3_column_bytes(statement_, index);

        if (text == nullptr) {
            return {};
        }

        return std::string{
            reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(size)};
    }

    [[nodiscard]] std::span<const std::byte> column_blob(
        const int index) const noexcept
    {
        const void* data = sqlite3_column_blob(statement_, index);
        const int size = sqlite3_column_bytes(statement_, index);

        return {
            static_cast<const std::byte*>(data),
            static_cast<std::size_t>(size)};
    }

    [[nodiscard]] std::uint64_t column_u64(
        const int index,
        const std::string_view name) const
    {
        return decode_u64(
            sqlite3_column_blob(statement_, index),
            sqlite3_column_bytes(statement_, index),
            name);
    }

    [[nodiscard]] security::Sha256Digest column_digest(
        const int index,
        const std::string_view name) const
    {
        return decode_digest(
            sqlite3_column_blob(statement_, index),
            sqlite3_column_bytes(statement_, index),
            name);
    }

private:
    void check_bind(const int result)
    {
        if (result != SQLITE_OK) {
            throw_sqlite(
                database_,
                InvestigationStoreErrorCode::sqlite_failure,
                operation_);
        }
    }

    sqlite3* database_;
    sqlite3_stmt* statement_{nullptr};
    std::string operation_;
};

class Transaction final {
public:
    explicit Transaction(sqlite3* database)
        : database_{database}
    {
        execute_sql(
            database_,
            "BEGIN IMMEDIATE",
            InvestigationStoreErrorCode::sqlite_failure,
            "begin investigation transaction");
    }

    ~Transaction()
    {
        if (!committed_) {
            char* ignored = nullptr;
            sqlite3_exec(
                database_,
                "ROLLBACK",
                nullptr,
                nullptr,
                &ignored);
            sqlite3_free(ignored);
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit()
    {
        execute_sql(
            database_,
            "COMMIT",
            InvestigationStoreErrorCode::sqlite_failure,
            "commit investigation transaction");
        committed_ = true;
    }

private:
    sqlite3* database_;
    bool committed_{false};
};

constexpr std::string_view schema_sql = R"sql(
CREATE TABLE evidence_archives (
    archive_id INTEGER PRIMARY KEY,
    archive_digest BLOB NOT NULL UNIQUE CHECK(length(archive_digest) = 32),
    trusted_head BLOB NOT NULL CHECK(length(trusted_head) = 32),
    declared_head BLOB NOT NULL CHECK(length(declared_head) = 32),
    archive_schema INTEGER NOT NULL,
    record_schema INTEGER NOT NULL,
    record_count INTEGER NOT NULL CHECK(record_count >= 0),
    source TEXT NOT NULL CHECK(length(source) > 0),
    imported_at_utc TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE command_evidence (
    archive_id INTEGER NOT NULL REFERENCES evidence_archives(archive_id) ON DELETE CASCADE,
    ordinal BLOB NOT NULL CHECK(length(ordinal) = 8),
    record_digest BLOB NOT NULL CHECK(length(record_digest) = 32),
    previous_record_digest BLOB NOT NULL CHECK(length(previous_record_digest) = 32),
    world_fingerprint_before BLOB NOT NULL CHECK(length(world_fingerprint_before) = 8),
    observed_tick BLOB NOT NULL CHECK(length(observed_tick) = 8),
    schema_version INTEGER NOT NULL,
    command_type INTEGER NOT NULL,
    client_id BLOB NOT NULL CHECK(length(client_id) = 8),
    session_id BLOB NOT NULL CHECK(length(session_id) = 8),
    sequence BLOB NOT NULL CHECK(length(sequence) = 8),
    target_tick BLOB NOT NULL CHECK(length(target_tick) = 8),
    entity_id BLOB NOT NULL CHECK(length(entity_id) = 8),
    velocity_x INTEGER NOT NULL,
    velocity_y INTEGER NOT NULL,
    session_sequence_before BLOB NOT NULL CHECK(length(session_sequence_before) = 8),
    session_sequence_after BLOB NOT NULL CHECK(length(session_sequence_after) = 8),
    pending_commands_before BLOB NOT NULL CHECK(length(pending_commands_before) = 8),
    pending_commands_after BLOB NOT NULL CHECK(length(pending_commands_after) = 8),
    rejection_code INTEGER NOT NULL,
    queue_outcome INTEGER NOT NULL,
    encoded_record BLOB NOT NULL CHECK(length(encoded_record) = 160),
    PRIMARY KEY (archive_id, ordinal),
    UNIQUE (archive_id, record_digest)
) WITHOUT ROWID;

CREATE TABLE archive_sessions (
    archive_id INTEGER NOT NULL REFERENCES evidence_archives(archive_id) ON DELETE CASCADE,
    client_id BLOB NOT NULL CHECK(length(client_id) = 8),
    session_id BLOB NOT NULL CHECK(length(session_id) = 8),
    first_ordinal BLOB NOT NULL CHECK(length(first_ordinal) = 8),
    last_ordinal BLOB NOT NULL CHECK(length(last_ordinal) = 8),
    first_observed_tick BLOB NOT NULL CHECK(length(first_observed_tick) = 8),
    last_observed_tick BLOB NOT NULL CHECK(length(last_observed_tick) = 8),
    accepted_count INTEGER NOT NULL CHECK(accepted_count >= 0),
    rejected_count INTEGER NOT NULL CHECK(rejected_count >= 0),
    PRIMARY KEY (archive_id, client_id, session_id)
) WITHOUT ROWID;

CREATE TABLE replay_results (
    archive_id INTEGER PRIMARY KEY REFERENCES evidence_archives(archive_id) ON DELETE CASCADE,
    replay_code INTEGER NOT NULL,
    processed_records INTEGER NOT NULL CHECK(processed_records >= 0),
    failed_ordinal BLOB CHECK(failed_ordinal IS NULL OR length(failed_ordinal) = 8),
    performed_tick_advances BLOB NOT NULL CHECK(length(performed_tick_advances) = 8),
    final_tick BLOB NOT NULL CHECK(length(final_tick) = 8),
    final_session_sequence BLOB NOT NULL CHECK(length(final_session_sequence) = 8),
    final_pending_command_count BLOB NOT NULL CHECK(length(final_pending_command_count) = 8),
    final_world_fingerprint BLOB NOT NULL CHECK(length(final_world_fingerprint) = 8),
    replayed_head_digest BLOB NOT NULL CHECK(length(replayed_head_digest) = 32),
    recorded_at_utc TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX command_evidence_identity_idx
    ON command_evidence (archive_id, client_id, session_id, ordinal);

CREATE INDEX command_evidence_tick_idx
    ON command_evidence (archive_id, observed_tick, ordinal);

CREATE INDEX command_evidence_rejection_idx
    ON command_evidence (archive_id, rejection_code, ordinal);

CREATE INDEX command_evidence_outcome_idx
    ON command_evidence (archive_id, queue_outcome, ordinal);
)sql";

struct SessionAggregate final {
    std::uint64_t first_ordinal;
    std::uint64_t last_ordinal;
    std::uint64_t first_observed_tick;
    std::uint64_t last_observed_tick;
    std::size_t accepted_count{0};
    std::size_t rejected_count{0};
};

}

InvestigationStoreError::InvestigationStoreError(
    const InvestigationStoreErrorCode code,
    std::string message)
    : std::runtime_error{std::move(message)},
      code_{code}
{
}

InvestigationStoreErrorCode InvestigationStoreError::code() const noexcept
{
    return code_;
}

class InvestigationStore::Impl final {
public:
    explicit Impl(const std::filesystem::path& path)
    {
        const std::string database_path = path.string();
        const int result = sqlite3_open_v2(
            database_path.c_str(),
            &database_,
            SQLITE_OPEN_READWRITE |
                SQLITE_OPEN_CREATE |
                SQLITE_OPEN_FULLMUTEX,
            nullptr);

        if (result != SQLITE_OK) {
            const std::string message = database_ == nullptr
                ? "open investigation database: SQLite did not allocate a handle"
                : std::string{"open investigation database: "} +
                    sqlite3_errmsg(database_);

            sqlite3_close(database_);
            database_ = nullptr;

            throw InvestigationStoreError{
                InvestigationStoreErrorCode::open_failed,
                message};
        }

        sqlite3_extended_result_codes(database_, 1);
        sqlite3_busy_timeout(database_, 5'000);

        execute_sql(
            database_,
            "PRAGMA foreign_keys = ON; PRAGMA trusted_schema = OFF;",
            InvestigationStoreErrorCode::open_failed,
            "configure investigation database");

        migrate();
    }

    ~Impl()
    {
        sqlite3_close(database_);
    }

    void migrate()
    {
        try {
            Transaction transaction{database_};

            execute_sql(
                database_,
                "CREATE TABLE IF NOT EXISTS schema_migrations ("
                "version INTEGER PRIMARY KEY, "
                "applied_at_utc TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)",
                InvestigationStoreErrorCode::migration_failed,
                "create migration ledger");

            Statement current{
                database_,
                "SELECT COALESCE(MAX(version), 0) FROM schema_migrations",
                "read investigation schema version"};

            if (!current.step_row()) {
                throw InvestigationStoreError{
                    InvestigationStoreErrorCode::migration_failed,
                    "migration ledger did not return a schema version"};
            }

            const int version = current.column_int(0);

            if (version > investigation_schema_version) {
                throw InvestigationStoreError{
                    InvestigationStoreErrorCode::migration_failed,
                    "investigation database schema is newer than this binary"};
            }

            if (version == 0) {
                execute_sql(
                    database_,
                    schema_sql,
                    InvestigationStoreErrorCode::migration_failed,
                    "apply investigation schema version one");

                Statement record{
                    database_,
                    "INSERT INTO schema_migrations(version) VALUES (?)",
                    "record investigation migration"};
                record.bind_int(1, investigation_schema_version);
                record.step_done();
            }

            transaction.commit();
        } catch (const InvestigationStoreError& error) {
            if (error.code() == InvestigationStoreErrorCode::migration_failed) {
                throw;
            }

            throw InvestigationStoreError{
                InvestigationStoreErrorCode::migration_failed,
                error.what()};
        }
    }

    [[nodiscard]] int schema_version() const
    {
        Statement statement{
            database_,
            "SELECT COALESCE(MAX(version), 0) FROM schema_migrations",
            "read investigation schema version"};

        if (!statement.step_row()) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::sqlite_failure,
                "migration ledger did not return a schema version"};
        }

        return statement.column_int(0);
    }

    [[nodiscard]] EvidenceArchiveImportResult import_archive(
        const std::span<const std::byte> encoded_archive,
        const security::Sha256Digest trusted_head,
        std::string source)
    {
        if (encoded_archive.empty()) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "encoded evidence archive must not be empty"};
        }

        if (source.empty()) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "evidence archive source must not be empty"};
        }

        command::CommandEvidenceArchive archive;

        try {
            archive = command::decode_command_evidence_archive(
                encoded_archive,
                trusted_head);
        } catch (const command::CommandEvidenceArchiveError& error) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::archive_verification_failed,
                std::string{"evidence archive verification failed: "} +
                    error.what()};
        } catch (const command::CommandEvidenceDecodeError& error) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::archive_verification_failed,
                std::string{"evidence record verification failed: "} +
                    error.what()};
        }

        const auto archive_digest = security::sha256(encoded_archive);
        Transaction transaction{database_};

        Statement insert_archive{
            database_,
            "INSERT OR IGNORE INTO evidence_archives("
            "archive_digest, trusted_head, declared_head, archive_schema, "
            "record_schema, record_count, source) VALUES (?, ?, ?, ?, ?, ?, ?)",
            "insert evidence archive"};

        insert_archive.bind_digest(1, archive_digest);
        insert_archive.bind_digest(2, trusted_head);
        insert_archive.bind_digest(3, archive.head_digest);
        insert_archive.bind_int(
            4,
            static_cast<int>(command::command_evidence_archive_schema_version));
        insert_archive.bind_int(
            5,
            static_cast<int>(command::command_evidence_schema_version));
        insert_archive.bind_int64(
            6,
            checked_int64(archive.records.size(), "archive record count"));
        insert_archive.bind_text(7, source);
        insert_archive.step_done();

        const bool inserted = sqlite3_changes(database_) == 1;
        std::int64_t archive_id = 0;

        if (inserted) {
            archive_id = sqlite3_last_insert_rowid(database_);
        } else {
            Statement existing{
                database_,
                "SELECT archive_id, record_count, declared_head "
                "FROM evidence_archives WHERE archive_digest = ?",
                "read existing evidence archive"};
            existing.bind_digest(1, archive_digest);

            if (!existing.step_row()) {
                throw InvestigationStoreError{
                    InvestigationStoreErrorCode::sqlite_failure,
                    "duplicate archive digest was not readable"};
            }

            archive_id = existing.column_int64(0);
            const auto existing_count = checked_size(
                existing.column_int64(1),
                "stored archive record count");
            const auto existing_head = existing.column_digest(
                2,
                "stored archive head");

            if (existing_count != archive.records.size() ||
                existing_head != archive.head_digest) {
                throw InvestigationStoreError{
                    InvestigationStoreErrorCode::sqlite_failure,
                    "archive digest conflicts with stored archive metadata"};
            }

            transaction.commit();

            return EvidenceArchiveImportResult{
                .archive_id = archive_id,
                .inserted = false,
                .record_count = archive.records.size(),
                .archive_digest = archive_digest,
                .chain_head = archive.head_digest,
            };
        }

        Statement insert_record{
            database_,
            "INSERT INTO command_evidence("
            "archive_id, ordinal, record_digest, previous_record_digest, "
            "world_fingerprint_before, observed_tick, schema_version, command_type, "
            "client_id, session_id, sequence, target_tick, entity_id, velocity_x, "
            "velocity_y, session_sequence_before, session_sequence_after, "
            "pending_commands_before, pending_commands_after, rejection_code, "
            "queue_outcome, encoded_record) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            "insert command evidence record"};

        std::map<std::pair<std::uint64_t, std::uint64_t>, SessionAggregate>
            session_aggregates;

        for (const auto& record : archive.records) {
            const auto encoded_record = command::encode_command_evidence(record);
            const auto record_digest = command::digest_command_evidence(record);
            const auto& entry = record.entry;
            const auto& envelope = entry.envelope;

            insert_record.bind_int64(1, archive_id);
            insert_record.bind_u64(2, record.ordinal);
            insert_record.bind_digest(3, record_digest);
            insert_record.bind_digest(4, record.previous_record_digest);
            insert_record.bind_u64(
                5,
                entry.world_fingerprint_before.value());
            insert_record.bind_u64(6, entry.observed_tick);
            insert_record.bind_int(7, static_cast<int>(envelope.schema_version));
            insert_record.bind_int(8, static_cast<int>(envelope.type));
            insert_record.bind_u64(9, envelope.client_id.value());
            insert_record.bind_u64(10, envelope.session_id.value());
            insert_record.bind_u64(11, envelope.sequence);
            insert_record.bind_u64(12, envelope.target_tick);
            insert_record.bind_u64(13, envelope.payload.entity_id.value());
            insert_record.bind_int64(14, envelope.payload.velocity.x.count());
            insert_record.bind_int64(15, envelope.payload.velocity.y.count());
            insert_record.bind_u64(16, entry.session_sequence_before);
            insert_record.bind_u64(17, entry.session_sequence_after);
            insert_record.bind_u64(18, entry.pending_commands_before);
            insert_record.bind_u64(19, entry.pending_commands_after);
            insert_record.bind_int(20, static_cast<int>(entry.rejection_code));
            insert_record.bind_int(21, static_cast<int>(entry.queue_outcome));
            insert_record.bind_blob(22, encoded_record);
            insert_record.step_done();

            const auto key = std::pair{
                envelope.client_id.value(),
                envelope.session_id.value()};

            const auto [iterator, was_inserted] = session_aggregates.try_emplace(
                key,
                SessionAggregate{
                    .first_ordinal = record.ordinal,
                    .last_ordinal = record.ordinal,
                    .first_observed_tick = entry.observed_tick,
                    .last_observed_tick = entry.observed_tick,
                });

            auto& aggregate = iterator->second;

            if (!was_inserted) {
                aggregate.last_ordinal = record.ordinal;
                aggregate.last_observed_tick = entry.observed_tick;
            }

            if (entry.rejection_code == command::CommandRejectionCode::none) {
                ++aggregate.accepted_count;
            } else {
                ++aggregate.rejected_count;
            }
        }

        Statement insert_session{
            database_,
            "INSERT INTO archive_sessions("
            "archive_id, client_id, session_id, first_ordinal, last_ordinal, "
            "first_observed_tick, last_observed_tick, accepted_count, rejected_count) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            "insert archive session summary"};

        for (const auto& [identity, aggregate] : session_aggregates) {
            insert_session.bind_int64(1, archive_id);
            insert_session.bind_u64(2, identity.first);
            insert_session.bind_u64(3, identity.second);
            insert_session.bind_u64(4, aggregate.first_ordinal);
            insert_session.bind_u64(5, aggregate.last_ordinal);
            insert_session.bind_u64(6, aggregate.first_observed_tick);
            insert_session.bind_u64(7, aggregate.last_observed_tick);
            insert_session.bind_int64(
                8,
                checked_int64(aggregate.accepted_count, "accepted count"));
            insert_session.bind_int64(
                9,
                checked_int64(aggregate.rejected_count, "rejected count"));
            insert_session.step_done();
        }

        transaction.commit();

        return EvidenceArchiveImportResult{
            .archive_id = archive_id,
            .inserted = true,
            .record_count = archive.records.size(),
            .archive_digest = archive_digest,
            .chain_head = archive.head_digest,
        };
    }

    void record_replay_result(
        const std::int64_t archive_id,
        const command::CommandEvidenceReplayResult& result)
    {
        if (archive_id <= 0) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "archive identifier must be positive"};
        }

        Statement archive_exists{
            database_,
            "SELECT 1 FROM evidence_archives WHERE archive_id = ?",
            "locate archive before replay result"};
        archive_exists.bind_int64(1, archive_id);

        if (!archive_exists.step_row()) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::archive_not_found,
                "archive identifier does not exist"};
        }

        if (result.final_pending_command_count >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint64_t>::max())) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "pending command count exceeds storage range"};
        }

        Transaction transaction{database_};
        Statement statement{
            database_,
            "INSERT INTO replay_results("
            "archive_id, replay_code, processed_records, failed_ordinal, "
            "performed_tick_advances, final_tick, final_session_sequence, "
            "final_pending_command_count, final_world_fingerprint, replayed_head_digest) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(archive_id) DO UPDATE SET "
            "replay_code = excluded.replay_code, "
            "processed_records = excluded.processed_records, "
            "failed_ordinal = excluded.failed_ordinal, "
            "performed_tick_advances = excluded.performed_tick_advances, "
            "final_tick = excluded.final_tick, "
            "final_session_sequence = excluded.final_session_sequence, "
            "final_pending_command_count = excluded.final_pending_command_count, "
            "final_world_fingerprint = excluded.final_world_fingerprint, "
            "replayed_head_digest = excluded.replayed_head_digest, "
            "recorded_at_utc = CURRENT_TIMESTAMP",
            "record replay result"};

        statement.bind_int64(1, archive_id);
        statement.bind_int(2, static_cast<int>(result.code));
        statement.bind_int64(
            3,
            checked_int64(result.processed_records, "processed record count"));

        if (result.failed_ordinal.has_value()) {
            statement.bind_u64(4, *result.failed_ordinal);
        } else {
            statement.bind_null(4);
        }

        statement.bind_u64(5, result.performed_tick_advances);
        statement.bind_u64(6, result.final_tick);
        statement.bind_u64(7, result.final_session_sequence);
        statement.bind_u64(
            8,
            static_cast<std::uint64_t>(result.final_pending_command_count));
        statement.bind_u64(9, result.final_world_fingerprint.value());
        statement.bind_digest(10, result.replayed_head_digest);
        statement.step_done();
        transaction.commit();
    }

    [[nodiscard]] std::vector<EvidenceArchiveSummary> list_archives(
        const std::size_t limit,
        const std::optional<std::int64_t> before_archive_id) const
    {
        validate_limit(limit);

        if (before_archive_id.has_value() && *before_archive_id <= 0) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "archive cursor must be positive"};
        }

        std::string sql =
            "SELECT archive_id, source, imported_at_utc, record_count, "
            "archive_digest, trusted_head, declared_head "
            "FROM evidence_archives";

        if (before_archive_id.has_value()) {
            sql += " WHERE archive_id < ?";
        }

        sql += " ORDER BY archive_id DESC LIMIT ?";

        Statement statement{database_, sql, "list evidence archives"};
        int parameter = 1;

        if (before_archive_id.has_value()) {
            statement.bind_int64(parameter++, *before_archive_id);
        }

        statement.bind_int64(parameter, checked_int64(limit, "archive query limit"));

        std::vector<EvidenceArchiveSummary> result;
        result.reserve(limit);

        while (statement.step_row()) {
            result.push_back(EvidenceArchiveSummary{
                .archive_id = statement.column_int64(0),
                .source = statement.column_text(1),
                .imported_at_utc = statement.column_text(2),
                .record_count = checked_size(
                    statement.column_int64(3),
                    "archive record count"),
                .archive_digest = statement.column_digest(
                    4,
                    "archive digest"),
                .trusted_head = statement.column_digest(
                    5,
                    "archive trusted head"),
                .chain_head = statement.column_digest(
                    6,
                    "archive chain head"),
            });
        }

        return result;
    }

    [[nodiscard]] std::vector<StoredEvidenceRecord> query_evidence(
        const std::int64_t archive_id,
        const EvidenceQuery& query) const
    {
        if (archive_id <= 0) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "archive identifier must be positive"};
        }

        validate_limit(query.limit);

        if (query.minimum_observed_tick.has_value() &&
            query.maximum_observed_tick.has_value() &&
            *query.minimum_observed_tick > *query.maximum_observed_tick) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "minimum observed tick exceeds maximum observed tick"};
        }

        std::string sql =
            "SELECT record_digest, encoded_record FROM command_evidence "
            "WHERE archive_id = ?";

        if (query.client_id.has_value()) {
            sql += " AND client_id = ?";
        }
        if (query.session_id.has_value()) {
            sql += " AND session_id = ?";
        }
        if (query.rejection_code.has_value()) {
            sql += " AND rejection_code = ?";
        }
        if (query.queue_outcome.has_value()) {
            sql += " AND queue_outcome = ?";
        }
        if (query.minimum_observed_tick.has_value()) {
            sql += " AND observed_tick >= ?";
        }
        if (query.maximum_observed_tick.has_value()) {
            sql += " AND observed_tick <= ?";
        }
        if (query.after_ordinal.has_value()) {
            sql += " AND ordinal > ?";
        }

        sql += " ORDER BY ordinal ASC LIMIT ?";

        Statement statement{database_, sql, "query command evidence"};
        int parameter = 1;
        statement.bind_int64(parameter++, archive_id);

        if (query.client_id.has_value()) {
            statement.bind_u64(parameter++, query.client_id->value());
        }
        if (query.session_id.has_value()) {
            statement.bind_u64(parameter++, query.session_id->value());
        }
        if (query.rejection_code.has_value()) {
            statement.bind_int(parameter++, static_cast<int>(*query.rejection_code));
        }
        if (query.queue_outcome.has_value()) {
            statement.bind_int(parameter++, static_cast<int>(*query.queue_outcome));
        }
        if (query.minimum_observed_tick.has_value()) {
            statement.bind_u64(parameter++, *query.minimum_observed_tick);
        }
        if (query.maximum_observed_tick.has_value()) {
            statement.bind_u64(parameter++, *query.maximum_observed_tick);
        }
        if (query.after_ordinal.has_value()) {
            statement.bind_u64(parameter++, *query.after_ordinal);
        }

        statement.bind_int64(
            parameter,
            checked_int64(query.limit, "evidence query limit"));

        std::vector<StoredEvidenceRecord> result;
        result.reserve(query.limit);

        while (statement.step_row()) {
            const auto stored_digest = statement.column_digest(
                0,
                "stored evidence digest");
            const auto encoded = statement.column_blob(1);
            const auto record = command::decode_command_evidence(encoded);

            if (command::digest_command_evidence(record) != stored_digest) {
                throw InvestigationStoreError{
                    InvestigationStoreErrorCode::sqlite_failure,
                    "stored evidence digest does not match encoded record"};
            }

            result.push_back(StoredEvidenceRecord{
                .archive_id = archive_id,
                .record_digest = stored_digest,
                .record = record,
            });
        }

        return result;
    }

    [[nodiscard]] std::vector<EvidenceSessionSummary> list_sessions(
        const std::int64_t archive_id) const
    {
        if (archive_id <= 0) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "archive identifier must be positive"};
        }

        Statement statement{
            database_,
            "SELECT client_id, session_id, first_ordinal, last_ordinal, "
            "first_observed_tick, last_observed_tick, accepted_count, rejected_count "
            "FROM archive_sessions WHERE archive_id = ? "
            "ORDER BY client_id ASC, session_id ASC",
            "list archive sessions"};
        statement.bind_int64(1, archive_id);

        std::vector<EvidenceSessionSummary> result;

        while (statement.step_row()) {
            result.push_back(EvidenceSessionSummary{
                .client_id = command::ClientId{
                    statement.column_u64(0, "session client identifier")},
                .session_id = command::SessionId{
                    statement.column_u64(1, "session identifier")},
                .first_ordinal = statement.column_u64(2, "first ordinal"),
                .last_ordinal = statement.column_u64(3, "last ordinal"),
                .first_observed_tick = statement.column_u64(
                    4,
                    "first observed tick"),
                .last_observed_tick = statement.column_u64(
                    5,
                    "last observed tick"),
                .accepted_count = checked_size(
                    statement.column_int64(6),
                    "accepted count"),
                .rejected_count = checked_size(
                    statement.column_int64(7),
                    "rejected count"),
            });
        }

        return result;
    }

    [[nodiscard]] std::optional<command::CommandEvidenceReplayResult>
    replay_result(const std::int64_t archive_id) const
    {
        if (archive_id <= 0) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "archive identifier must be positive"};
        }

        Statement statement{
            database_,
            "SELECT replay_code, processed_records, failed_ordinal, "
            "performed_tick_advances, final_tick, final_session_sequence, "
            "final_pending_command_count, final_world_fingerprint, "
            "replayed_head_digest FROM replay_results WHERE archive_id = ?",
            "read replay result"};
        statement.bind_int64(1, archive_id);

        if (!statement.step_row()) {
            return std::nullopt;
        }

        std::optional<std::uint64_t> failed_ordinal;

        if (!statement.column_is_null(2)) {
            failed_ordinal = statement.column_u64(2, "failed ordinal");
        }

        return command::CommandEvidenceReplayResult{
            .code = static_cast<command::CommandEvidenceReplayCode>(
                statement.column_int(0)),
            .processed_records = checked_size(
                statement.column_int64(1),
                "processed replay records"),
            .failed_ordinal = failed_ordinal,
            .performed_tick_advances = statement.column_u64(
                3,
                "performed tick advances"),
            .final_tick = statement.column_u64(4, "final replay tick"),
            .final_session_sequence = statement.column_u64(
                5,
                "final session sequence"),
            .final_pending_command_count = checked_size_u64(
                statement.column_u64(6, "final pending command count"),
                "final pending command count"),
            .final_world_fingerprint = simulation::StateFingerprint{
                statement.column_u64(7, "final world fingerprint")},
            .replayed_head_digest = statement.column_digest(
                8,
                "replayed head digest"),
        };
    }

    [[nodiscard]] std::size_t archive_count() const
    {
        return count_rows("evidence_archives");
    }

    [[nodiscard]] std::size_t evidence_record_count() const
    {
        return count_rows("command_evidence");
    }

private:
    static void validate_limit(const std::size_t limit)
    {
        if (limit == 0 || limit > 1'000) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::invalid_argument,
                "query limit must be between one and 1000"};
        }
    }

    [[nodiscard]] std::size_t count_rows(const std::string_view table) const
    {
        const std::string sql = "SELECT COUNT(*) FROM " + std::string{table};
        Statement statement{database_, sql, "count investigation rows"};

        if (!statement.step_row()) {
            throw InvestigationStoreError{
                InvestigationStoreErrorCode::sqlite_failure,
                "count query did not return a row"};
        }

        return checked_size(statement.column_int64(0), "row count");
    }

    sqlite3* database_{nullptr};
};

InvestigationStore::InvestigationStore(const std::filesystem::path& path)
    : impl_{std::make_unique<Impl>(path)}
{
}

InvestigationStore::~InvestigationStore() = default;
InvestigationStore::InvestigationStore(InvestigationStore&&) noexcept = default;
InvestigationStore& InvestigationStore::operator=(
    InvestigationStore&&) noexcept = default;

int InvestigationStore::schema_version() const
{
    return impl_->schema_version();
}

EvidenceArchiveImportResult InvestigationStore::import_archive(
    const std::span<const std::byte> encoded_archive,
    const security::Sha256Digest trusted_head,
    std::string source)
{
    return impl_->import_archive(
        encoded_archive,
        trusted_head,
        std::move(source));
}

void InvestigationStore::record_replay_result(
    const std::int64_t archive_id,
    const command::CommandEvidenceReplayResult& result)
{
    impl_->record_replay_result(archive_id, result);
}

std::vector<EvidenceArchiveSummary> InvestigationStore::list_archives(
    const std::size_t limit,
    const std::optional<std::int64_t> before_archive_id) const
{
    return impl_->list_archives(limit, before_archive_id);
}

std::vector<StoredEvidenceRecord> InvestigationStore::query_evidence(
    const std::int64_t archive_id,
    const EvidenceQuery& query) const
{
    return impl_->query_evidence(archive_id, query);
}

std::vector<EvidenceSessionSummary> InvestigationStore::list_sessions(
    const std::int64_t archive_id) const
{
    return impl_->list_sessions(archive_id);
}

std::optional<command::CommandEvidenceReplayResult>
InvestigationStore::replay_result(const std::int64_t archive_id) const
{
    return impl_->replay_result(archive_id);
}

std::size_t InvestigationStore::archive_count() const
{
    return impl_->archive_count();
}

std::size_t InvestigationStore::evidence_record_count() const
{
    return impl_->evidence_record_count();
}

}
