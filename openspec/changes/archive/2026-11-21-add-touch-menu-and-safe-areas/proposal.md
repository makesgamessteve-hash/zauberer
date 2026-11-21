# Proposal: The menu by touch; the game inside the safe area

Milestone: M11 (polish after the 1.0 RC).

## Why

On a phone the menu (settings, controls, save and quit) opened only with Escape, which touch
players have not got. The on-screen controls were never drawn: the boost, chime and credits
buttons worked but could not be seen. On phones with a notch or rounded corners the game ran
under them.

## What Changes

- The touch layout gets a Menu button (the menu's own action: it opens the menu and goes back)
  and an OK button (confirm); the steer stick chooses in the menu, and the menu's help line says
  so on touch.
- Every on-screen control is drawn with its label (sticks in round brackets, buttons in square).
- The web page keeps the game inside the device's safe area (viewport-fit=cover and the safe-area
  insets).

## Platforms

Mobile web and any touch device: the new buttons and labels. Desktop and VR: none (the
controls appear only in touch mode). Web on a phone: the safe area.

## Impact

Capability `touch-and-gamepad-controls` (modified "Touch layout"). `flight/controls.lua`,
`data/input.lua`, `ui/menu.lua`, `main.lua`, `web/index.html`, the German strings.
