# Deterministic Simulation Model

## Purpose

The simulation core provides a reproducible reference model for later protocol validation, replay analysis, and concurrency work.

Given the same initial state, accepted command sequence, configuration, and number of ticks, the resulting canonical state must be identical.

## Current scope

Implemented:

- fixed-duration ticks
- integer simulation units
- stable entity ordering
- scheduled velocity commands
- sequence and target-tick checks
- bounded position and velocity
- fractional displacement preservation
- atomic world advancement
- canonical state encoding
- deterministic state fingerprints

Not implemented:

- networking
- collision detection
- hit validation
- client prediction
- rollback
- lag compensation
- multithreaded updates

## Units

The deterministic core does not store simulation state in floating point.

| Quantity | Representation |
|---|---|
| Time | signed 64-bit microseconds |
| Position | signed 64-bit millimeters |
| Velocity | signed 64-bit millimeters per second |
| Tick index | unsigned 64-bit integer |
| Entity identifier | non-zero unsigned 64-bit integer |
| Command sequence | unsigned 64-bit integer |

Protocol and presentation layers may use other representations, but conversion must happen at their boundaries.

## Tick contract

A world owns one `FixedStepClock`.

A successful call to `World::advance()`:

1. Computes the next tick without mutating the active world.
2. Applies commands scheduled for that tick.
3. Integrates entities in ascending `EntityId` order.
4. Commits the new clock and entity state.
5. Removes processed commands.

If integration fails, the active world remains unchanged.

Tick duration must be positive. Configuration is rejected when the velocity limit and tick duration could overflow displacement arithmetic.

## Command contract

`VelocityCommand` contains:

- target tick
- entity identifier
- sequence number
- requested velocity

A command is rejected when:

- its target tick is current or past
- the entity does not exist
- its sequence does not increase
- its target tick regresses for that entity
- its velocity exceeds the configured limit

Accepted commands are ordered by target tick, entity identifier, and sequence number. Their insertion order does not affect execution order.

## Integration

For each axis:

```text
scaled displacement =
    velocity × tick duration + previous residual
```

Whole millimeters are applied to position. The remainder is retained for the next tick.

This prevents repeated truncation from discarding sub-millimeter movement. For example, a velocity of one millimeter per second under a 100 ms tick produces one millimeter of movement after ten ticks, not zero.

Position updates use checked addition and are constrained by the configured world limit.

## Deterministic ordering

Entities are stored by `EntityId`, so iteration order is stable.

Pending commands are sorted explicitly. No result depends on hash-table order, allocation address, wall-clock time, thread scheduling, or an implicit random source.

The current implementation is intentionally single-threaded. It is the reference behavior against which later parallel implementations must be compared.

## Canonical state

`encode_world_state()` produces a versioned big-endian byte representation.

The encoding includes:

- schema version
- current tick and elapsed time
- tick duration
- world limits
- entity state
- fractional integration residuals
- highest accepted sequences
- latest scheduled ticks
- pending commands

Internal state is included because two worlds with identical visible positions can still behave differently on later ticks.

The canonical byte layout is covered by an exact fixture test.

## State fingerprint

`fingerprint_world_state()` applies 64-bit FNV-1a to the canonical bytes.

Its purpose is fast deterministic comparison in tests and replay tooling. It is not collision-resistant and must not be used for evidence integrity, signatures, authentication, or security decisions.

The evidence pipeline will use a separate cryptographic design.

## Failure behavior

The world rejects invalid configuration before simulation starts.

Expected input failures return result enums. Arithmetic or invariant failures throw exceptions.

`World::advance()` provides transactional behavior at the object level: the clock and entity state are committed only after all entities integrate successfully.

## Verification

Current tests cover:

- unit conversion boundaries
- invalid tick duration
- entity identifier validity
- tick progression
- invalid world limits
- entity admission
- command rejection
- fractional displacement
- scheduled command execution
- stable snapshot order
- insertion-order independence
- failed-advance state preservation
- canonical byte layout
- pending-command encoding
- stable replay fingerprint
- FNV-1a reference output

The standard, sanitizer, Python, documentation, and Docker checks run through:

```bash
bash scripts/check-local.sh
```

## Known limitations

The state format is internal and versioned, but no decoder exists yet.

The current fingerprint is suitable only for deterministic comparison.

Command scheduling assumes one authoritative world instance and does not address network arrival races. Those belong to later protocol and server-runtime milestones.

The model is two-dimensional. Extending it to three dimensions will require a versioned state-format change and new arithmetic boundary tests.
