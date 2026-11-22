# Proposal: The end of the story, shown

Milestone: M11 (polish after the 1.0 RC).

## Why

Four endings could be reached, but choosing one only put two lines on the HUD for a few seconds.
The end of a long story deserves its own moment, the credits, and a clear way back into the
changed world.

## What Changes

- When an ending is chosen the game shows "The end of the story": the ending's name and closing
  lines, the chronicle of the heirs' own people (Anarvan's), and "Your story is told; the world
  goes on."
- Then the credits, which can be skipped, and play goes on in the world after the ending.
- The world waits while it is shown (alone). The save records that the ending was seen, and the
  game is autosaved.
- The saga keeps each people's chronicle of the ending.

## Platforms

All the same.

## Co-op

The host's choice ends the story for everyone. Each heir sees the end on their own screen; the
world goes on in company.

## Impact

Capability `story-act-3-and-endings` (added "The ending shown"). `story/saga.lua`, `ui/menu.lua`,
`main.lua`, the German strings.
