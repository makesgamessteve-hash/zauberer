# Proposal: The disciplines and their mastery

## Why

M7 brings the disciplines (concept 3.4): spellcasting, ritual magic, alchemy, artificing,
summoning and naturalism, mastered through practice, study and trials, and its exit demo.

## What Changes

- **Mastery** (`magic/disciplines.lua`, `data/trials.lua`): tiers Novice, Apprentice,
  Journeyman, Adept and Master at 0, 100, 300, 700 and 1500 experience. Practice (casts, brews,
  enchantments, rituals, summonings and bonds, things learnt) and study (an hour in the tent's
  study, 12 experience) grow it; experience stops at each tier until its trial is passed.
- **Better work:** each tier above Novice gives 10% more yield and 8% lower costs.
- **In the game:** F2 shows the disciplines' page; tiers gained are told; saved.
- **The M7 exit demo** (`tests/scenarios/the_disciplines.lua`).

## Impact

- New capability `discipline-progression`. Alchemy, artificing, ritual magic and summoning
  use its factors.
