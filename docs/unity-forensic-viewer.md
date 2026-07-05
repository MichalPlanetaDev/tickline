# Unity forensic replay viewer

## Purpose

The Unity forensic replay viewer is an investigation client for trusted
Tickline investigation exports.

It presents deterministic replay information, command decisions, evidence
records, session state, and integrity findings without modifying the source
evidence.

The viewer is not:

- an authoritative simulation server;
- a live command processor;
- a network protocol endpoint;
- a production anti-cheat client;
- an evidence-signing authority.

## Architectural role

The viewer sits after the authoritative and forensic pipeline:

    untrusted client input
            |
            v
    protocol boundary
            |
            v
    authoritative command pipeline
            |
            v
    canonical evidence records
            |
            v
    SHA-256 evidence chain
            |
            v
    verified evidence archive
            |
            v
    investigation storage
            |
            v
    deterministic investigation bundle
            |
            v
    Unity forensic replay viewer

The Unity application consumes an exported investigation bundle. It does not
read directly from live client connections or mutate the authoritative world.

## Trust boundary

Every imported JSON document is treated as untrusted input.

The loading boundary validates:

- supported schema version;
- required archive metadata;
- required client and session identities;
- unsigned 64-bit decimal values;
- SHA-256 digest formatting;
- contiguous zero-based evidence ordinals;
- evidence-to-session references;
- evidence-chain link continuity;
- trusted final chain head;
- accepted and rejected command totals;
- replay summary consistency;
- valid ISO-8601 import timestamp.

A structurally invalid bundle is not admitted to the viewer state.

Validation findings use stable codes and paths so that the user interface,
tests, logs, and future automation can report the same failure consistently.

## Security interpretation

The current validator performs structural and chain-link validation.

It verifies that:

- every evidence record links to the expected previous digest;
- the final evidence digest matches the trusted head supplied by the bundle;
- evidence records reference declared sessions;
- replay command totals agree with the evidence outcomes;
- large protocol and simulation integers remain lossless.

The current Unity implementation does not independently recompute every
canonical evidence-record SHA-256 digest from all original record fields.

Therefore, successful Unity validation means that the bundle is structurally
consistent with its supplied trusted head. It does not independently establish:

- evidence authorship;
- archive provenance;
- digital-signature validity;
- client identity;
- transport authenticity;
- key ownership.

Digital signatures, key management, remote identity verification, and
independent canonical digest recomputation remain outside this initial viewer
foundation.

## Bundle schema

The supported bundle schema version is `1`.

The top-level document contains:

- `schemaVersion`;
- `archiveDigest`;
- `initialHeadDigest`;
- `trustedHeadDigest`;
- `importedAtUtc`;
- `sessions`;
- `evidence`;
- `replay`.

### Archive metadata

`archiveDigest` identifies the imported investigation archive.

`initialHeadDigest` is the chain head that precedes the first exported evidence
record.

`trustedHeadDigest` is the expected final evidence-chain head.

`importedAtUtc` records when the archive was imported into investigation
storage.

### Session summaries

Each session entry contains:

- client identity;
- session identity;
- last committed sequence;
- first target tick;
- last target tick;
- accepted command count;
- rejected command count.

The pair of client identity and session identity must be unique.

### Evidence records

Each evidence entry contains:

- zero-based ordinal;
- client identity;
- session identity;
- session sequence;
- target tick;
- command type;
- outcome;
- rejection code;
- previous digest;
- record digest.

Evidence outcomes are limited to `accepted` and `rejected`.

Evidence ordinals must be contiguous and ordered.

Each evidence record must reference a session declared in the same bundle.

### Replay summary

The replay summary contains:

- verification status;
- final simulation tick;
- final world fingerprint;
- accepted command count;
- rejected command count.

The accepted and rejected counts must match the exported evidence records.

## Lossless integer encoding

JSON numbers cannot safely represent every unsigned 64-bit integer in all
consumers.

The bundle therefore represents protocol and simulation values such as
sequences, ticks, and world fingerprints as decimal strings.

Examples:

    "sessionSequence": "18446744073709551615"
    "targetTick": "120"
    "finalWorldFingerprint": "991239472193847219"

The Unity validator parses these values using invariant-culture unsigned
64-bit parsing.

Negative numbers, signs, whitespace, decimals, exponent notation, and values
outside the unsigned 64-bit range are rejected.

## Digest representation

SHA-256 digests are represented as exactly 64 lowercase hexadecimal
characters.

Accepted example:

    aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

Rejected representations include:

- uppercase hexadecimal;
- prefixes such as `0x`;
- shortened values;
- non-hexadecimal characters;
- embedded whitespace.

## Unity project structure

The Unity project is located at:

    unity/TicklineForensics

