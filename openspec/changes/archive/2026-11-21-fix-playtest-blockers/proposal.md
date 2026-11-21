# Proposal: What stopped the bots in the prologue

## Why

Ten bot playthroughs (docs/playtests/2026-11-21-ten-bot-runs.md) never finished the prologue.
Lightning lit the grass beside the heir, who got up again in the same flames; a wolf brought the
heir down 93 times, each time beside it; the burning village reached into the warded cellar and
its embers burnt the heir at dawn; after the bond the carpet hovered over the cellar, out of
reach; and the places of the chores were hard to find (the HUD named none of them).

## What Changes

- Natural lightning strikes no nearer than 100 m to the player.
- Standing in flames or on scorching ground the HUD says to get away from the fire.
- Below ground (a cellar, a cave) the surface's fire and smoke do not reach.
- A defeated player gets up where the carpet first set out unless fire or a hunter is within
  30 m; then at the nearest place round it clear of both. The hunters near turn away for a while.
- Anarvan's fires are out by the morning after the night it burns, and its doors are burnt: a door
  the heir opened or shut no longer comes back in the ruin.
- The carpet that chose the heir is unrolled beside them once they are up in the open.
- The story's HUD names the next place, its distance and direction.
- The gameplay bot finds its way (an A* search round walls) and runs from fire.

## Impact

- Capabilities `damage-model`, `creatures`, `simulation-tiers`, `vertical-slice`.
- `src/sim/tier.c`, `src/sim/creatures.{c,h}`, `src/script/script_creatures.c` (`sim.spare_player`),
  `player/health.lua`, `life/fauna.lua`, `story/prologue.lua`, `story/director.lua`, `main.lua`,
  the German strings, `tests/bots/play_bot.lua`.
