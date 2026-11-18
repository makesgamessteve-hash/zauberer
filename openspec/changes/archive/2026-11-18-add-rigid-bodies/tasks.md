# Tasks

## 1. Bodies

- [x] 1.1 `sim/bodies`: shapes and mass from the material, gravity, heightfield contacts, sequential impulses with restitution, friction and rolling resistance; verify "rigid-bodies: Bodies fall and come to rest" and "Friction holds or lets go"
- [x] 1.2 Pairs on a 4 m grid; sphere, capsule and box pairs, boxes by separating axes with face clipping; verify "Bodies collide"
- [x] 1.3 Sleeping and waking (knocks, pushes, ground and water changes); verify "Sleeping bodies wake"
- [x] 1.4 Water velocity per cell; buoyancy, drag and flow; verify "Wood floats and stone sinks" and "shallow-water: Water runs downhill"
- [x] 1.5 Projectiles and sweeps; verify "Projectiles never pass through"
- [x] 1.6 Frozen outside the active tier, counts, lists and removal; verify "Bodies outside the active tier are frozen" and "Deterministic bodies"

## 2. In the game

- [x] 2.1 The tier owns, steps and times the bodies and wakes them when the ground is deformed
- [x] 2.2 `zb_render_draw_mesh` and the body view; verify "Bodies are drawn"
- [x] 2.3 Lua `sim.spawn`, `body`, `push`, `remove_body`, `bodies_near`, `body_stats`, `launch`, `projectile`, `sweep`; verify "Bodies from Lua"
- [x] 2.4 Bodies in saved games (`bodies.lua`); verify "Bodies are kept with the game"
- [x] 2.5 A perf budget for 200 bodies; verify "Bodies budget"
- [x] 2.6 The M3 exit demo as a headless scenario (`living_physics.lua`); verify "simulation-tiers: The M3 exit demo"
- [x] 2.7 Scars on return to the tier and healing of loaded chunks; verify "simulation-tiers: Fires catch up when their chunk returns" and "world-persistence: Burnt ground heals"; the water balance counts water that left
- [x] 2.8 Regenerate the Lua API reference; lint; fast checks (unit, headless, rendered, perf)
