# Design: Touch and Gamepad Controls

## Decisions

- **Actions, not devices.** Scripts read `steer`, `climb`, `boost` and `look`; the
  bindings decide the devices. Every device reaches the same actions, so scenarios
  inject actions and cover all devices at once; the device-specific parts (inversion,
  dead zone, touch layout) are tested at their own level.
- **Inversion in the binding** (`gamepad:-right_y`) rather than in scripts, so a player
  can rebind climb to any axis with either direction.
- **Dead zone for single axes** (0.15, like sticks), so resting sticks and triggers read
  exactly zero and do not make the carpet drift.
- **Response curve in Lua** (`controls.curve`, `|v|^1.6` with the sign kept): a modder or
  a later options screen can change it without engine work; the exponent is deterministic
  (`^` uses the engine's pow).
- **Touch layout**: the left thumb steers (a floating stick), the right thumb climbs (a
  second stick), boost sits within reach of the right thumb; the controls are created
  once and coexist with the mouse fallback.

## Risks / Trade-offs

- [Two sticks and buttons crowd small phones] → sizes are relative to the screen; a
  layout editor comes with the options screen.
