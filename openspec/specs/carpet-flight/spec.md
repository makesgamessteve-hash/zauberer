# carpet-flight Specification

## Purpose
Defines how the carpet flies over the planet: handling, collision with the ground and
the sea, the camera and HUD that follow it, and the determinism that replays and co-op
rely on.

## Requirements

### Requirement: Handling
The carpet SHALL accelerate with throttle to 60 m/s, boost to 150 m/s and ease back after a boost, brake to a stop, turn at up to 70° per second (less at speed) while banking into the turn, and climb or dive with the climb input.

#### Scenario: Speeds
- **WHEN** full throttle is held for ten seconds, then boost for five, then nothing for ten, then the brake
- **THEN** the speed reaches 60 m/s, then 150 m/s, eases back towards 60 m/s, and drops to zero

#### Scenario: Turning
- **WHEN** full right steering is held for one second at rest, and again at top speed
- **THEN** the heading turns 70° at rest, less than 40° per second at top speed, and the carpet banks into the turn

### Requirement: Ground and sea
The carpet SHALL stay at least 2.5 m above the terrain or the sea surface, sliding up slopes instead of passing through them, and SHALL not rise above 6,000 m.

#### Scenario: Climb and ground
- **WHEN** the carpet climbs, then dives into the ground, then flies into a cliff, over the sea, and up to the ceiling
- **THEN** it gains height while climbing, rests 2.5 m above the ground or the sea, ends above the cliff, and stops at 6,000 m

### Requirement: Endless flight
The carpet SHALL fly in any direction for at least ten minutes at full boost without falling below the ground, and the streamed terrain SHALL catch up with it.

#### Scenario: Ten minutes in any direction
- **WHEN** the carpet boosts north, east, south and west for 150 seconds each
- **THEN** it flies more than 80 km, never comes closer than 2.5 m to the ground, and the chunk under it is then generated to stage 2

### Requirement: Flying from input
The flight SHALL be driven by the input actions steer (turn and throttle), climb (and the touch climb stick), boost and look, and SHALL keep the chime, credits, fullscreen and quit actions.

#### Scenario: Flying from input
- **WHEN** throttle, steering, climbing, boost, the touch climb stick, chime, credits and look are injected
- **THEN** the carpet accelerates, turns right, climbs, boosts above 140 m/s, descends, the credits open, and the carpet is drawn near the render origin

### Requirement: Deterministic flight
Script math (the transcendental functions and the power operator) SHALL be bit-identical on every platform, so that the same inputs give the same flight everywhere.

#### Scenario: Deterministic math
- **WHEN** a script evaluates the sine of 0.5 and the arctangent of 1/2
- **THEN** it gets exactly 0.47942553860420301 and 0.46364760900080609

#### Scenario: Same flight on every platform
- **WHEN** the recorded flight replay runs on Linux, Windows and WebAssembly
- **THEN** every recorded state hash matches and the final hashes are identical

### Requirement: Co-op behaviour
Each heir SHALL fly their own carpet; the carpet's state SHALL be a plain snapshot (position, heading, speed, pitch, vertical speed, distance) that the host simulates authoritatively and a client can predict with the same code.

#### Scenario: Snapshot
- **WHEN** a carpet's snapshot is taken
- **THEN** it contains its chunk, offset, altitude, heading, speed, pitch, vertical speed and distance, and nothing else
