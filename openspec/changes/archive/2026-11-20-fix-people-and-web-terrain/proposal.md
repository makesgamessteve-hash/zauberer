# Proposal: People are seen; the terrain is textured in the browser

## Why

Screenshots of the game as players see it showed no people anywhere and, in the browser, bare
untextured ground. The person sprite's pivot was at its top, so every person was drawn below the
ground; the web build read the terrain atlas before it was fetched.

## What Changes

- The person sprite stands on its feet (pivot at the bottom, as the renderer measures it).
- The terrain atlas and its texture are part of the web boot set (read while the engine starts).
- A rendered test that sees people; a browser test that the atlas is there.

## Impact

- Capabilities `npcs` and `platform-targets`: one requirement each. `tools/gen/gen_sprites.py`,
  `assets/base/sprites/person.lua`, `tools/make_asset_index.py`.
