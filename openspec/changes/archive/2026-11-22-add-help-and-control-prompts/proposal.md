# Proposal: Help, hints and controls named for the device in hand

Milestone: M11 (polish after the 1.0 RC).

## Why

Players did not know which buttons do what: casting by gesture, stepping off the carpet, the bag,
the tent and the cloak were never explained, and nothing said where the menu was. The game is
played with keyboard and mouse, gamepads, touch and VR controllers, so a prompt must name the
control of the device in hand.

## What Changes

- The engine reports the device used last (`input.device()`: keyboard and mouse, gamepad or
  touch; stick drift does not count).
- Prompts name the control of that device for an action, and change when the player switches.
- First-time hints show once each when the heir can first do something: fly, the menu, step off,
  get on, use, cast, the bag, sleep when tired, eat when hungry. They are kept with the player's
  data, turned off in the settings ("Hints"), and shown again from the help page.
- Help pages in the title and the menu: the controls of the device in hand, flying, walking,
  casting (the eight forms), survival, the bag and tent, and the story so far.

## Platforms

Desktop: keyboard and mouse, or a gamepad as it is used. Mobile web: the touch controls. VR: the
HUD panel shows the same lines.

## Impact

New capability `help`. `src/input/input.{c,h}`, `src/script/script_api.c`, `ui/help.lua`,
`ui/menu.lua`, `ui/settings.lua`, `main.lua`, the German strings.
