# Protocol Specification

Tickline uses the protocol boundary as the first hard line between untrusted client input and authoritative simulation state.

The protocol is intentionally specified before implementation. The goal is to prevent the parser from becoming an improvised collection of assumptions. Every byte accepted by the server must have a documented meaning, a size limit, and a failure mode.

## Design goals

The protocol is designed for:

```text
explicit framing
bounded parsing
versioned messages
deterministic replay
server-authoritative validation
malformed-input classification
fuzzable parser surfaces
clear evidence generation
```

The protocol is not designed for:

```text
production matchmaking
commercial anti-cheat deployment
encrypted transport in the first milestone
third-party game traffic
obfuscated messages
bypass resistance against real anti-cheat systems
```

Tickline may later document TLS or authenticated channels, but the initial protocol focuses on local owned-code validation and parser hardening.

## Transport model

Initial transport target:

```text
TCP
```

TCP is selected first because it simplifies the early engineering range:

```text
ordered byte stream
simple local testing
easy framing experiments
straightforward Docker smoke tests
simple bot/client implementation
good fit for parser hardening milestones
```

TCP does not remove the need for protocol framing. TCP is a byte stream, not a message stream. Tickline must still define where one message begins and ends.

Possible later transport research:

```text
UDP command protocol
loss/reorder simulation
snapshot interpolation
client prediction
lag-compensation windows
QUIC/HTTP3 investigation notes
```

Those are later research topics, not v0.1 requirements.

## Byte order

All multi-byte integer fields are encoded in network byte order:

```text
big-endian
```

This choice makes byte-level inspection predictable and avoids depending on host CPU endianness.

Floating-point values should not appear in the initial wire protocol unless a milestone explicitly accepts the determinism and validation costs. The preferred representation is fixed-point integer encoding for simulation-relevant values.

Example:

```text
position_x_mm: int32
position_y_mm: int32
velocity_x_mm_per_s: int32
velocity_y_mm_per_s: int32
```

Using millimeters as integer units avoids treating client-provided floating-point values as authoritative truth.

## Frame format

Initial frame layout:

```text
offset  size  field
0       4     magic
4       2     protocol_version
6       2     header_size
8       4     frame_length
12      4     message_type
16      8     session_id
24      8     sequence
32      N     payload
```

Header size:

```text
32 bytes
```

Fields:

| Field | Type | Meaning |
|---|---:|---|
| `magic` | 4 bytes | ASCII `TICK` |
| `protocol_version` | `uint16` | Wire protocol version |
| `header_size` | `uint16` | Header length in bytes |
| `frame_length` | `uint32` | Total frame length, including header |
| `message_type` | `uint32` | Message discriminator |
| `session_id` | `uint64` | Client session identifier |
| `sequence` | `uint64` | Client command sequence |
| `payload` | bytes | Message payload |

Frame constraints:

```text
magic must equal TICK
protocol_version must be supported
header_size must equal 32 for v1
frame_length must be >= header_size
frame_length must be <= configured maximum
payload length is frame_length - header_size
message_type must be known or rejected as unsupported
sequence must be validated by the simulation/session layer
```

The parser validates structure. It does not validate gameplay truth.

## Maximum frame size

Initial configured limit:

```text
4096 bytes
```

The limit exists to prevent accidental or hostile unbounded reads and allocations.

The parser must reject frames where:

```text
frame_length < header_size
frame_length > max_frame_size
frame_length is inconsistent with available bytes
payload is too short for the declared message type
payload contains extra bytes not allowed by that message type
```

The exact maximum may become configurable, but every configuration must preserve a hard upper bound.

## Message types

Initial message type allocation:

| ID | Name | Direction | Purpose |
|---:|---|---|---|
| `1` | `ClientHello` | client to server | Start session and announce client capabilities |
| `2` | `ServerHello` | server to client | Accept protocol version and return server policy summary |
| `10` | `InputCommand` | client to server | Submit movement/action claim |
| `11` | `HitClaim` | client to server | Submit claimed hit against a target |
| `20` | `ServerCorrection` | server to client | Return authoritative state correction |
| `21` | `ValidationFinding` | server to client | Optional client-visible validation finding |
| `30` | `Heartbeat` | both | Keepalive and latency observation |
| `40` | `DisconnectNotice` | server to client | Explain intentional protocol/session close |

Message IDs should remain stable after release. Deprecated messages should be documented rather than silently reused.

## Session identifiers

`session_id` is a protocol-level identifier for a local controlled session.

Requirements:

```text
server must not trust client-chosen session identity for authorization
server may assign or confirm a session during ClientHello / ServerHello
session_id must be included in evidence context
session_id must be stable for deterministic replay of a session
```

A later authentication milestone may replace or extend this model. v0.1 does not claim authenticated identity.

## Sequence numbers

`sequence` is client-controlled input and must be validated.

Expected rule:

