# chunk-streaming Specification

## Purpose
Keeps terrain chunks generated at the right stage around the players' focus points:
in rings, ordered by predicted position and view, within job, time and memory budgets,
with hysteresis, cancellation and eviction, on every platform.

## Requirements

### Requirement: Rings
The stream SHALL bring every chunk within the near radius of a focus to stage 2 and every chunk within the far radius to at least stage 0, where the far radius grows with the focus's altitude up to a maximum.

#### Scenario: Settled rings
- **GIVEN** a still focus at 100 m altitude with the desktop profile
- **WHEN** the stream has finished all work
- **THEN** every chunk within 1,536 m is at stage 2 and every chunk within 12,400 m is at least at stage 0

#### Scenario: Higher means farther
- **WHEN** the focus climbs from 100 m to 2,000 m altitude
- **THEN** the far ring grows to 20 km and chunks between the old and new radius are added at stage 0

#### Scenario: Around a cube corner
- **WHEN** the focus is at a cube corner
- **THEN** the rings contain chunks of three faces and every chunk once

### Requirement: Priority
Chunks SHALL be generated in order of their distance to the focus position predicted two seconds ahead from its velocity, with chunks behind the view direction delayed.

#### Scenario: Ahead first
- **WHEN** a focus moving east at 150 m/s starts streaming
- **THEN** most of the first 20 chunks generated lie east of the focus

#### Scenario: In view first
- **WHEN** a still focus looking north starts streaming
- **THEN** most of the first 20 chunks generated lie north of the focus

### Requirement: Budgets
The stream SHALL keep at most the profile's number of generation jobs in flight, and when jobs run inline it SHALL stop starting jobs in an update once its time budget is used up.

#### Scenario: Job limit
- **WHEN** a focus starts streaming with worker threads
- **THEN** at no update are more jobs in flight than the profile allows

#### Scenario: Inline time budget
- **GIVEN** inline jobs and a clock that advances 2 ms per generated chunk with a 4 ms budget
- **WHEN** an update runs
- **THEN** it generates at most three chunks

#### Scenario: Ring refresh cost
- **WHEN** a focus with the desktop profile moves far enough for its rings to be recomputed, in the release build on the Linux CI job
- **THEN** the update without generation takes at most 2 ms

### Requirement: Hysteresis
A chunk SHALL keep its stage until it is more than 25 % beyond the ring that asked for it.

#### Scenario: Flying along a ring boundary
- **WHEN** a focus moves back and forth by 40 m for 200 updates after the rings have settled
- **THEN** no chunk is generated again and none is evicted

### Requirement: Cancellation
Queued generation jobs of chunks that left every ring SHALL be cancelled.

#### Scenario: Teleport
- **WHEN** the focus jumps 500 km while jobs are queued
- **THEN** the queued jobs of the old rings are cancelled and the new rings are generated

### Requirement: Memory cap
The stream SHALL keep its chunk data within the memory cap by evicting the least recently wanted chunks outside the rings, and SHALL never evict chunks inside a ring.

#### Scenario: Long flight with a small cap
- **WHEN** a focus flies 10 km with a cap of 5 MB
- **THEN** chunk memory stays within the cap after every update, chunks near the start are evicted and chunks inside the rings stay loaded

### Requirement: Endless flight
A focus flying at 150 m/s SHALL never reach ground that is not generated to stage 2 once streaming has warmed up.

#### Scenario: Thirty seconds at full speed
- **WHEN** a focus flies at 150 m/s for 30 seconds of updates at 60 Hz, with one generation job per update
- **THEN** after the first two seconds the chunk under the focus is always at stage 2

### Requirement: Platform profiles
The stream SHALL provide desktop, web and mobile web profiles with the radii, caps, job counts and time budgets of the design.

#### Scenario: Profile values
- **WHEN** the three profiles are requested
- **THEN** their near radii are 1,536 m, 1,024 m and 640 m and their memory caps 512 MB, 256 MB and 128 MB

### Requirement: Co-op behaviour
The stream SHALL support up to four focus points (one per local player, or every player on the host), keeping a chunk while any focus wants it; chunk content SHALL not depend on streaming order.

#### Scenario: Two focus points
- **WHEN** two focus points 30 km apart stream and then the second is removed
- **THEN** both rings are generated, and after the removal only the first focus's rings remain wanted

#### Scenario: Same content as direct generation
- **WHEN** a streamed chunk is compared with the same chunk generated directly
- **THEN** their data hashes are equal

### Requirement: Scripting access
Scripts SHALL be able to set and clear focus points, read the ground height from loaded chunks and read the streaming statistics.

#### Scenario: Streaming from Lua
- **WHEN** a script sets a focus and the engine runs a few hundred frames
- **THEN** the statistics report stage-2 chunks and the ground height under the focus matches the terrain height within 0.5 m
