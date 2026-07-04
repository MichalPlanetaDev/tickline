# Architecture

Tickline is a deterministic security-engineering range. Its architecture is designed to prove one central principle: client-side claims are not trusted until they cross an explicit protocol boundary and are validated against authoritative state.

The project combines game-security concepts, systems programming, protocol hardening, evidence integrity, runtime diagnostics, and professional engineering workflow. Each component exists to support a reviewable chain from untrusted input to validated state and tamper-evident evidence.

## Architectural goals

Tickline is designed around these goals:

```text
make trust boundaries explicit
keep simulation deterministic
validate client claims server-side
record structured telemetry
protect evidence integrity
support forensic replay
make runtime behavior observable
keep the local demo reproducible
separate implementation from investigation
document failure modes and limitations
```

Tickline is not designed to be a commercial anti-cheat product. It is a controlled engineering range for owned-code testing and portfolio review.

## System overview

```text
+------------------------+
| Controlled clients     |
| Unity viewer / bots    |
+-----------+------------+
            |
            | untrusted commands
            v
+-----------+------------+
| Protocol boundary      |
| framing, parsing,      |
| versioning, limits     |
+-----------+------------+
            |
            | accepted messages
            v
+-----------+------------+
| C++ authoritative core |
| deterministic ticks,   |
| state, physics, rules  |
+-----------+------------+
            |
            | validation results
            v
+-----------+------------+
| Evidence pipeline      |
| telemetry events,      |
| hash chain, manifests  |
+-----------+------------+
            |
            +-----------------------------+
            |                             |
            v                             v
+-----------+------------+    +-----------+------------+
| Investigation storage  |    | Runtime diagnostics    |
| SQLite / query layer   |    | logs, metrics, traces  |
+-----------+------------+    +-----------+------------+
            |                             |
            v                             v
+-----------+------------+    +-----------+------------+
| Python analytics       |    | Docker / CI / Linux    |
| reports, statistics    |    | reproducible workflow  |
+-----------+------------+    +------------------------+
            |
            v
+-----------+------------+
| Unity forensic viewer  |
| timeline replay, state |
| inspection, evidence   |
+------------------------+
```

The system is intentionally modular. The simulation core should not know about the dashboard, GitHub workflow, or release process. The evidence layer should not depend on Unity. The protocol parser should be testable without starting a server. The investigation layer should be able to read completed evidence artifacts without mutating simulation state.

## Primary components

### C++ authoritative core

Location:

```text
cpp/include/tickline/
cpp/src/
cpp/tests/
```

Responsibilities:

```text
fixed-timestep simulation
authoritative entity state
movement validation
timing validation
sequence validation
hit and ray validation
spatial queries
deterministic replay support
validation result generation
```

Expected engineering properties:

```text
C++23
RAII ownership
minimal global state
explicit value types
deterministic update order
unit-testable pure logic
sanitizer-compatible behavior
clear handling of integer and floating-point boundaries
```

The C++ core should be treated as the technical center of the project. Higher-level tools exist to drive it, inspect it, or operationalize it.

### Protocol boundary

The protocol layer is the first hard boundary between untrusted input and authoritative logic.

Responsibilities:

```text
message framing
protocol version checks
maximum frame size enforcement
parser error classification
malformed input rejection
message sequencing expectations
replay-protection fields
fuzzable parser surface
```

The protocol boundary must not silently repair hostile or malformed input. It should reject invalid data early, classify why it was rejected, and emit evidence when appropriate.

Implemented parser properties:

```text
bounded reads
explicit frame length limits
no undefined behavior on malformed input
no unchecked integer conversions
no trust in client-provided timestamps
no trust in client-provided object state
stable error codes for investigation
strict rejection of truncated and trailing bytes
fixed-size big-endian command-envelope decoding
```

The current `AuthoritativeCommandGateway` connects complete parsed commands to
the existing command pipeline. Parser failures do not mutate session or world
state and are kept distinct from authoritative command rejections.

Incremental stream reassembly, compatibility tests, a libFuzzer target, and a committed regression corpus are implemented. Socket ownership, connection policy, and protocol-level malformed-frame evidence remain separate work.

