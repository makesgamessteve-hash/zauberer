# shallow-water Specification

## Purpose
Defines surface water in the active tier: how it flows between 2 m cells, pools and
goes still, soaks into the ground, drains into the terrain's water bodies, falls as
rain, and keeps an exact volume balance.

## Requirements

### Requirement: Water flows
Surface water in the active tier SHALL flow between neighbouring 2 m cells by the difference of their water surfaces, never more than a cell holds, so that it runs downhill, spreads, and fills hollows to a flat surface; each cell SHALL report the velocity of its water from the flows through its faces.

#### Scenario: Water runs downhill
- **WHEN** water is poured high on a rock slope
- **THEN** the water running down the slope reports a velocity downhill, the place it was poured drains and the water gathers further down, with the chunk awake while it moves

#### Scenario: A basin fills flat and goes still
- **WHEN** water is poured into a rock basin and left
- **THEN** its surface levels out within 2 cm, all of it stays in the basin, and the chunk goes to sleep keeping the water

### Requirement: Volume balance
Every cubic metre SHALL be accounted for: the water on the ground, soaked into it and drained into the terrain's water bodies SHALL equal the water rained and added, up to rounding, counting the water standing in chunks when they left the active tier and the water magic took away.

#### Scenario: Water drains into the sea
- **WHEN** water is poured on a shore that slopes into the sea, and also onto the sea
- **THEN** the sea takes none directly, most of the shore's water drains into it, and the balance holds

### Requirement: Rain and the ground
Rain SHALL fall on every cell of the active tier; the ground SHALL soak it up at the rate and up to the capacity of its surface material, and the rest SHALL stay on the ground and run off.

#### Scenario: Rain soaks in or runs off
- **WHEN** steady rain falls on flat soil, and a cloudburst on flat rock
- **THEN** the soil soaks up nearly all of it with no standing water, the rock soaks up none and holds standing water, and the balance holds in both

### Requirement: Deterministic water
The same ground, water and rain SHALL give the same water, cell for cell; places outside the active tier SHALL report that they are not loaded and take no water.

#### Scenario: Deterministic water
- **WHEN** the same water is poured on the same slope twice, and water is poured and read outside the active tier
- **THEN** the simulation hashes are equal, wet cells are reported, and outside the tier nothing is loaded or poured

### Requirement: Water in the game
The engine SHALL run surface water at 20 Hz around the world's focus points and draw standing water; scripts SHALL pour water, read cells and the balance, and set the rain.

#### Scenario: Water from Lua
- **WHEN** a scenario pours water near the carpet and lets it rain
- **THEN** cells near the carpet hold water that spreads, the balance holds, standing water is drawn, and invalid volumes and radii are refused

### Requirement: Water budget
A chunk of moving water SHALL step in at most 2 ms on desktop in a release build.

#### Scenario: Water budget
- **WHEN** a flood spreads over a chunk
- **THEN** the median step takes at most 2 ms

### Requirement: Removing and freezing water
Magic SHALL take standing water away (counted in the balance) and freeze water, standing or a water body, for a time: frozen water SHALL neither flow nor take water in, SHALL be reported as frozen so that it can be walked on, and SHALL melt when its time is up or when thawed.

#### Scenario: Water for magic
- **WHEN** standing water is partly and wholly removed, a pool and the sea are frozen, time passes, the ice is thawed, and frozen water is drawn
- **THEN** removals take what was asked or what there was and are counted in the balance, frozen water keeps its depth and does not flow until it melts on time or is thawed, dry ground does not freeze, the sea freezes at its level, and ice is drawn pale and nearly opaque
