# Design: Air and Gases

## Context

`sim/active` keeps the active chunks as 128 × 128 cells with neighbours across edges and
seams. Fire takes one wind for the whole planet from `sim.set_weather`. The chunk data
has 1 m heights and a canopy cover grid every 32 m.

## Goals / Non-Goals

**Goals:**

- A wind that reads the land.
- Gases that drift and fade believably.
- Cheap enough to run around every player all the time.

**Non-Goals:**

- Vertical layers and thermals.
- Oxygen and enclosed spaces (interiors).
- Species scents and their use by creatures (M5).
- Bodily effects (M6).
- A global wind model (`add-weather`).

## Decisions

### 8 m cells

The active grid gains a resolution. Air at 32 cells per chunk is 1,024 cells, against
16,384 for fire and water. A smoke column is visible as tens of metres anyway. Neighbour
linking is the same code at any resolution.

### Wind from the ground, computed once per wind change

Each cell keeps a speed factor and the ground's slope, computed when its chunk is taken
in:

- factor: 1 + (height above the 56 m neighbourhood mean) / 40 m, clamped to 0.5–1.6,
  then times 1 − half the canopy cover;
- slope: the gradient of the heights.

The local wind is the weather's wind (in the chunk's axes) with up to 80 % of its uphill
component turned away (more on steeper slopes), times the factor. It is recomputed only
when the weather's wind changes. Each chunk also keeps its mean wind, which fire reads
for its lean.

### Semi-Lagrangian gases

Each step traces every cell back along its wind for 0.25 s and samples the old field
bilinearly. Neighbours beyond the chunk's edge come from the active grid. This is stable
for any wind (a 30 m/s gale moves less than a cell per step) and costs four reads per
cell. It does not conserve mass exactly, which gases do not need. Emissions are
counted, and the statistics report what is in the air.

Spreading is an explicit 4-neighbour term (2 m²/s). The losses per second are:

- smoke 1/120: it rises out of the lowest layer;
- steam 1/20: it condenses;
- scent 1/300;
- fumes 1/200.

Values under 10⁻⁹ kg/m³ are cut to zero, which lets clean chunks sleep.

### Fire couples through the engine

Each fire survey (once a second) asks the air for each chunk's mean wind for its lean
weights. Each tick, the engine releases smoke for every burning cell: its burn rate
times the fuel's smoke share from the registry.

### Drawing smoke

The engine draws up to 256 smoke puffs, generated sprite art, over the cells with the
most smoke within 400 m of the camera. They are scaled and lifted by concentration.

## Risks / Trade-offs

- Without vertical layers, smoke cannot rise over a ridge and descend beyond it; it
  hugs the ground until it thins. That is acceptable until thermals come.
- A gas leaving the active tier is lost.