### Simulation and validation layer

The validation layer turns accepted protocol messages into authoritative decisions.

Examples of claims that require validation:

```text
client claims a position
client claims elapsed time
client claims sequence order
client claims a weapon fire event
client claims a hit
client claims a target identifier
client claims a distance or aim vector
```

The server may accept, reject, clamp, or flag claims depending on policy. A validation finding is not a punishment by itself. It is a structured observation that must be reviewable.

### Evidence pipeline

The evidence pipeline records what happened and why the system made a decision.

Responsibilities:

```text
structured telemetry events
validation findings
reason codes
observed values
expected limits
severity levels
hash-chained evidence records
manifest generation
tamper verification
exportable artifacts
```

The evidence layer should answer these questions:

```text
who generated the event
when the server observed it
what claim was made
which rule evaluated it
what values were observed
what limit was expected
what decision was made
whether the evidence chain is intact
```

Evidence integrity is a project requirement, not decoration. The goal is to demonstrate that security telemetry can be made reviewable and tamper-evident.

### Investigation storage

Initial storage target:

```text
SQLite
```

SQLite is appropriate for the local range because it is reproducible, portable, and easy to inspect. The schema should support investigation queries without requiring a full service mesh or production database cluster.

Responsibilities:

```text
ingest evidence artifacts
store telemetry records
store validation findings
query suspicious sessions
query rule breakdowns
query per-entity timelines
support reproducible reports
```

Later versions may include a small read-only API, but the database layer should remain independently testable.

### Python analytics tools

Location:

```text
tools/python/
```

Responsibilities:

```text
parse evidence artifacts
generate statistical summaries
compute baseline distributions
detect outliers in controlled data
produce reports
support test fixture generation
```

Python is used for analysis, not as a replacement for the core simulation. Statistical or machine-learning experiments must be framed as investigation support, not automatic enforcement.

Acceptable analytics scope:

```text
descriptive statistics
outlier scores
rule-trigger frequency
timeline aggregation
false-positive discussion
experiment reproducibility
```

Avoided scope:

```text
black-box ban decisions
unexplained cheat classification
unvalidated production ML claims
```

### Unity forensic viewer

Location:

```text
unity-viewer/
```

Responsibilities:

```text
load replay/evidence artifacts
visualize deterministic timelines
show entity movement
show accepted and rejected claims
show validation findings
display evidence-chain status
support reviewer-friendly inspection
```

The Unity viewer is not the source of truth. It is an investigation and visualization tool over artifacts produced by the authoritative system.

It should prove C# and Unity tooling competence through:

```text
clean scene/tool structure
clear data import boundary
timeline UI
debug visualization
state inspection
replay controls
safe handling of external files
```

### Linux runtime and diagnostics

Tickline should demonstrate practical Linux and runtime awareness.

Expected diagnostics scope:

```text
process inspection
signals and graceful shutdown
file permissions
log locations
resource usage
container runtime behavior
network socket inspection
sanitizer output interpretation
basic failure triage
```

Relevant tooling may include:

```text
ps
top
htop
df
du
free
iostat
ss
lsof
journalctl
strace
grep
awk
sed
find
less
```

Advanced tools such as eBPF, OpenTelemetry, Kubernetes, service mesh, and cloud deployment should only be added if the local system has a real need for them. They must not be included as decorative complexity.

### CI, Docker, and reproducibility

CI is part of the architecture because reproducibility is a security and reliability property.

Expected quality gates:

```text
CMake configure
C++ build
C++ tests
clang-format check
clang-tidy where practical
AddressSanitizer build
UndefinedBehaviorSanitizer build
parser fuzz target build
Python tests
documentation sanity checks
Docker build
Docker smoke test
```

Docker should be used to reproduce the local runtime, not to hide broken setup assumptions.

Expected Docker properties:

```text
minimal images where practical
non-root runtime user
explicit exposed ports
mounted artifact directories
deterministic smoke command
clear health checks where useful
```

## Trust boundaries

### Boundary 1: Client to protocol parser

The client is untrusted.

Untrusted values include:

