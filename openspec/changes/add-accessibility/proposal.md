# Proposal: Accessibility

## Why

M11 makes the game playable for more people (concept 8.6): remappable controls, toggle instead of
hold, gesture assist, colour that is never the only cue, captions, text size and contrast, reduced
flashing, a field of view, optional story markers and survival presets.

## What Changes

- `ui/settings.lua`: the settings, kept with the player's data and applied at once.
- `ui/menu.lua`: Escape in play opens the menu (resume, settings, controls with remapping, packages,
  credits, save and quit); the world goes on while the heir's actions are idle.
- `ui/access.lua`: input shaped by the settings, gesture assist, colour glyphs, captions with direction
  (thunder, fire close by, calling creatures), story markers; survival presets (wanderer, survivor, hermit).
- Colour vision modes: a correction of the image for deuteranopia, protanopia and tritanopia.
- Engine: `input.capture()`, `engine.set_flash_strength()`, `render.colour_vision()`.

## Impact

- New capability `accessibility`. Survival presets from M5 become a setting. There is no camera shake or
  head bob in the game; speech is always shown as text with the speaker's name.
