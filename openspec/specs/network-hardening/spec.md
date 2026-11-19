# network-hardening Specification

## Purpose
Co-op over real networks: impaired connections in tests, reconnecting, bandwidth budgets, interpolation, interest and desync detection.

## Requirements

### Requirement: Impaired connections in tests
The loopback network SHALL delay messages by a latency and jitter, keep their order and cut connections at a rate, deterministically from a seed.

#### Scenario: Impaired connections in tests
- **WHEN** messages travel an impaired loopback
- **THEN** they arrive late, in order, and connections are cut as set

### Requirement: Reconnecting keeps the heir
A client whose connection is lost SHALL be away, not gone, for 30 s, and rejoining with its resume token SHALL give back its player index; after that it has left.

#### Scenario: Reconnecting keeps the heir
- **WHEN** a client's connection is cut and it rejoins in time and too late
- **THEN** in time it is the same player again; too late it has left

### Requirement: Bandwidth budget
Each connection SHALL keep a budget (64 kbit/s, bursts of 256): over it, "latest" channels keep only the newest unsent message per sender while reliable ones always go; traffic per player is counted.

#### Scenario: Bandwidth budget
- **WHEN** a connection is flooded on both kinds of channel
- **THEN** latest messages are coalesced, reliable ones all arrive, and the counts show it

### Requirement: Smooth over latency
Other heirs SHALL be drawn 0.1 s in the past, interpolated between the snapshots around that time.

#### Scenario: Smooth over latency
- **WHEN** snapshots are buffered and sampled
- **THEN** the sample lies between its neighbours

### Requirement: Rejoining after a lost connection
The game SHALL rejoin a lost session with waits of 1, 2, 4... s until the 30 s grace ends.

#### Scenario: Rejoining after a lost connection
- **WHEN** the connection is lost and rejoined
- **THEN** it tries at growing waits, gives up after the grace, and stops when connected

### Requirement: What is sent, and checked
Entities SHALL be sent to an heir within 3 km, nearest first; snapshots on a latest channel, deltas and inputs reliable; parts of the replicated state SHALL be hashed and the differing ones found.

#### Scenario: What is sent, and checked
- **WHEN** entities near and far are chosen and two states compared
- **THEN** the near ones go nearest first and the differing part is found
