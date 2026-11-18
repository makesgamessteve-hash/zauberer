# Proposal: Rigid bodies

## Why

Concept 5.10 wants solid things that fall, roll, collide and float: boulders
loosened by an Earth spell, logs a flood carries off, crates, thrown stones and
arrows, and later the debris of collapsing buildings. This is the last change of
M3. With it the simulation backbone is complete: fire, water, air, weather,
deformable ground and rigid bodies in one active tier.

## What Changes

- **Rigid bodies** (`sim/bodies`):
  - spheres, capsules and boxes of a registry material: its density gives the mass and
    its friction the grip;
  - they fall towards the planet's centre and collide with the 1 m heightfield and with
    each other;
  - contacts are resolved with sequential impulses (restitution, friction, rolling
    resistance) at a fixed 60 Hz;
  - boxes meet boxes through a separating-axis test with face clipping;
  - pairs are found on a 4 m grid;
  - bodies at rest sleep until a real knock, a push, or a change to the ground or
    water under them wakes them;
  - bodies outside the active tier are frozen where they are;
  - everything is in doubles on planet-centred coordinates and in a fixed order, so
    peers and replays agree.
- **Water**:
  - the balance now counts the water standing in chunks that left the active tier
    (`left`), so it holds after the player flies away;
  - bodies float by the volume under the water's surface (oak floats seven tenths
    under, granite sinks);
  - water slows them, and its flow carries them along;
  - water cells now report the water's velocity (`vx`, `vy`) from the flows through
    their faces.
- **Projectiles and sweeps**:
  - small spheres fly under gravity and drag;
  - each step is a swept test against the ground and the bodies, so a fast bolt never
    passes through a thin board;
  - a hit stops the projectile and hands its momentum to the body it struck;
  - `zb_bodies_sweep` casts a ray or sphere for spells.
- **The active tier** owns the bodies, steps and times them (`bodies_ms`), and wakes
  bodies whose ground was deformed.
- **Drawing**: low-polygon meshes in the material's colour (a faceted ball, a block,
  a ten-sided log with rounded ends) through a new `zb_render_draw_mesh`.
- **Saving**: bodies are kept with the game in `saves/<name>/bodies.lua`. Saves
  without that file load with no bodies.
- **Fixes found by the M3 exit demo** (`living_physics.lua`, now a headless test):
  - a chunk that comes back to the active tier with nothing burning shows its scars
    again;
  - a loaded chunk whose ash is due to go or whose plants are due to grow back is
    generated again from its surface up, so scars heal while the player stays near;
  - the water balance counts water in chunks that left the tier.
- **Lua**: `sim.spawn`, `sim.body`, `sim.push`, `sim.remove_body`, `sim.bodies_near`,
  `sim.body_stats`, `sim.launch`, `sim.projectile`, `sim.sweep`.

## Capabilities

### New Capabilities

- `rigid-bodies`: bodies, contacts, sleeping, buoyancy, projectiles and sweeps.

### Modified Capabilities

- `shallow-water`: cells report the water's velocity.
- `simulation-tiers`: the tier runs and times the bodies.
- `world-persistence`: saved games keep the bodies.

## Impact

- New: `sim/bodies`, `app/body_view`, `script/script_bodies`.
- Changed: `sim/water` (velocity, balance), `sim/fire` (scars on return), `sim/tier`,
  `world` (healing loaded chunks; `zb_stream_regenerate`, `zb_deltas_burn_due`), `render`
  (`zb_render_draw_mesh`), `script/script_save`, the engine.
- Not covered yet: collisions with voxel volumes and building pieces (M5 and later),
  and fallen trees as bodies (with the vegetation's damage states).
