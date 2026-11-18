# Proposal: Gesture recognition

## Why

A weave's form is drawn (concept 3.3): a line for a bolt, a circle for an area, a
square for a wall. The weave grammar needs to know which of the eight forms a stroke is,
and how well it was drawn, because accuracy becomes the weave's control. The plan asks
for 2D strokes now, from a mouse, a finger or a stick; 3D gestures in XR come with M11.

## What Changes

- **The recogniser** (`input/gesture`):
  - takes a stroke of 2D points and resamples it evenly along its length;
  - centres and scales it, and compares it with templates of the eight forms by
    their cosine distance at the best rotation (Protractor);
  - the forms are tap, line, circle, spiral, triangle, square, zigzag and wave;
  - strokes may be drawn in any orientation, either way round or mirrored, and
    closed forms from any point on their outline;
  - strokes shorter than a tap size are taps;
  - strokes like no form (accuracy below 0.55) are refused;
  - it reports the best and second-best form with their accuracies, the stroke's
    size, centre and direction;
  - **gesture assist** (accessibility) judges sloppy shapes more kindly.
- **Lua**: `gesture.forms`, `gesture.recognize` and `gesture.template`.
- **Drawing** (`magic/stroke.lua`): while the draw button is held, mouse motion (or a
  stick or finger, integrated over time) traces a stroke. On release it is
  recognised. The trail is kept, scaled for the HUD. The bindings and the HUD come
  with the spellbook, where casting is wired into the game.

## Capabilities

### New Capabilities

- `gesture-recognition`: the eight forms of weaves from drawn strokes.

## Impact

- New: `input/gesture`, `script/script_gesture`, `assets/base/scripts/magic/stroke.lua`.
