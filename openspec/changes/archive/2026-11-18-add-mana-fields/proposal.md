# Proposal: Mana fields

## Why

Concept 3.2 makes mana energy with a flavour: eight colours, each tied to a physical
domain. Every weave is paid for from the mana around the mage, so the field must exist
before the spell grammar (M4) can price and power a weave. Changing the world must
change the mana: a forest fire spikes Fire, its ashes turn to Decay, and overdrawing
the land blights it.

## What Changes

- **The ley network** (`world/ley`), made once per world from its seed:
  - 64 nexuses on high ground, each with two colours and a power;
  - ley lines along great circles to each nexus's three nearest;
  - the nearest line and nexus from anywhere on the planet.
- **The mana field** (`sim/mana`):
  - eight colours on 32 m cells (8 per chunk edge) over the active tier;
  - each cell's richness comes from the physical state:
    - from the ground, plants and climate when the chunk comes in, and again when the
      ground is deformed;
    - from fire, water, wind, weather, daylight, burn scars and blight every two
      seconds;
  - ley lines add their colours, and nexuses add Void and theirs;
  - a cell holds 20 mana per unit of richness and fills again over about five
    minutes;
  - mana spreads slowly between cells and fast along ley lines;
  - drawing takes mana from the cells around a place;
  - draining a cell empty again and again within minutes blights it for 60 game days:
    little Life and much Decay;
  - the blight is a new world edit, kept with saves.
- **The active tier** runs the field with the other simulations, feeds it the weather and
  the ground's changes, and times it (`mana_ms`).
- **Lua**: `mana.colors`, `mana.at`, `mana.draw`, `mana.release`, `mana.stats` and
  `mana.ley`.

## Capabilities

### New Capabilities

- `mana`: the ley network and the mana field.

### Modified Capabilities

- `world-persistence`: the edits per chunk also keep blighted mana cells.
- `simulation-tiers`: the tier runs and times the mana field.

## Impact

- New: `world/ley`, `sim/mana`, `script/script_mana`.
- Changed: `world/delta` (`ZB_EDIT_BLIGHT`), `world/world` (`zb_world_ley`,
  `zb_world_blight`), `sim/tier`.
- The mage's own reservoir, motes and the cost of weaves follow in `add-mana-motes` and
  `add-weave-grammar`. Seeing ley lines (*Sense Void*) comes with the weave grammar.