```text
accepted command sequences for a session must increase monotonically
```

Invalid sequence behavior:

```text
duplicate sequence -> replay-relevant finding
lower sequence -> replay-relevant finding
large unexpected jump -> suspicious or policy-dependent finding
sequence wraparound -> unsupported in v1
```

The parser reads the sequence. The simulation/session layer decides whether it is acceptable.

## Timestamp policy

Client timestamps may appear inside selected payloads, but they are not authoritative.

Rules:

```text
server time is authoritative for validation
client time may be used only as a claim
client time must be bounded against server-observed windows
client clock drift must be considered in false-positive analysis
client timestamps must be recorded when relevant to evidence
```

Initial protocol messages should prefer server-observed timing whenever possible.

## Payload encoding

v1 payloads use fixed-width binary fields.

Reasons:

```text
predictable parser behavior
small surface area
simple fuzzing
clear byte-level documentation
no dependency on JSON parser behavior
no hidden allocation behavior from nested dynamic structures
```

Dynamic strings are avoided in early protocol messages. If strings are introduced later, they must have explicit length limits and UTF-8 validation policy.

## ClientHello payload

Purpose:

```text
start a controlled local session
announce client build metadata
negotiate protocol compatibility
```

Payload layout:

```text
offset  size  field
0       4     client_build_id
4       4     client_capabilities
8       8     client_nonce
```

Fields:

| Field | Type | Meaning |
|---|---:|---|
| `client_build_id` | `uint32` | Test client build or fixture identifier |
| `client_capabilities` | `uint32` | Bitset of optional local capabilities |
| `client_nonce` | `uint64` | Client-generated nonce for replay experiments |

Validation notes:

```text
client_build_id is informational
client_capabilities must ignore unknown bits unless policy rejects them
client_nonce is not a security guarantee by itself
```

## ServerHello payload

Purpose:

```text
confirm supported protocol
return authoritative policy summary
establish server nonce for replay experiments
```

Payload layout:

```text
offset  size  field
0       8     server_nonce
8       4     tick_rate_hz
12      4     max_frame_size
16      4     movement_policy_id
20      4     evidence_schema_version
```

The server policy summary is not an authorization contract. It helps clients and tests understand the active local range configuration.

## InputCommand payload

Purpose:

```text
submit a movement or action claim
```

Payload layout:

```text
offset  size  field
0       8     client_time_ms
8       4     input_flags
12      4     position_x_mm
16      4     position_y_mm
20      4     velocity_x_mm_per_s
24      4     velocity_y_mm_per_s
28      4     aim_x_microunits
32      4     aim_y_microunits
```

Fields:

| Field | Type | Meaning |
|---|---:|---|
| `client_time_ms` | `uint64` | Client-observed time claim |
| `input_flags` | `uint32` | Movement/action bitset |
| `position_x_mm` | `int32` | Claimed X position |
| `position_y_mm` | `int32` | Claimed Y position |
| `velocity_x_mm_per_s` | `int32` | Claimed X velocity |
| `velocity_y_mm_per_s` | `int32` | Claimed Y velocity |
| `aim_x_microunits` | `int32` | Claimed aim vector X component |
| `aim_y_microunits` | `int32` | Claimed aim vector Y component |

Validation examples:

```text
movement envelope
velocity envelope
client time monotonicity
sequence monotonicity
aim vector normalization tolerance
state transition validity
cooldown policy
```

The client may claim position and velocity, but the server decides whether the claim is acceptable.

## HitClaim payload

Purpose:

```text
submit a claimed hit against an authoritative target
```

Payload layout:

```text
offset  size  field
0       8     client_time_ms
8       8     target_entity_id
16      4     origin_x_mm
20      4     origin_y_mm
24      4     direction_x_microunits
28      4     direction_y_microunits
32      4     claimed_distance_mm
```

Fields:

| Field | Type | Meaning |
|---|---:|---|
| `client_time_ms` | `uint64` | Client time claim |
| `target_entity_id` | `uint64` | Claimed target |
| `origin_x_mm` | `int32` | Claimed shot origin X |
| `origin_y_mm` | `int32` | Claimed shot origin Y |
| `direction_x_microunits` | `int32` | Claimed ray direction X |
| `direction_y_microunits` | `int32` | Claimed ray direction Y |
| `claimed_distance_mm` | `uint32` | Claimed hit distance |

Validation examples:

```text
target exists
target is hittable in current authoritative state
direction is non-zero
direction is normalized within tolerance
target lies within ray tolerance
claimed distance matches authoritative geometry
distance is within maximum range
cooldown allows shot
sequence is acceptable
```

Hit validation must be geometric and stateful. A syntactically valid hit claim is not a valid hit.

## Heartbeat payload

Purpose:

```text
support liveness and timing observation
```

Payload layout:

```text
offset  size  field
0       8     local_time_ms
8       8     nonce_echo
```