Important paths:

    unity/TicklineForensics/Assets/Tickline/Forensics/Runtime
    unity/TicklineForensics/Assets/Tickline/Forensics/Tests/EditMode
    unity/TicklineForensics/Assets/Tickline/Forensics/Tests/EditMode/Fixtures
    unity/TicklineForensics/Packages
    unity/TicklineForensics/ProjectSettings

Runtime code belongs to the `Tickline.Forensics` assembly.

EditMode tests belong to the `Tickline.Forensics.Tests` assembly.

The test assembly references the runtime assembly but the runtime assembly does
not depend on the tests.

## Loading flow

The current loading sequence is:

1. Receive a file path or JSON string.
2. Reject an empty path or empty JSON input.
3. Read the file without modifying it.
4. Deserialize the JSON into the schema-versioned bundle model.
5. Run structural validation.
6. Return the bundle together with all validation findings.
7. Admit the bundle to future viewer state only when validation succeeds.

File-system and deserialization failures are converted into stable validation
results rather than escaping as unhandled viewer exceptions.

## Validation findings

Each validation issue contains:

- stable code;
- JSON-style path;
- human-readable message.

Example:

    Code: evidence_chain_broken
    Path: $.evidence[4].previousDigest
    Message: The previous digest does not match the chain head.

Multiple findings may be returned in one validation pass.

This allows the investigation interface to display the complete set of
detected problems instead of stopping after the first malformed field.

## Test fixtures

The repository contains a deterministic valid bundle fixture with:

- one session;
- one accepted command;
- one rejected command;
- two linked evidence records;
- a matching trusted head;
- a replay summary with matching command totals;
- the maximum unsigned 64-bit value as the world fingerprint.

EditMode tests cover:

- successful fixture loading;
- evidence-link tampering;
- invalid unsigned integer input;
- unsupported schema versions;
- empty JSON input.

Additional fixtures will be introduced for timeline playback, state snapshots,
export compatibility, and full integrity visualization.

## Running Unity tests

Because the Windows Unity editor rejects case-sensitive WSL project paths, the test runner stages a disposable project copy on the case-insensitive Windows temporary filesystem. The WSL repository remains the authoritative source and Unity does not modify it.

From WSL:

    bash scripts/check-unity.sh

A specific Unity editor can be selected explicitly:

    TICKLINE_UNITY_EDITOR="/mnt/c/Program Files/Unity/Hub/Editor/6000.3.2f1/Editor/Unity.exe" bash scripts/check-unity.sh

The script:

- locates the Unity editor;
- opens the project in batch mode;
- runs EditMode tests;
- writes NUnit-compatible XML results to `/tmp`;
- writes the Unity log to `/tmp`;
- fails when Unity does not produce a passing test result.

## Current implementation

The current viewer foundation includes:

- Unity project metadata;
- runtime and test assembly definitions;
- serializable investigation-bundle model;
- file-based bundle loading;
- JSON-based bundle loading;
- stable validation results;
- schema validation;
- identity validation;
- unsigned 64-bit validation;
- digest-format validation;
- evidence ordinal validation;
- session-reference validation;
- evidence-chain link validation;
- trusted-head validation;
- replay-count validation;
- deterministic JSON fixture;
- Unity EditMode tests;
- WSL Unity test runner.

## Planned v0.7.0 implementation

The remaining milestone work includes:

- deterministic investigation-bundle export from the native storage layer;
- Unity import workflow and file selection;
- replay timeline model;
- play, pause, step, seek, and speed controls;
- command inspection panel;
- session inspection panel;
- evidence inspection panel;
- replay-state inspection;
- evidence-chain visualization;
- tamper and integrity finding visualization;
- replay fixtures generated by the native implementation;
- compatibility tests between native export and Unity import;
- viewer scene and user-interface documentation.

## Non-goals

This milestone does not add:

- live multiplayer networking;
- authoritative command execution inside Unity;
- modification of evidence archives;
- automatic repair of malformed evidence;
- client authentication;
- transport encryption;
- evidence signing;
- key management;
- production deployment;
- production anti-cheat enforcement.

## Operational requirements

Investigation bundles should be treated as immutable inputs.

The viewer must not overwrite imported files.

Validation failures must remain visible to the investigator.

Unknown future schema versions must be rejected rather than interpreted using
the current schema.

Large integer fields must remain encoded as decimal strings through every
native export, JSON transport, Unity load, and user-interface path.

Evidence ordering must remain deterministic.

Viewer features must not silently change the meaning of an authoritative
decision or evidence outcome.

## Native investigation-bundle export

The committed Unity fixture is generated by the native C++ investigation-bundle exporter from a verified archive stored in SQLite. The exporter reads evidence through the integrity-checking storage query path, verifies chain continuity, requires replay metadata, normalizes the stored UTC timestamp, preserves unsigned protocol values as decimal strings, and emits deterministic schema-versioned JSON.

The C++ exporter test compares its output byte-for-byte with the committed Unity fixture. Unity EditMode tests then load the same fixture, validate its evidence chain, build the replay timeline, and exercise playback behavior.
