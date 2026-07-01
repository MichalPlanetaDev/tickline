# Debugging Workflow

Tickline treats debugging as an engineering discipline, not as guesswork so the project should prove that failures can be reproduced, isolated, inspected, explained, and fixed without hiding behind vague logs or accidental local state. This document defines the expected debugging workflow for local development, CI failures, protocol issues, runtime problems, evidence-integrity failures, and Linux/container diagnostics.

## Principle

The debugging rule is:

```text
reproduce first
isolate the failing boundary
inspect with the right tool
fix the smallest real cause
add a regression check
document the failure mode when it matters
```

Avoid:

```text
random rewrites
blind dependency upgrades
deleting generated state without understanding why
changing multiple layers at once
claiming a fix without reproduction
hiding failures inside Docker
ignoring sanitizer output
```

## Failure boundaries

Every failure should be assigned to a boundary before attempting a fix.

Primary boundaries:

```text
build system
C++ simulation
protocol parser
validation policy
evidence writer
evidence verifier
SQLite ingest
Python analytics
Unity viewer
Docker runtime
Linux host environment
CI workflow
operator command
```

A clear boundary prevents debugging from turning into uncontrolled changes.

## First-response checklist

When something fails, collect:

```text
command that failed
working directory
Git commit
operating system
compiler version
CMake version
Python version
Docker version
complete error output
whether it reproduces from a clean checkout
whether it reproduces outside Docker
whether it reproduces inside Docker
```

Useful commands:

```bash
pwd
git status --short
git rev-parse --short HEAD
uname -a
cmake --version
g++ --version || true
clang++ --version || true
python3 --version
docker --version
docker compose version
```

Do not start by editing code. Start by making the failure observable.

## Local repository checks

Before debugging code, verify repository state:

```bash
git status --short
git diff
git diff --check
git log --oneline -5
```

Common problems:

```text
uncommitted local changes
wrong branch
generated files accidentally staged
old build directory hiding configuration issues
local artifact files affecting tests
```

Clean only when you understand what will be removed.

Useful cleanup commands:

```bash
rm -rf build
rm -rf .pytest_cache
find . -name '__pycache__' -type d -prune -exec rm -rf {} +
```

Avoid broad destructive commands unless the path is explicit and reviewed.

## Build-system debugging

Initial build system target:

```text
CMake + C++23
```

Expected build commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

If configuration fails, inspect:

```text
missing compiler
unsupported compiler version
wrong CMake generator
missing dependency
invalid CMake path
toolchain mismatch
stale cache
```

Useful commands:

```bash
cmake -S . -B build -LAH
cat build/CMakeCache.txt
cmake --build build --verbose
```

If a CMake cache is suspected:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Do not commit fixes that only work because of one local build cache.

## Compiler diagnostics

Compiler warnings should be treated as design feedback.

Typical categories:

```text
narrowing conversion
signed/unsigned mismatch
uninitialized value
unused variable
missing virtual destructor
implicit fallthrough
shadowing
copy instead of move
undefined behavior risk
```

Policy:

```text
fix the cause
do not silence warnings globally
use targeted suppression only with explanation
keep warning flags meaningful
```

When an error is unclear, reduce it:

```text
find first compiler error
ignore cascading errors until first cause is fixed
inspect included headers
check namespace and symbol names
check target link order
```

## Sanitizer debugging

Sanitizers are expected for C++ hardening.

