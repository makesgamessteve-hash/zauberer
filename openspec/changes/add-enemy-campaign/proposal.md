# Proposal: Azhdar's campaign

## Why

Azhdar's campaign makes the world darker as time passes (concept 1.7): paced by progress, never
failing the story.

## What Changes

- `story/campaign.lua`: targets by distance from Azhdar's origin with the Circle's sites among
  them; strikes every 4 days at first, sooner with progress; burnt places, blight, refugees,
  rumours; the ley weakened by 0.02 a strike (`mana.set_ley_strength`).
- M10. No platform differences: Lua on the engine's world, the same on desktop, web and WebXR.

## Impact

- New capability `enemy-campaign`; `mana.set_ley_strength` (C). Co-op: the host's campaign.
