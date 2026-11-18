# Proposal: Touch and Gamepad Controls

## Why

Milestone **M1 (First Flight)** runs on desktops, desktop browsers and phones. The
flight must be equally controllable with keyboard and mouse, a gamepad and a touch
screen (concept §8.4: a floating virtual stick for movement and flight, the right side
for looking and climbing).

## What Changes

- **Flight actions** in the base package's default bindings: `steer` (WASD/arrows, left
  stick, touch stick), `climb` (R/F, Page Up/Down, right stick up/down), `climb_touch`
  (a second touch stick), `boost` (Shift, right shoulder, touch button) and `look`
  (mouse, right stick).
- **Inverted gamepad axes**: a binding such as `gamepad:-right_y` reverses an axis
  (SDL reports "up" as negative), and single gamepad axes get the same dead zone as
  sticks.
- **Response curves**: stick and key values are shaped with an exponent of 1.6 for fine
  control near the centre.
- A **touch layout** for flight: steer stick bottom left, climb stick bottom right, boost
  and chime buttons beside it, credits top right; added once in touch mode.

## Capabilities

### New Capabilities

- `touch-and-gamepad-controls`: flight bindings per device, axis inversion and dead
  zone, response curves and the touch layout.

### Modified Capabilities

None.

## Impact

- `src/input/input.c` (inversion, dead zone), `assets/base/data/input.lua`,
  `assets/base/scripts/flight/controls.lua`.
- Platforms: touch controls appear on touch-primary devices (mobile web) and switch on
  at the first touch; gamepads work on desktop and in browsers with the Gamepad API.
- Co-op: input is per player (the input snapshot carries the player index); nothing else
  changes.
