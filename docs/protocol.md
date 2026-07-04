# Protocol Boundary

Tickline's protocol layer converts bounded untrusted bytes into a typed
`CommandEnvelope`. It is the executable trust boundary before session replay
protection, authoritative world admission, and command evidence generation.

The protocol parser establishes structural validity only. It does not decide
whether a client identity, session, sequence, target tick, entity, or velocity
claim is authoritative-valid. Those decisions remain in `CommandValidator`,
`CommandSession`, and `World`.

## Implemented scope

The protocol boundary provides:

- a fixed 16-byte big-endian frame header;
- a configurable hard maximum frame size;
- one versioned command-envelope message type;
- exact fixed-size command payload decoding;
- stable parser error codes and names;
- strict rejection of truncated and trailing data;
- incremental stream reassembly across arbitrary chunk boundaries;
- multiple-frame emission from one stream chunk;
- fail-closed stream state after malformed input;
- a gateway into `AuthoritativeCommandPipeline`;
- compatibility contract tests;
- a libFuzzer target and committed regression corpus.

It does not provide a socket server, connection ownership, rate limiting,
transport authentication, encryption, compression, or protocol-level evidence
for malformed frames.

## Byte order and integer representation

Every multi-byte integer is encoded in big-endian byte order. Signed velocity
values use the two's-complement bit pattern of `int64` and are reconstructed
without numeric narrowing.

The parser does not use unaligned typed reads, client-sized recursive data, or
allocation directly proportional to an unchecked declared length.

## Frame format

Protocol version 1 uses this fixed header:

```text
offset  size  type     field
0       4     bytes    magic
4       2     uint16   protocol_version
6       2     uint16   header_size
8       4     uint32   frame_size
12      2     uint16   message_type
14      2     uint16   flags
16      N     bytes    payload
```

Header constants:

```text
magic             ASCII TICK
protocol_version  1
header_size       16
flags             0
maximum frame     4096 bytes by default
```

`frame_size` includes both the header and payload.

The current message allocation is:

| Value | Name | Payload |
|---:|---|---|
| `1` | `command_envelope` | fixed 60-byte command envelope |

Unknown message types are rejected. Message identifiers are compatibility
sensitive and must not be silently reused.

## Command-envelope payload

The command message encodes the complete authoritative command envelope:

```text
offset  size  type     field
0       2     uint16   command_schema_version
2       2     uint16   command_type
4       8     uint64   client_id
12      8     uint64   session_id
20      8     uint64   sequence
28      8     uint64   target_tick
36      8     uint64   entity_id
44      8     int64    velocity_x_mm_per_s
52      8     int64    velocity_y_mm_per_s
```

Payload size is exactly 60 bytes. The complete command frame is exactly 76
bytes.

The codec rejects an entity identifier of zero because `EntityId` cannot
represent it. Other structurally representable claims are preserved even when
they are not authoritative-valid. For example, an unsupported command schema,
unknown command type, zero client identifier, zero session identifier, or zero
sequence is decoded and then rejected by the authoritative validation layer.

This separation prevents the byte parser from duplicating session and
simulation policy.

## Stable parser errors

Parser errors have explicit numeric values and machine-readable names:

| Value | Name | Meaning |
|---:|---|---|
| `0` | `none` | no parser error |
| `1` | `truncated_header` | fewer than 16 bytes are available at strict EOF |
| `2` | `invalid_magic` | magic is not `TICK` |
| `3` | `unsupported_protocol_version` | protocol version is not supported |
| `4` | `invalid_header_size` | header size is not 16 |
| `5` | `frame_too_small` | declared frame size is smaller than the header |
| `6` | `frame_too_large` | declared frame size exceeds the configured limit |
| `7` | `reserved_flags_set` | a reserved flag bit is non-zero |
| `8` | `unknown_message_type` | message type is not allocated |
| `9` | `truncated_frame` | strict input or stream EOF ends before declared size |
| `10` | `trailing_data` | strict one-frame input contains extra bytes |
| `11` | `payload_size_mismatch` | command payload is not exactly 60 bytes |
| `12` | `invalid_entity_id` | entity identifier is zero |

Numeric assignments and names are compatibility contracts.

## Deterministic rejection order

When several fields are invalid, the parser reports the first failure in this
order:

1. minimum header availability;
2. magic;
3. protocol version;
4. header size;
5. declared minimum frame size;
6. configured maximum frame size;
7. reserved flags;
8. message type;
9. truncated or trailing strict-frame bytes;
10. message-specific payload size;
11. message-specific structural invariants.

This order is deterministic and covered by exact-code tests.

## Strict one-frame parser

`parse_frame()` accepts exactly one complete frame. It rejects missing bytes and
trailing bytes. This API is appropriate when the caller already owns one exact
frame boundary, such as a datagram or a length-delimited artifact.

Strict parsing must not be used directly on arbitrary TCP receive chunks,
because stream transports do not preserve application message boundaries.

## Incremental stream parser

`FrameStreamParser` accepts arbitrary byte chunks and reassembles frames without
assuming transport boundaries.

