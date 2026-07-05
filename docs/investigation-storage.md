# Investigation storage and query layer

Tickline stores verified command-evidence archives in SQLite for local forensic analysis. The storage boundary accepts only archives that have already passed the canonical archive decoder and SHA-256 chain verification against a caller-supplied trusted head.

## Trust boundary

The database is not treated as evidence authority. Import follows this order:

1. decode the complete archive from untrusted bytes;
2. validate archive and record schemas;
3. verify every record invariant;
4. verify the SHA-256 record chain against the trusted head;
5. calculate the SHA-256 digest of the complete encoded archive;
6. begin an immediate SQLite transaction;
7. persist archive metadata, canonical record bytes, query columns, and session summaries;
8. commit only after every row succeeds.

Malformed or unverifiable archives are rejected before a transaction begins. Any SQLite failure rolls back the complete import.

## Schema version 1

`schema_migrations` records applied migrations. A database whose migration version is newer than the running binary is rejected.

`evidence_archives` stores provenance, the complete-archive digest, the trusted and declared chain heads, schema versions, and record count. The archive digest is unique and provides idempotent imports.

`command_evidence` stores each canonical 160-byte evidence record together with its record digest and denormalized fields used for investigation filters. The primary key is `(archive_id, ordinal)`. Record digests are unique within an archive.

`archive_sessions` stores per-client and per-session ordinal and tick ranges plus accepted and rejected submission counts.

`replay_results` stores the latest deterministic replay result associated with an imported archive.

## Unsigned integer representation

SQLite INTEGER values are signed 64-bit values. Tickline protocol and evidence fields are unsigned 64-bit values. To avoid narrowing or reinterpretation, unsigned values are stored as fixed-width eight-byte big-endian blobs. Fixed-width big-endian ordering preserves unsigned numeric ordering for equality, range filters, and pagination.

## Query contract

Archive pagination is descending by database archive identifier and uses an exclusive `before_archive_id` cursor.

Evidence pagination is ascending by evidence ordinal and uses an exclusive `after_ordinal` cursor. Queries can filter by client identity, session identity, rejection code, queue outcome, and observed-tick range. Limits must be between 1 and 1000.

Each returned record is decoded from its canonical stored bytes and its SHA-256 digest is recalculated. A mismatch is reported as a storage-integrity failure rather than returning untrusted data.

## Operational constraints

The current layer is intended for local investigation workloads. It uses SQLite full-mutex mode, foreign-key enforcement, a five-second busy timeout, immediate write transactions, and prepared statements.

The current release does not provide multi-process ingestion coordination, remote database access, retention policies, encryption at rest, access control, or a production investigation service API.
