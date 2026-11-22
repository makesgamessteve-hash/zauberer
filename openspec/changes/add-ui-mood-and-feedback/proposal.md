# Proposal: A UI that takes the mood of the world

Milestone: UI redesign. Last of six changes; needs `add-ui-drawing-layer`, `add-ui-art-kit` and
`redesign-hud`.

## Why

Once the UI has frames, it can belong to the moment rather than float over it. A HUD that looks the
same at noon, at dusk, in a burning village and in the tent at night breaks the mood the world builds.
And with frames and icons the existing "text size" setting needs to become a real UI scale.

## What Changes

- **Light of the hour:** HUD frames take a tint from the sky: warm when the sun is low, lapis at
  night, shaded under a storm. The tint is gentle (at most 20 %), never touches the colours that
  carry meaning, and high contrast is never tinted.
- **Danger at the edges:** while a hunter is near the screen's edges warm with a pomegranate band.
  Under 30 % health the band beats slowly with the heart (steadily with reduced flashing). A hit
  marks the edge it came from for a moment.
- **UI sounds:** a soft cloth sound for a toast, a page turn for the book, a chime for a new goal;
  generated and credited, at the effects volume.
- The chapter card's band draws out from the middle and toasts fade in and out (add-story-presentation,
  redesign-hud).
- Left for a later change: a UI scale for frames and icons (the text size setting stays as it is).

## Platforms

All the same. In WebXR the edge vignette is the existing comfort vignette, tinted.

## Co-op

Local to each heir.

## Impact

New capability `ui-mood`. `ui/mood.lua` (the tint, danger, hits), `ui/kit.lua` (frames tinted),
`tools/gen/gen_sounds.py` (the UI's sounds), `player/soundscape.lua`, `main.lua`.