Its behavior is:

- an incomplete header is buffered rather than rejected;
- an incomplete payload is buffered rather than rejected;
- one input chunk may emit zero, one, or multiple owned frames;
- a complete frame is removed before parsing the next frame;
- buffer capacity is bounded by trusted `ProtocolLimits`;
- malformed complete headers enter a persistent failed state;
- calls after failure return the original parser error;
- `reset()` is required before reusing a failed parser;
- `finish()` classifies incomplete EOF as `truncated_header` or
  `truncated_frame`.

The parser emits `OwnedFrame` values so returned payloads do not reference an
internal buffer that will be reused for later network data.

Two valid concatenated frames are not trailing data in the stream API. They are
two independently emitted frames. The strict parser continues to reject the
same concatenation as `trailing_data`.

## Authoritative gateway

`AuthoritativeCommandGateway::submit()` performs:

```text
untrusted exact frame
  -> frame header validation
  -> exact frame boundary validation
  -> command-envelope decoding
  -> authoritative command pipeline
  -> command evidence for complete typed commands
```

A parser failure:

- returns a `ParseErrorCode`;
- does not call the authoritative pipeline;
- does not mutate session state;
- does not mutate world state;
- does not append command evidence because no complete typed command exists.

A structurally valid command that fails authoritative validation is different:
it returns no parser error, produces a normal `CommandSubmissionResult`, and is
recorded in the existing command evidence chain.

Protocol-level evidence for malformed frames requires a separate schema because
command evidence requires a complete `CommandEnvelope`.

## Limits and trusted configuration

The default maximum frame size is 4096 bytes. The configured maximum must be at
least the fixed 16-byte header size. Invalid trusted configuration raises an
exception; malformed untrusted input returns a parser error.

The current command frame is 76 bytes, so a deployment intended to accept
command messages must configure a limit of at least 76 bytes.

`FrameStreamParser` reserves no more than the trusted configured maximum and
never grows its active frame buffer beyond the declared validated frame size.

## Compatibility guarantees

Protocol versioning and command schema versioning are independent:

- the frame protocol version defines framing, byte order, header fields, and
  message identifiers;
- the command schema version defines the meaning of the command payload after
  framing succeeds.

Within protocol version 1, the following are fixed contracts:

- magic bytes;
- big-endian byte order;
- 16-byte header size;
- `frame_size` semantics;
- message type numeric assignments;
- command payload offsets and widths;
- parser error numeric assignments and names;
- strict parser rejection order.

A change to any of those contracts requires either a new protocol version or an
explicit compatibility design. Details are maintained in
`docs/protocol-compatibility.md` and enforced by
`tickline_protocol_compatibility_tests`.

## Fuzzing and regression corpus

The fuzz entry point exercises:

- header decoding;
- strict frame parsing;
- command-envelope decoding;
- incremental stream parsing at a data-dependent split point;
- EOF classification.

The committed seed corpus is located at:

```text
cpp/fuzz/corpus/protocol_parser
```

The normal test suite runs every named `.bin` seed through a deterministic
regression test. The optional libFuzzer target is built with Clang:

```bash
CC=clang CXX=clang++ cmake -S . -B build-fuzz -DTICKLINE_BUILD_FUZZ_TARGETS=ON
cmake --build build-fuzz --parallel --target tickline_protocol_parser_fuzz
build-fuzz/tickline_protocol_parser_fuzz -max_len=4096 cpp/fuzz/corpus/protocol_parser
```

AddressSanitizer and UndefinedBehaviorSanitizer can be added in a compatible
Clang environment with `-DTICKLINE_ENABLE_SANITIZERS=ON`.

Fuzzer-generated artifacts must be written to a separate working corpus or
artifact directory rather than committed automatically.

## Security properties

The implemented boundary provides:

- bounded reads;
- fixed-width decoding;
- exact payload length enforcement;
- no untrusted recursive structure;
- no silent acceptance of strict trailing bytes;
- arbitrary stream-fragment handling;
- persistent fail-closed state after malformed stream input;
- stable failure classification;
- separation of structural and authoritative validation;
- deterministic regression and fuzz coverage.

It does not provide:

- confidentiality;
- peer authentication;
- message authentication;
- anti-replay cryptography;
- transport integrity;
- rate limiting;
- connection-level denial-of-service controls.

Those controls must not be inferred from successful parsing.

## Test coverage

The protocol tests cover:

- valid header and command frames;
- big-endian encoding and signed extrema;
- every header rejection class;
- all header truncation lengths;
- configured maximum boundaries;
- strict frame truncation and trailing data;
- short and long command payloads;
- zero entity identifiers;
- preservation of authoritative-invalid claims;
- parser-to-pipeline acceptance and rejection;
- parser failure non-mutation;
- every possible split position in one valid frame;
- byte-by-byte stream delivery;
- multiple frames in one chunk;
- complete plus partial frame delivery;
- stream EOF classification;
- fail-closed stream state and reset;
- wire-format compatibility constants;
- committed fuzz corpus regressions.
