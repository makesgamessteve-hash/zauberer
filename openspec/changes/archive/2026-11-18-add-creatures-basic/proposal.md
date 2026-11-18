# Proposal: First creatures

## Why

Vertical Slice A needs living things to meet: wolves to scare away with fire, deer that
bolt from the flames, and a storm bird whose death leaves motes to collect. The full
ecology (populations, food webs, migration) is M5's; this change brings the first few
species into the active tier with simple senses and a utility choice of what to do,
built on the simulations that are already there (fire, water, air, bodies).

## What Changes

- **Species as data** (`data/creatures.lua`, CC BY 4.0): grey wolf, red deer and storm
  bird, with size, mass, speeds, flight and cruising height, diet, pack, senses, health,
  aggression, fears, resistances, and for magical ones their mana's colour and motes.
  Mods add or replace species by id.
- **Creatures in the active tier** (`sim/creatures`):
  - walkers keep to the ground, stand on ice and keep out of open water; fliers keep
    their height above the ground; outside the active tier creatures wait;
  - senses: sight (shorter at night), hearing, smell on the air's scent, fire nearby,
    and stimuli the scripts raise (noises, lights, threats);
  - five times a second, staggered, a utility choice between wandering, grazing,
    resting, fleeing, hunting the player and investigating a scent or noise; packs keep
    together; a hunter that reaches a mage on foot attacks;
  - harm from heat and flames, thick smoke and fast bodies, and from magic through the
    scripts, less the species' resistance; healing up to full health; death;
  - events for the scripts: deaths (with the cause, and the colour and motes of a
    magical creature), fleeing and attacks;
  - deterministic from the seed.
- **The tier** owns the creatures, steps and times them with the rest.
- **Lua**: `sim.define_species`, `sim.spawn_creature`, `sim.creature`,
  `sim.creatures_near`, `sim.hurt`, `sim.stimulus`, `sim.set_player`,
  `sim.creature_events`, `sim.creature_stats`, `sim.remove_creature`.
- **Drawing**: low-polygon walkers with swinging legs and fliers with flapping wings, in
  their species' colour; the dead lie on their side.
- **The game** loads the species and places a few of each near the start; the player's
  whereabouts reach the creatures every tick.

## Impact

- New capability `creatures`; `simulation-tiers` times the creatures as well.
- Herds, packs, populations and scent tracking in depth follow in M5
  (`add-ecology`, `add-fauna-behaviour`). Harm to the player from attacks follows in
  `add-damage-model`, motes from dead magical creatures in `add-mana-motes`.