Important sanitizer classes:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
LeakSanitizer where available
ThreadSanitizer later if multithreading is introduced
```

Sanitizer findings are not optional.

If AddressSanitizer reports a failure, identify:

```text
access type
invalid address
allocation stack
free stack
use stack
container involved
input that triggered failure
```

Common ASan failure classes:

```text
heap-use-after-free
stack-use-after-return
heap-buffer-overflow
stack-buffer-overflow
double-free
use-after-scope
memory leak
```

If UBSan reports a failure, identify:

```text
signed integer overflow
invalid shift
null reference
misaligned access
out-of-bounds enum value
invalid downcast
division by zero
```

Fix the code, then add a regression test with the smallest reproducing input.

## Protocol parser debugging

The parser is a security boundary. Debugging must be precise.

When parsing fails unexpectedly, inspect:

```text
frame length
header size
magic bytes
protocol version
message type
payload length
expected payload length
byte order
trailing bytes
error code
```

Useful artifact format for protocol failures:

```text
hex input
expected parser result
actual parser result
parser error code
message type if decoded
frame length if decoded
```

Parser bugs should usually produce tests such as:

```text
valid frame parses
invalid magic rejected
unsupported version rejected
frame too small rejected
frame too large rejected
payload too short rejected
payload too long rejected
unknown message type rejected
truncated buffer rejected
```

Do not debug parser failures by starting the whole server first. The parser should be testable as a pure component.

## Fuzzing triage

Fuzzer findings must be minimized before fixing.

For a fuzz crash, capture:

```text
crashing input
sanitizer output
parser error path
minimal reproducer
commit hash
```

Expected workflow:

```text
save crashing input as fixture
write regression test
fix parser or verifier
confirm fuzzer no longer reproduces crash
keep fixture if it represents a meaningful boundary case
```

A fuzz crash should not be dismissed because the input is unrealistic. Protocol boundaries must handle arbitrary bytes safely.

## Simulation debugging

Simulation bugs should be made deterministic.

Collect:

```text
initial state
tick duration
input sequence
server time
entity state before tick
entity state after tick
validation policy
expected decision
actual decision
```

Avoid debugging simulation logic through UI first. Use tests and replayable input streams.

Simulation failure categories:

```text
wrong update order
non-deterministic state iteration
floating-point tolerance error
integer unit conversion error
incorrect elapsed time
invalid clamp
state transition bug
spatial query bug
hit geometry bug
```

A good simulation test should specify:

```text
given state
given input
when tick advances
expected state
expected validation findings
```

## Floating-point and units

Tickline should prefer integer units for protocol and evidence.

Preferred units:

```text
milliseconds
millimeters
millimeters per second
microunits for normalized vectors
integer counters
```

Debugging unit errors requires checking:

```text
conversion point
rounding rule
overflow risk
sign handling
tolerance value
evidence output unit
test expectation unit
```

Avoid mixing:

```text
meters and millimeters
seconds and milliseconds
floats and fixed-point integers
client time and server time
```

Unit mistakes should be fixed at the boundary, not patched downstream.

## Evidence-integrity debugging

Evidence failures must not be repaired silently.

When verification fails, identify:

```text
artifact path
line number
record_index
expected previous_hash
actual previous_hash
expected record_hash
actual record_hash
canonicalization input
manifest hash
signature status
```

Failure categories:

```text
invalid JSON
unsupported schema
missing required field
record index gap
previous_hash mismatch
record_hash mismatch
manifest hash mismatch
signature invalid
unsigned artifact
truncated artifact
```

Verification tooling should report exact failure locations. A vague “invalid evidence” error is not sufficient.

## SQLite ingest debugging

SQLite is an investigation index, not a place to hide invalid evidence.

Before ingest:

```text
verify artifact schema
verify hash chain
verify manifest if present
reject invalid evidence
```

Debug ingest failures by checking:

```text
database path
directory permissions
schema version
transaction boundary
constraint violation
record count
JSON field serialization
duplicate artifact id
```

Useful commands:

```bash
sqlite3 reports/tickline.db '.schema'
sqlite3 reports/tickline.db 'select count(*) from evidence_records;'
sqlite3 reports/tickline.db 'select rule_id, count(*) from evidence_records group by rule_id;'
```

Database writes should use transactions. Partial ingest should be avoided or explicitly marked.

## Python analytics debugging

Python tools should be deterministic and artifact-driven.

Debug with:

```bash
python3 --version
python3 -m pytest
python3 -m tickline_tools --help
```

Analytics failures often come from:

```text
invalid artifact path
artifact failed verification
schema mismatch
unexpected null
bad numeric conversion
incorrect grouping
unstable sorting
hidden dependence on local files
```

Analytics must not ignore verification failure. A report generated from corrupted evidence should say so clearly or refuse to proceed.

## Unity viewer debugging

The Unity viewer is a read-only forensic tool.

Common failure categories:

```text
artifact file cannot be loaded
schema version unsupported
evidence verification failed
timeline sorting incorrect
coordinate conversion mismatch
large artifact freezes UI
scene references missing
generated Unity files accidentally committed
```

Unity viewer rules:

```text
do not mutate evidence artifacts
show artifact verification status
show unsupported schema clearly
keep importer logic separate from visualization logic
do not commit Library, Temp, Logs, csproj, or sln files
```

Debug Unity file problems before committing:

```bash
git status --short
```

If `unity-viewer/Library` appears in Git output, stop and fix `.gitignore`.

## Linux process debugging

Useful process commands:

```bash
ps aux | grep tickline
pgrep -af tickline
top
htop
kill -TERM <pid>
kill -KILL <pid>
```

Signal policy:

```text
SIGTERM should trigger graceful shutdown where supported
SIGINT should stop local development processes cleanly
SIGKILL is last resort and cannot be handled
```

When a process does not exit:

```text
check open sockets
check blocking I/O
check background threads
check pending flush
check deadlock if multithreading exists
```

Useful commands:

```bash
lsof -p <pid>
strace -p <pid>
```

## Linux file and permission debugging

Common artifact failures are permission failures.

Useful commands:

```bash
ls -la
ls -lah evidence reports samples
stat evidence
id
umask
df -h
du -sh .
```

Check:

```text
directory exists
directory is writable
file owner
file group
permissions
disk space
mount behavior under WSL
Docker volume ownership
```

Do not solve permission problems by blindly using `sudo`. Understand which user owns the files.

## Linux network debugging

Useful socket commands:

```bash
ss -tulpn
ss -tanp
lsof -iTCP -sTCP:LISTEN
```

When a server cannot bind:

```text
port already in use
wrong bind address
insufficient permission for privileged port
container port not published
IPv4/IPv6 mismatch
firewall or WSL networking issue
```

When a client cannot connect:

```text
server not running
wrong host
wrong port
container network mismatch
service bound to 127.0.0.1 inside container
firewall issue
protocol handshake rejected
```

Prefer local reproducible checks before adding infrastructure complexity.

## Log debugging

Logs should answer:

```text
what started
what configuration was loaded
what port or artifact path is used
which session failed
which rule fired
why a connection closed
where evidence was written
why verification failed
```

Avoid logs that expose:

```text
secrets
private keys
credentials
unredacted sensitive external data
```

Useful CLI log tools:

```bash
grep
awk
sed
less
tail -f
```

Examples:

```bash
grep -n "verification_failed" evidence/*.jsonl
awk '/protocol/ { print }' samples/*.log
tail -f logs/tickline.log
```

If logs become structured JSON later, tooling should preserve readability.

## Docker debugging

Docker should reproduce the runtime, not hide failures.

Useful commands:

```bash
docker compose ps
docker compose logs
docker compose logs -f
docker compose build --no-cache
docker compose down --volumes
docker inspect <container>
docker exec -it <container> sh
```

Common Docker failures:

```text
wrong working directory
missing copied file
wrong user permissions
volume mount ownership
port not published
health check command missing dependency
binary built for wrong path
runtime artifact directory missing
```

Container rules:

```text
run non-root where practical
make artifact paths explicit
do not bake secrets into images
keep smoke commands deterministic
```

## CI debugging

CI failure should be reproducible locally whenever practical.

First inspect:

```text
failing job
failing step
exact command
runner OS
tool versions
environment variables
path assumptions
artifact upload/download behavior
```

Common CI-only issues:

```text
case-sensitive path mismatch
missing executable permission
dependency not installed
different compiler version
script assumes interactive shell
line-ending problem
Docker unavailable or configured differently
test depends on local artifact
```

For scripts, ensure:

```bash
chmod +x scripts/*.sh
```

Use explicit shell behavior:

```bash
set -euo pipefail
```

CI should not contain commands that only echo success without testing real behavior.

## Git regression debugging

If something worked before and fails now:

```bash
git log --oneline
git diff <good-tag>..HEAD
git bisect start
git bisect bad
git bisect good <known-good-tag>
```

A good repository should be bisectable because commits are meaningful and tests expose failures.

When reverting public history:

```bash
git revert <commit>
```

Do not rewrite public `main` history to hide a mistake.

## Incident-style notes

For serious failures, write a short note.

Template:

```text
Failure:
Impact:
Detection:
Root cause:
Fix:
Regression coverage:
Prevention:
```

This is not corporate theatre. It is a compact way to prove engineering judgment.

Use incident-style notes for:

```text
evidence verifier accepted tampered artifact
parser crashed on malformed frame
CI silently skipped tests
Docker image ran as root accidentally
release artifact contained wrong file
```

## Root-cause analysis policy

A root cause should identify the system condition that allowed the failure.

Weak root cause:

```text
I made a mistake.
```

Better root cause:

```text
The parser trusted frame_length before checking it against the configured maximum,
which allowed an oversized allocation path. Tests covered valid frames but did not
cover frame_length above the limit.
```

A good fix changes the system so the same class of failure is less likely.

## Debugging anti-patterns

Avoid:

```text
changing many files before reproducing
rewriting architecture to fix a small bug
adding sleeps to fix race conditions without understanding them
ignoring warnings
turning off sanitizer checks
committing generated Unity files
using Docker as a black box
treating corrupted evidence as valid
claiming CI validates something it does not run
```

## Debugging acceptance criteria

A bug fix is complete when:

```text
the failure is reproduced
the root cause is understood
the fix targets the cause
a test or check covers the failure where practical
documentation is updated if behavior or workflow changed
CI passes
the commit message describes the real change
```

## Summary

Tickline debugging should be disciplined:

```text
observe
reproduce
isolate
inspect
fix
verify
document when useful
```
