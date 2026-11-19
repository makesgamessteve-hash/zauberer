# Proposal: Ritual magic

## Why

M7's exit demo calls rain over a drought-struck valley. Rituals (concept 3.4) are slow,
strong magic laid on the ground at the right time, channelled from the land around.

## What Changes

- **Rituals** (`data/rituals.lua`, `magic/ritual.lua`): a circle laid on foot with ingredients,
  at a time (twilight, night, day), channelled still and unhurt for some seconds. Its mana is
  drawn from the land within 64 m first, the rest from the mage.
- **Broken:** moving out of the circle, being hurt or running out of mana breaks it, and the
  drawn mana lashes back.
- **Effects** for hours: Call rain (rain over the land for six hours, then the world's weather
  again), Ward the camp (beasts keep out), Unfold a room (a new room of the pocket home, by the
  tent at night). Ritual magic's mastery makes rituals cheaper.
- **In the game:** / lays the circle for the chosen ritual, ' chooses the next; the circle and
  its progress are drawn and told.

## Impact

- New capability `ritual-magic`. `weather` is held by Call rain.