```text
frame length
message type
protocol version
client timestamp
sequence number
entity identifier
position
velocity
aim direction
target identifier
claimed hit distance
claimed latency
```

Parser requirements:

```text
validate framing before deserialization
enforce size limits
reject unsupported versions
classify malformed input
avoid undefined behavior
avoid unbounded allocation
emit observable failure information
```

### Boundary 2: Protocol parser to simulation core

A parsed message is not automatically trusted.

The parser proves only that bytes formed a valid message. It does not prove that the claim is physically, temporally, or logically valid.

The simulation core must still validate:

```text
state transitions
movement distance
elapsed server time
tick window
sequence monotonicity
hit geometry
target existence
cooldowns
rate limits
```

### Boundary 3: Simulation core to evidence pipeline

Validation findings must be converted into evidence without losing meaning.

The evidence layer must preserve:

```text
rule identity
severity
observed value
expected limit
entity/session context
server timestamp
decision
human-readable explanation
machine-readable code
```

### Boundary 4: Evidence artifact to investigation tooling

Investigation tools consume artifacts. They must not silently trust corrupted artifacts.

The investigation layer should verify:

```text
schema version
required fields
hash-chain continuity
manifest signature when available
record order assumptions
unsupported version behavior
```

### Boundary 5: Local range to external systems

Tickline should not target external games, services, or anti-cheat products.

All hostile behavior must be generated by controlled clients, tests, fuzzers, or fixtures inside this repository.

## Data flow

A typical suspicious movement claim should follow this path:

```text
client sends movement command
protocol layer checks frame and version
parser converts bytes into message
simulation core compares claim against authoritative state
validation rule detects impossible movement
evidence pipeline records finding
hash chain links finding to prior record
artifact is written
SQLite ingests artifact
Python report summarizes finding
Unity viewer visualizes timeline
reviewer inspects decision
```

A malformed protocol input follows a shorter path:

```text
client sends malformed frame
protocol layer rejects frame
parser error is classified
evidence pipeline records protocol finding
connection may be closed depending on policy
```

## Determinism policy

Determinism is required for replay and investigation credibility.

The simulation should prefer:

```text
fixed tick duration
stable update order
explicit random seeds if randomness is used
bounded floating-point tolerance
replayable input streams
testable state transitions
```

The project should document any non-deterministic behavior instead of pretending it does not exist.

## Error and failure handling

Errors should be classified by boundary.

Examples:

```text
protocol_error
validation_error
evidence_integrity_error
storage_error
runtime_error
configuration_error
operator_error
```

Failure handling should avoid:

```text
silent drops without telemetry
catch-all errors with no context
process crashes on malformed input
partial evidence writes that appear valid
ambiguous investigation output
```

## Security model

Tickline assumes the attacker controls the client.

The attacker may:

```text
send malformed frames
send valid frames with invalid claims
replay old messages
manipulate client timestamps
skip sequence numbers
claim impossible movement
claim invalid hits
flood the server
attempt to corrupt local artifacts
```

The attacker must not be modeled as having arbitrary write access to the server process unless a specific milestone adds local tamper testing. Different attacker capabilities must remain separate in documentation.

## Operational model

Tickline is a local reproducible range.

Primary operator workflow:

```text
clone repository
build C++ core
run tests
run controlled clients
generate evidence
verify evidence integrity
ingest investigation data
run analytics
open Unity replay viewer
inspect findings
```

The final project should make this workflow possible from a clean checkout with documented commands.

## Non-goals

Tickline does not aim to provide:

```text
production anti-cheat deployment
kernel monitoring
third-party game analysis
bypass development
cheat detection against real users
commercial-scale telemetry ingestion
unbounded cloud infrastructure
automatic ban decisions
```

These exclusions are part of the design. They protect the project from becoming unsafe, unfocused, or misleading.

## Architecture review checklist

Before adding a new component, answer:

```text
Which trust boundary does it clarify?
Which proof target does it support?
Can it be tested?
Can it be reproduced?
Can it fail safely?
Does it create offensive misuse risk?
Does it make the system clearer or only bigger?
```

If a feature does not pass this checklist, it should not enter the repository.
