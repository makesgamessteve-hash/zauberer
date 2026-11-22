# Proposal: A HUD with a place for everything

Milestone: UI redesign. Third of six changes; needs `add-ui-drawing-layer` and `add-ui-art-kit`.
Mockups: docs/design/ui/01-flight-hud.png, 02-flight-calm.png.

## Why

In play the HUD is 15 to 30 lines of one colour in the top-left corner:
- the version string;
- the speed, heading, altitude and distance flown;
- the control legend;
- health, needs, the pack's contents, mana, the hands, the free weave;
- the ritual, the place, "On foot", the story beat, the Circle's next remnant;
- mods' lines.

Every module appends its line to one list, so nothing has a place, nothing ranks above anything
else, and the numbers are raw ("Mana 7971/100", "37.0 C"). It covers the sky, the best part of the
picture, and it does not help the player decide what to do next.

## What Changes

- **Zones**, each with a framed home (see docs/design/ui/README.md for the full table of where each
  current line goes):
  - **Top middle, the compass band:** the heading; the goal as a saffron mark with its name and
    distance; known places nearby; the sun or moon.
  - **Top left, the objective card:** the current goal and its steps, ticked as they are done.
  - **Upper right, toasts:**
    - pickups, discoveries, "Game saved";
    - sound captions, with an arrow for their direction;
    - first-time hints.

    At most three at a time, newest on top, each fading after its time.
  - **Right edge, the carpet's gauge:** the height above the ground and the speed; the carpet's
    condition as its fringe.
  - **Bottom corners:** the two hands where a rider's hands are (the weave's icon and name, the
    binding chip, a charge bar), with the mana ring on the left wrist: a segment and glyph per colour,
    the total in the middle. The bottom middle is left to the carpet's front edge, the one part of
    the carpet the rider sees.
  - **Above the carpet's edge, the prompt chip:** the binding of the device last used and a verb ("Step
    off", "Talk to Farrokh", "Keep still").
  - **Above the left hand:** health beside a pomegranate, and the needs as icon and word, only while
    they want something.
- **The calm HUD:** after 8 seconds with no event and no change of input, the HUD draws back to the
  compass, the hands and health at reduced strength. Any event, input change or danger brings it
  back.
- **On foot:** the same zones without the gauge, with the carpet as a compass mark.
- **Rituals:**
  - progress is a ring around the circle's middle (it is in the world, not in the HUD);
  - "keep still" is the prompt chip.
- **Mods:** a framed mod slot under the toasts. Mods give it lines or icons through `modapi.hud`,
  and the slot keeps them in a frame.
- **The line list is retired.** `*.hud_lines` functions become data providers (`*.hud_state`) the
  zones read. No HUD text is drawn outside a frame, chip, plate or card.
- **Development builds only:** F3 shows a framed debug panel with the old details (speed, altitude,
  position, chunk stage, ticks).

## Platforms

- **Touch:** the zones move clear of the sticks and buttons (the hands sit over the hand buttons,
  the prompt chip is itself a button).
- **Gamepad:** chips show the pad's glyphs.
- **WebXR:** the zones are drawn on the HUD panel low ahead.

## Co-op

- The compass shows the other heirs as ally marks with their names.
- Quick messages arrive as toasts with the sender's name.

## Impact

New capability `hud`.

Code:
- `main.lua`: `flight_lines`, `draw_hud` and the walking and home variants are replaced by
  `ui/hud.lua`, with one file per zone under `ui/hud/`.
- Every module's `hud_lines` becomes `hud_state`.
- `modapi.hud`.

Tests that read HUD text (`hud_text`, string checks in scenarios) move to `hud.state()` and the null
backend's recorded calls.
