# Proposal: The narrative director

## Why

M10 (The Story) completes the narrative (concept 1.7): the story is a graph of beats anchored into
the generated planet and its past, pointed to by rumours, never by markers; the same seed and choices
give the same story.

## What Changes

- `data/story.lua`: Weaves, the Circle, revelations, lieutenants, beats, endings.
- `story/narrative.lua`: open beats, site candidates and deterministic picks (reserved), remnant
  meetings, rumours with direction and distance, reunions for latecomers, beat triggers.
- `story/saga.lua` and `main.lua`: the story after the prologue begins (or resumes from the save);
  rumours reach people's talk; the HUD tells the open beat.
- The exit demo `tests/scenarios/story_bot.lua`: the whole story, from the prologue to each ending.
- M10. No platform differences: Lua on the engine's world, the same on desktop, web and WebXR.

## Impact

- New capability `narrative-director`. Co-op: the host's story; every heir sees its lines.