Heartbeat messages may support latency experiments, but they must not become trusted time sources.

## DisconnectNotice payload

Purpose:

```text
explain server-initiated connection closure
```

Payload layout:

```text
offset  size  field
0       4     reason_code
4       4     detail_code
```

Examples:

```text
unsupported_protocol_version
malformed_frame
frame_too_large
rate_limit_exceeded
session_policy_violation
server_shutdown
```

Disconnect messages should avoid exposing sensitive implementation detail. Tickline is local and controlled, but the habit matters.

## Parser error taxonomy

Parser errors should have stable machine-readable codes.

Initial taxonomy:

```text
invalid_magic
unsupported_protocol_version
invalid_header_size
frame_too_small
frame_too_large
truncated_frame
unknown_message_type
payload_too_short
payload_too_long
invalid_enum_value
reserved_bits_set
integer_overflow_risk
```

Parser errors should be testable without running the full server.

## Validation finding taxonomy

Validation findings are produced after parsing.

Initial taxonomy:

```text
sequence_replay
sequence_regression
client_time_regression
client_time_jump
movement_envelope_violation
velocity_envelope_violation
invalid_state_transition
fire_cooldown_violation
unknown_target
invalid_aim_vector
hit_distance_violation
hit_geometry_violation
rate_limit_violation
```

A parser error and a validation finding are different classes of event. The system must not blur them.

## Fuzzing strategy

The protocol parser must be fuzzable as an owned-code target.

Fuzzing goals:

```text
no crashes on arbitrary bytes
no undefined behavior
no unbounded allocation
no hangs on malformed input
stable error classification where possible
valid frames still parse correctly
```

Expected fuzz target scope:

```text
parse one frame from a byte buffer
return parsed message or parser error
never call network code
never write evidence files
never require global mutable state
```

The fuzz target should be small and deterministic.

## Rate limits

Rate limiting is a session/runtime concern rather than a frame-format concern.

The protocol should support evidence for:

```text
messages per second
bytes per second
malformed frames per window
connection churn
heartbeat timeout
```

Rate limits must be configurable and recorded in evidence when they produce findings.

## Versioning policy

Protocol version is explicit in every frame.

Rules:

```text
v1 parser accepts only v1 unless compatibility is documented
unsupported versions are rejected
message IDs are not silently reused
field meaning must not change without versioning
reserved bits must be checked or explicitly ignored by policy
```

Versioning exists to make future changes reviewable.

## Replay policy

Replay resistance in v1 is local and experimental.

Initial controls:

```text
session id
sequence monotonicity
client nonce
server nonce
server-observed time
evidence of repeated sequence or stale command
```

v1 does not claim cryptographic replay protection. Cryptographic authentication may be a later milestone if it can be implemented safely and documented honestly.

## Evidence generated from protocol events

Protocol-layer findings should preserve:

```text
session id if known
remote endpoint if available
parser error code
frame length when known
protocol version when known
message type when known
sequence when known
server timestamp
decision
connection action
```

Gameplay validation findings should preserve:

```text
session id
entity id
message type
sequence
client time if present
server time
rule id
observed value
expected limit
severity
decision
explanation
```

The evidence schema is defined separately in `docs/evidence-integrity.md`.

## Security considerations

The parser must assume hostile bytes.

Implementation must avoid:

```text
reinterpret_cast over untrusted buffers
unaligned unsafe reads
unchecked frame lengths
signed integer overflow
allocations directly sized by untrusted fields without limits
recursive parsing
ambiguous partial-frame handling
silent acceptance of trailing bytes
```

Prefer:

```text
span-like bounded views
explicit read_u16/read_u32/read_u64 helpers
checked arithmetic
clear ParseResult type
stable error enum
unit tests for each failure mode
fuzz target for arbitrary input
```

## Test requirements

Protocol implementation should include tests for:

```text
valid ClientHello
valid InputCommand
valid HitClaim
invalid magic
unsupported protocol version
invalid header size
frame too small
frame too large
truncated frame
unknown message type
payload too short
payload too long
sequence values preserved
big-endian decoding
boundary frame length exactly at limit
malformed random bytes do not crash parser
```

Tests should check exact error codes. Vague failure assertions are not enough for a parser boundary.

## Documentation requirements for future changes

Any protocol change must update:

```text
message table
frame layout if changed
payload layout
parser error taxonomy if changed
validation finding taxonomy if changed
test coverage
evidence mapping
CHANGELOG entry for released versions
```

Protocol changes are compatibility-sensitive and should not be mixed with unrelated UI or documentation work.

## Summary

Tickline's protocol is a controlled boundary, not just a serialization format.

```text
bytes enter
framing is checked
version is checked
payload is parsed within limits
claims are passed to authoritative validation
findings become structured evidence
```

The protocol is successful when malformed input is boring: rejected, classified, tested, and reviewable.
