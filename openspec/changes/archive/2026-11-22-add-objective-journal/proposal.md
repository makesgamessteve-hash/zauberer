# Proposal: The journal of goals and words

Milestone: M11 (polish after the 1.0 RC).

## Why

The "next place" line covered only the prologue. Requests people made were said once and lost,
story lines scrolled off the HUD after a few seconds, and the timed story moments (Azhdar's
arrival, the hiding) could not be hurried.

## What Changes

- A journal page in the menu shows:
  - the current goal with its place, distance and direction (the story's own lines);
  - the requests people made (who asked, and in which place);
  - what is finished (prologue goals and saga steps);
  - the last 100 lines said, with the game time, newest first.
  It is kept with the save.
- Everything said on the HUD goes into the journal (a repeat only once). Requests heard in talk
  ("work") are kept once per place.
- Holding use for a second hurries Azhdar's arrival or the hiding to their end, with every line
  of it still said.

## Platforms

All the same. On touch the menu button opens the menu, where the Journal is.

## Impact

Capability `narrative-director` (two added requirements). `ui/journal.lua`, `ui/menu.lua`,
`player/news.lua`, `people/folk.lua`, `story/prologue.lua`, `main.lua`, the German strings.
