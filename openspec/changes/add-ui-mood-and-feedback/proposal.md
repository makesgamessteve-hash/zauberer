# Proposal: A UI that takes the mood of the world

Milestone: UI redesign. Last of six changes; needs `add-ui-drawing-layer`, `add-ui-art-kit` and
`redesign-hud`.

## Why

Once the UI has frames, it can belong to the moment rather than float over it. A HUD that looks the
same at noon, at dusk, in a burning village and in the tent at night breaks the mood the world builds.
And with frames and icons the existing "text size" setting needs to become a real UI scale.

## What Changes

- **Light of the hour:** HUD frames take a tint from the sky:
  - warm saffron at dawn and dusk;
  - lapis at night (the parchment of the book stays paper);
  - shaded under a storm.

  The tint is gentle (at most 20 %) and never changes the meaning of a colour.
- **Danger at the edges:** while danger is near (a hunter, fire, low health, a duel) the screen's
  edges warm with an ornamental vignette in pomegranate, stronger as it closes in. Low health beats
  slowly with the heart. Damage shows a short mark on the edge it came from.
- **Feedback:**
  - frames unfold when they appear (a 120 ms slide and fade);
  - toasts slide in;
  - the objective card's ticks pop;
  - the mana ring pulses when mana comes or goes;
  - the hand medallions flash when a weave goes off.
- **UI sounds:**
  - a soft cloth sound for a frame appearing;
  - a page turn for the book;
  - a chime for a new goal;
  - a quill scratch for a journal entry.

  All of them are generated and credited, and their volume is part of effects.
- **UI scale** replaces text size: 1, 2 or 3 grid pixels per UI pixel on top of the window's scale.
  Frames, icons and text grow together, and the zones re-anchor.
- **Reduced motion** (from the accessibility settings) turns slides, pulses, pops and the heartbeat
  into plain appearance, keeping their meaning.

## Platforms

All the same. In WebXR the edge vignette is the existing comfort vignette, tinted.

## Co-op

Local to each heir.

## Impact

New capability `ui-mood`. Modified requirement: `settings` Text size and high contrast (now UI scale).

Code:
- `ui/mood.lua`: the tint, danger, transitions;
- `ui/layout.lua`: the UI scale;
- `tools/gen/gen_sounds.py`: the UI sounds;
- the settings page.
