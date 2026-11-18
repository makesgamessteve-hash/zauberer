# air-and-gases Specification

## Purpose
Defines the air of the active tier: the local wind shaped by the ground and forest, and
the gases it carries (smoke, steam, scent, fumes), how they drift, spread and fade.

## Requirements

### Requirement: Local wind
The active tier SHALL have a wind per 8 m cell: the weather's wind, faster over crests and slower in hollows and under forest canopy, and turned along slopes rather than up them; outside the active tier the weather's wind SHALL apply.

#### Scenario: The ground shapes the wind
- **WHEN** a wind blows across a ridge, and over a forest
- **THEN** it is faster on the crest than on the plain and slower on the windward slope than on the crest, the forest shelters it to under 60 % of its speed, and a chunk outside the tier reports the weather's wind

### Requirement: Gases in the air
Smoke, steam, scent and fumes SHALL be concentrations in the lowest air layer that the local wind carries, that spread a little, and that are lost at each gas's own rate; chunks without gas SHALL sleep.

#### Scenario: Gases drift downwind and fade
- **WHEN** smoke is released for ten seconds in a steady wind, then steam and scent, and the air is left
- **THEN** the smoke is far thicker downwind than upwind, the emitted mass is recorded and part of it is in the air, steam is gone within a minute while scent remains, and finally the air clears and the chunk sleeps

### Requirement: Deterministic air
The same ground, wind and sources SHALL give the same air; places outside the active tier SHALL report that they are not loaded and take no gas.

#### Scenario: Deterministic air
- **WHEN** the same fumes are released over the same ridge twice, and gas is released outside the tier
- **THEN** the hashes are equal and nothing outside the tier is loaded

### Requirement: Air in the game
The engine SHALL run the air at 4 Hz around the focus points with the weather's wind, put smoke from burning cells into it, lean fire with the local wind, and draw smoke; scripts SHALL read the wind and gases, release gases and read statistics.

#### Scenario: Air from Lua
- **WHEN** a scenario sets the wind, releases scent and lights a fire near the carpet
- **THEN** the wind near the carpet blows from the weather's direction, the scent drifts downwind, the fire's smoke is in the air, and unknown gases and invalid amounts are refused

### Requirement: Air budget
A chunk full of moving gas SHALL step in at most 1 ms on desktop in a release build.

#### Scenario: Air budget
- **WHEN** smoke fills a chunk in a wind
- **THEN** the median step takes at most 1 ms
