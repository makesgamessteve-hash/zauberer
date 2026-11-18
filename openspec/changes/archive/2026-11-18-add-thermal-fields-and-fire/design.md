# Design: Heat and Fire

## Context

Chunks carry 1 m heights and surface materials (stages 1 and 2) and plants (stage 3).
The material registry gives ignition temperature, heat of combustion, burn rate and heat
capacity. The world keeps edits per chunk and applies them when a chunk arrives. There is
no weather simulation yet; wind and rain are set from outside.

## Goals / Non-Goals

**Goals:**

- Fires that read right from the carpet: fronts that lean with wind and slope, crowns
  that throw embers, rivers and rain that stop them, and scars that heal.
- Heat as a field any source can feed (spells in M4), not a fire-only flag.
- Zero cost where nothing is hot; deterministic.

**Non-Goals:**

- Air layers, smoke, oxygen and closed rooms (`add-air-and-gases`).
- Snow and ice melting, and lava (`add-shallow-water`).
- The near tier's fire fronts and dormant catch-up (`add-simulation-tiers`).
- Burning buildings and creatures.
- Sunlight heating the ground.

## Decisions

### Temperature above ambient, per chunk, 2 m cells

A cell stores its temperature above the ambient air, not the absolute temperature. The
ambient comes from the chunk's climate, season, time of day and height. It is updated
once a second. A cell with no extra heat is exactly 0, so a quiet chunk can sleep with
all zeros and cost nothing. Heat entering a sleeping chunk (from a neighbour, a spell or
an ember) wakes it.

Cells are 2 m. Each chunk has 128 × 128 of them, so a chunk at full fire is 16k cells,
and the active tier holds up to 64 chunks. Neighbours across chunk edges are found once
per change of the chunk set: the planet maps each ghost cell's centre to the chunk and
cell it falls in, so cube-face seams need no special case.

### Fine fuel decides ignition, all fuel decides burning

A cell's heat capacity counts a thin layer of ground, the first 1 kg/m² of fuel and that
fuel's water. A forest cell therefore catches about as easily as a meadow, from its
litter and twigs, but burns far longer and harder, because its whole crown load
burns at the wood rate. Water boils off at 100 °C before the fuel heats further, which
is what makes wet fuel slow and very wet fuel immune. Fuel wetter than 0.3 kg/kg does
not burn at all.

### Spread as weighted radiation

A burning cell releases `rate × heat of combustion` W/m², reduced by its wetness. 30 %
of that goes to its eight neighbours, with these weights:

- wind: `exp(0.35 s/m × wind along the direction)`, which makes 6 m/s about 8 times
  stronger downwind than across;
- slope: `exp(4 × rise per metre)`, clamped;
- distance: `1/d²`.

Neighbours heat, dry and ignite in turn. This cellular heat transfer is tuned against
real orders of magnitude rather than derived. A grass fire spreads at about 0.1–0.2 m/s
in calm air and 0.5 m/s in a 5 m/s wind.

### Embers

A burning wood cell in wind of 2 m/s or more throws embers at a rate proportional to its
intensity. An ember lands 2–8 s of wind downwind, with a sideways scatter. It ignites
dry fuel there half of the time. Counter-based hashes of the cell and step make this
deterministic. A wide river stops a fire, and a narrow one may not in a gale, as in
reality.

### Physical time, game-time scars

The fire steps at 20 Hz in real seconds, as flight does. The calendar runs 36 times
faster (a 40-minute day), so a fire stepped in game time would race across the land.
Scars are dated in game minutes, because regrowth is measured in game days.

### Scars in the deltas

A cell that burnt out with more than half its fuel gone becomes a `ZB_EDIT_BURNT` edit:
the 2 m cell and the game minute. The world applies scars at its current time:

- ash on the ground for 20 days;
- every plant on the cell removed until its layer's regrowth time has passed (ground
  cover 15 days, shrubs 48, trees 240).

Scars flush to the world once a second, a batch per chunk, so the terrain mesh is
rebuilt at most once a second per chunk.

Edits are now indexed per chunk by target, because a burning chunk adds thousands of
them and the old linear search for a replaced edit was quadratic.

### Drawing

Each frame the engine collects burning cells within 400 m of the camera and draws the
nearest 512 as animated flame sprites, scaled with intensity. The ash surface has no
texture layer and is drawn in its flat dark colour, so the terrain atlas and its golden
images do not change.

## Risks / Trade-offs

- Which chunks the fire holds depends on stream timing, so fire is not yet part of
  replays. `add-simulation-tiers` makes chunk arrival deterministic.
- A fire at the edge of the active tier stops there. The near tier's front model comes
  with simulation tiers.
- The radiation model is tuned, not physical. Its parameters are named constants in one
  place, so later playtests can adjust them.
