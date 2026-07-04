# Protocol Compatibility Contract

This document defines which Tickline protocol properties are stable and how
future changes must be introduced without silently reinterpreting recorded or
live bytes.

## Independent version domains

Tickline has two version domains:

1. `protocol_version` controls frame-level representation;
2. `command_schema_version` controls command meaning after a frame has decoded.

A command schema change does not permit changing frame byte order, header
layout, or message identifiers. A frame protocol change does not automatically
change command semantics.

## Protocol version 1 invariants

The following values are immutable within protocol version 1:

```text
magic bytes             54 49 43 4b (ASCII TICK)
byte order              big-endian
header size             16 bytes
frame_size meaning      complete header plus payload
command message type    1
reserved flags          must be zero
command payload size    60 bytes
command frame size      76 bytes
```

The command payload offsets and widths documented in `docs/protocol.md` are also
immutable within protocol version 1.

## Stable parser failures

`ParseErrorCode` numeric values and machine-readable names are compatibility
contracts. Existing values must not be renumbered, renamed, or reused.

New failure classes may be appended with new numeric values when they do not
change the classification of previously defined inputs. Reordering validation
so that the same malformed input produces a different existing error is a
compatibility change and requires explicit review.

## Compatible additions

A change may remain compatible with protocol version 1 when all existing byte
sequences retain their previous interpretation. Examples include:

- adding a new message type with a previously unused numeric identifier;
- adding internal parser APIs that preserve the same wire contract;
- adding stricter trusted configuration outside the wire representation;
- adding tests, fuzz seeds, telemetry, or diagnostics;
- adding a new command schema version inside the existing command message.

A newly allocated message type must have its own exact payload contract and must
not reinterpret message type `1`.

## Changes requiring a new protocol version

The following require a new protocol version or a separately negotiated framing
mode:

- changing magic bytes;
- changing byte order;
- changing header size or field offsets;
- changing the meaning of `frame_size`;
- reusing an existing message type for a different payload;
- changing reserved flag behavior without negotiation;
- changing the command message payload length or field offsets;
- accepting bytes that version 1 intentionally classifies as malformed when the
  change would make recorded evidence ambiguous.

## Strict and stream API consistency

The strict parser and incremental stream parser share the same header decoder
and limits. They differ only in boundary ownership:

- strict parsing requires exactly one complete frame and rejects trailing data;
- stream parsing buffers incomplete data and emits each concatenated frame
  independently.

A stream parser must not weaken header, size, type, flag, or payload validation.

## Evidence and replay implications

Protocol bytes may later become investigation evidence. Compatibility changes
must therefore consider:

- whether old frames remain decodable;
- whether parser errors keep the same meaning;
- whether a recorded command maps to the same typed envelope;
- whether forensic tools can identify unsupported versions explicitly;
- whether migration would preserve original bytes rather than silently rewrite
  them.

Unsupported versions must fail explicitly. Silent fallback to a different
layout is prohibited.

## Enforcement

The compatibility contract is enforced by:

- `tickline_protocol_compatibility_tests` for constants, numeric assignments,
  names, and representative wire offsets;
- parser unit tests for deterministic rejection order;
- codec round-trip tests;
- the committed fuzz regression corpus;
- code review of any protocol or command-schema version change.

A pull request that intentionally changes a compatibility contract must update
this document, `docs/protocol.md`, tests, and release notes in the same change.
