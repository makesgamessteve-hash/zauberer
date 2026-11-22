# Proposal: The UI's art kit

Milestone: UI redesign. Second of six changes; needs `add-ui-drawing-layer`.

## Why

The concept asks for "hand-drawn frames with carpet-border ornament" and a palette of lapis,
turquoise, saffron, pomegranate and ochre (docs/concept/08 §8.1). Art is procedural-first, so the
frames, pages and icons the new UI draws come from a generator, reproducible and CC BY 4.0 like the
rest of the packages.

## What Changes

- `tools/gen/gen_ui.py` writes `assets/base/ui/`:
  - **Frames** as nine-slice textures with their slice widths in `ui/frames.lua`:
    - `kilim`: an ink edge, a pomegranate band with saffron diamonds, a turquoise inner line and
      knotted corners. Used for panels that matter (the objective card, vitals, speech).
    - `thread`: an ink edge and one saffron line, for light panels (toasts, the compass, the gauge).
    - `plate`: a pomegranate name plate.
    - `chip`: a key or button chip.
    - `page`: the book's parchment with darkened edges and a gutter.
    - `band`: the story card's ink band edged with saffron dots.
  - **The icon atlas** (`ui/icons.png`, 16×16 cells, named in `ui/icons.lua`):
    - needs: health, thirst, hunger, tiredness, cold, heat;
    - the items of the pack and bag;
    - each weave's form;
    - the eight mana colours with their glyphs;
    - compass marks (goal, place, heir, carpet, ally);
    - device glyphs for prompts: mouse buttons, gamepad faces and shoulders, touch buttons.
  - **The palette** in `ui/palette.lua`: named colours (ink, lapis, turquoise, saffron, pomegranate,
    ochre, parchment and their shades) that every UI script uses instead of literal numbers.
  - **Variants:**
    - high contrast: solid ink fills and a white inner line;
    - one per colour-vision mode: mana colours chosen to stay apart, glyphs unchanged.
- The credits list the generated UI art.

## Platforms

All the same.

## Co-op

No effect.

## Impact

New capability `ui-art-kit`. `tools/gen/gen_ui.py`, `assets/base/ui/*`, `assets/base/credits.lua`,
the asset index. Reproducible like the other generators (same output from the same seed).
