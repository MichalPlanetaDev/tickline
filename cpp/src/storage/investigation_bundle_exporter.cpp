#include "tickline/storage/investigation_bundle_exporter.hpp"

#include "tickline/command/command_evidence.hpp"
#include "tickline/command/command_envelope.hpp"
#include "tickline/command/command_validator.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tickline::storage {
namespace {

using SessionKey = std::pair<std::uint64_t, std::uint64_t>;

struct SessionProjection final {
    bool initialized{false};
    std::uint64_t last_committed_sequence{0};
    std::uint64_t first_target_tick{0};
    std::uint64_t last_target_tick{0};
};

[[noreturn]] void fail(
    const InvestigationBundleExportErrorCode code,
    std::string message)
{
    throw InvestigationBundleExportError{
        code,
        std::move(message),
    };
}

void append_json_string(
    std::string& output,
    const std::string_view value)
{
    static constexpr char hexadecimal[] = "0123456789abcdef";

    output.push_back('"');

    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;

        case '\\':
            output += "\\\\";
            break;

        case '\b':
            output += "\\b";
            break;

        case '\f':
            output += "\\f";
            break;

        case '\n':
            output += "\\n";
            break;

        case '\r':
            output += "\\r";
            break;

        case '\t':
            output += "\\t";
            break;

        default:
            if (character < 0x20) {
                output += "\\u00";
                output.push_back(hexadecimal[character >> 4]);
                output.push_back(hexadecimal[character & 0x0f]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }

    output.push_back('"');
}

void append_indent(std::string& output, const std::size_t count)
{
    output.append(count, ' ');
}

void append_string_field(
    std::string& output,
    const std::size_t indent,
    const std::string_view name,
    const std::string_view value,
    const bool comma)
{
    append_indent(output, indent);
    append_json_string(output, name);
    output += ": ";
    append_json_string(output, value);
    output += comma ? ",\n" : "\n";
}

void append_raw_field(
    std::string& output,
    const std::size_t indent,
    const std::string_view name,
    const std::string_view value,
    const bool comma)
{
    append_indent(output, indent);
    append_json_string(output, name);
    output += ": ";
    output += value;
    output += comma ? ",\n" : "\n";
}

[[nodiscard]] std::string normalize_timestamp(std::string value)
{
    if (value.size() == 19 && value[10] == ' ') {
        value[10] = 'T';
        value.push_back('Z');
    }

    return value;
}

[[nodiscard]] std::string_view command_type_name(
    const command::CommandType type)
{
    switch (type) {
    case command::CommandType::set_velocity:
        return "set_velocity";
    }

    return "unknown";
}

[[nodiscard]] bool record_was_accepted(
    const command::CommandEvidenceRecord& record)
{
    return record.entry.rejection_code ==
               command::CommandRejectionCode::none &&
        record.entry.queue_outcome ==
               command::CommandQueueOutcome::accepted;
}

[[nodiscard]] std::string_view rejection_name(
    const command::CommandEvidenceRecord& record)
{
    if (record.entry.rejection_code !=
        command::CommandRejectionCode::none) {
        return command::command_rejection_code_name(
            record.entry.rejection_code);
    }

    if (record.entry.queue_outcome !=
        command::CommandQueueOutcome::accepted) {
        return command::command_queue_outcome_name(
            record.entry.queue_outcome);
    }

    return "none";
}

void require_json_integer(
    const std::uint64_t value,
    const std::string_view description)
{
    if (value >
        static_cast<std::uint64_t>(
            std::numeric_limits<int>::max())) {
        fail(
            InvestigationBundleExportErrorCode::
                json_integer_out_of_range,
            std::string{description} +
                " exceeds the Unity JSON integer range");
    }
}

[[nodiscard]] EvidenceArchiveSummary find_archive(
    const InvestigationStore& store,
    const std::int64_t archive_id)
{
    std::optional<std::int64_t> before_archive_id;

    while (true) {
        const auto page =
            store.list_archives(100, before_archive_id);

        if (page.empty()) {
            break;
        }

        for (const auto& archive : page) {
            if (archive.archive_id == archive_id) {
                return archive;
            }
        }

        before_archive_id = page.back().archive_id;
    }

    fail(
        InvestigationBundleExportErrorCode::archive_not_found,
        "investigation archive was not found");
}

[[nodiscard]] std::vector<StoredEvidenceRecord>
load_all_evidence(
    const InvestigationStore& store,
    const std::int64_t archive_id)
{
    constexpr std::size_t page_size = 100;

    std::vector<StoredEvidenceRecord> records;
    std::optional<std::uint64_t> after_ordinal;

    while (true) {
        EvidenceQuery query;
        query.after_ordinal = after_ordinal;
        query.limit = page_size;

        auto page = store.query_evidence(
            archive_id,
            query);

        if (page.empty()) {
            break;
        }

        for (auto& stored : page) {
            if (!records.empty() &&
                stored.record.ordinal <=
                    records.back().record.ordinal) {
                fail(
                    InvestigationBundleExportErrorCode::
                        evidence_chain_mismatch,
                    "evidence query returned non-increasing ordinals");
            }

            after_ordinal = stored.record.ordinal;
            records.push_back(std::move(stored));
        }

        if (page.size() < page_size) {
            break;
        }
    }

    return records;
}

}

InvestigationBundleExportError::InvestigationBundleExportError(
    const InvestigationBundleExportErrorCode code,
    std::string message)
    : std::runtime_error{std::move(message)},
      code_{code}
{
}

InvestigationBundleExportErrorCode
InvestigationBundleExportError::code() const noexcept
{
    return code_;
}

std::string export_investigation_bundle_json(
    const InvestigationStore& store,
    const std::int64_t archive_id)
{
    if (archive_id <= 0) {
        fail(
            InvestigationBundleExportErrorCode::
                invalid_archive_id,
            "archive identifier must be positive");
    }

    const auto archive = find_archive(store, archive_id);
    const auto replay = store.replay_result(archive_id);

    if (!replay.has_value()) {
        fail(
            InvestigationBundleExportErrorCode::
                replay_result_missing,
            "archive does not have replay metadata");
    }

    const auto evidence =
        load_all_evidence(store, archive_id);

    if (evidence.size() != archive.record_count) {
        fail(
            InvestigationBundleExportErrorCode::
                evidence_count_mismatch,
            "stored evidence count does not match archive metadata");
    }

    const auto initial_head =
        evidence.empty()
            ? security::Sha256Digest{}
            : evidence.front().record.previous_record_digest;

    auto expected_digest = initial_head;

    std::map<SessionKey, SessionProjection> projections;
    std::size_t accepted_count = 0;
    std::size_t rejected_count = 0;

    for (const auto& stored : evidence) {
        const auto& record = stored.record;
        const auto& envelope = record.entry.envelope;

        require_json_integer(record.ordinal, "evidence ordinal");

        if (record.previous_record_digest != expected_digest) {
            fail(
                InvestigationBundleExportErrorCode::
                    evidence_chain_mismatch,
                "stored evidence chain contains a broken link");
        }

        expected_digest = stored.record_digest;

        const SessionKey key{
            envelope.client_id.value(),
            envelope.session_id.value(),
        };

        auto& projection = projections[key];

        if (!projection.initialized) {
            projection.initialized = true;
            projection.first_target_tick =
                envelope.target_tick;
        }

        projection.last_target_tick =
            envelope.target_tick;
        projection.last_committed_sequence =
            record.entry.session_sequence_after;

        if (record_was_accepted(record)) {
            ++accepted_count;
        } else {
            ++rejected_count;
        }
    }

    if (expected_digest != archive.chain_head ||
        archive.trusted_head != archive.chain_head) {
        fail(
            InvestigationBundleExportErrorCode::
                evidence_chain_mismatch,
            "archive chain heads do not match stored evidence");
    }

    const auto sessions = store.list_sessions(archive_id);

    if (sessions.size() != projections.size()) {
        fail(
            InvestigationBundleExportErrorCode::
                session_summary_mismatch,
            "session summaries do not match stored evidence");
    }

    std::size_t session_accepted_count = 0;
    std::size_t session_rejected_count = 0;

    for (const auto& session : sessions) {
        const SessionKey key{
            session.client_id.value(),
            session.session_id.value(),
        };

        if (!projections.contains(key)) {
            fail(
                InvestigationBundleExportErrorCode::
                    session_summary_mismatch,
                "session summary references no evidence records");
        }

        require_json_integer(
            static_cast<std::uint64_t>(
                session.accepted_count),
            "accepted command count");

        require_json_integer(
            static_cast<std::uint64_t>(
                session.rejected_count),
            "rejected command count");

        session_accepted_count += session.accepted_count;
        session_rejected_count += session.rejected_count;
    }

    if (session_accepted_count != accepted_count ||
        session_rejected_count != rejected_count) {
        fail(
            InvestigationBundleExportErrorCode::
                session_summary_mismatch,
            "session outcome totals do not match evidence");
    }

    require_json_integer(
        static_cast<std::uint64_t>(accepted_count),
        "replay accepted command count");

    require_json_integer(
        static_cast<std::uint64_t>(rejected_count),
        "replay rejected command count");

    std::string output;
    output.reserve(4096 + evidence.size() * 640);

    output += "{\n";

    append_raw_field(
        output,
        2,
        "schemaVersion",
        std::to_string(investigation_bundle_schema_version),
        true);

    append_string_field(
        output,
        2,
        "archiveDigest",
        archive.archive_digest.to_hex(),
        true);

    append_string_field(
        output,
        2,
        "initialHeadDigest",
        initial_head.to_hex(),
        true);

    append_string_field(
        output,
        2,
        "trustedHeadDigest",
        archive.trusted_head.to_hex(),
        true);

    append_string_field(
        output,
        2,
        "importedAtUtc",
        normalize_timestamp(archive.imported_at_utc),
        true);

    output += "  \"sessions\": [\n";

    for (std::size_t index = 0;
         index < sessions.size();
         ++index) {
        const auto& session = sessions[index];
        const SessionKey key{
            session.client_id.value(),
            session.session_id.value(),
        };
        const auto& projection = projections.at(key);

        output += "    {\n";

        append_string_field(
            output,
            6,
            "clientId",
            std::to_string(session.client_id.value()),
            true);

        append_string_field(
            output,
            6,
            "sessionId",
            std::to_string(session.session_id.value()),
            true);

        append_string_field(
            output,
            6,
            "lastCommittedSequence",
            std::to_string(
                projection.last_committed_sequence),
            true);

        append_string_field(
            output,
            6,
            "firstTargetTick",
            std::to_string(projection.first_target_tick),
            true);

        append_string_field(
            output,
            6,
            "lastTargetTick",
            std::to_string(projection.last_target_tick),
            true);

        append_raw_field(
            output,
            6,
            "acceptedCommands",
            std::to_string(session.accepted_count),
            true);

        append_raw_field(
            output,
            6,
            "rejectedCommands",
            std::to_string(session.rejected_count),
            false);

        output += "    }";
        output += index + 1 < sessions.size()
            ? ",\n"
            : "\n";
    }

    output += "  ],\n";
    output += "  \"evidence\": [\n";

    for (std::size_t index = 0;
         index < evidence.size();
         ++index) {
        const auto& stored = evidence[index];
        const auto& record = stored.record;
        const auto& envelope = record.entry.envelope;
        const bool accepted = record_was_accepted(record);

        output += "    {\n";

        append_raw_field(
            output,
            6,
            "ordinal",
            std::to_string(record.ordinal),
            true);

        append_string_field(
            output,
            6,
            "clientId",
            std::to_string(envelope.client_id.value()),
            true);

        append_string_field(
            output,
            6,
            "sessionId",
            std::to_string(envelope.session_id.value()),
            true);

        append_string_field(
            output,
            6,
            "sessionSequence",
            std::to_string(envelope.sequence),
            true);

        append_string_field(
            output,
            6,
            "targetTick",
            std::to_string(envelope.target_tick),
            true);

        append_string_field(
            output,
            6,
            "commandType",
            command_type_name(envelope.type),
            true);

        append_string_field(
            output,
            6,
            "outcome",
            accepted ? "accepted" : "rejected",
            true);

        append_string_field(
            output,
            6,
            "rejectionCode",
            rejection_name(record),
            true);

        append_string_field(
            output,
            6,
            "previousDigest",
            record.previous_record_digest.to_hex(),
            true);

        append_string_field(
            output,
            6,
            "recordDigest",
            stored.record_digest.to_hex(),
            false);

        output += "    }";
        output += index + 1 < evidence.size()
            ? ",\n"
            : "\n";
    }

    output += "  ],\n";
    output += "  \"replay\": {\n";

    append_raw_field(
        output,
        4,
        "verified",
        replay->verified() ? "true" : "false",
        true);

    append_string_field(
        output,
        4,
        "finalTick",
        std::to_string(replay->final_tick),
        true);

    append_string_field(
        output,
        4,
        "finalWorldFingerprint",
        std::to_string(
            replay->final_world_fingerprint.value()),
        true);

    append_raw_field(
        output,
        4,
        "acceptedCommands",
        std::to_string(accepted_count),
        true);

    append_raw_field(
        output,
        4,
        "rejectedCommands",
        std::to_string(rejected_count),
        false);

    output += "  }\n";
    output += "}\n";

    return output;
}

}
