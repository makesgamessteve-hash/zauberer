# Proposal: The Codex

## Why

Knowledge is an axis of progression (concept 7.3, 7.4): the mage learns the living world
by watching it and taking samples, and that knowledge keeps them alive. The first two
hours (concept 7.8) have the Codex warn of a poisonous mushroom once it is observed.
M5 needs the natural-history part of the Codex for the plants, fungi, animals and insects
that now live in the world.

## What Changes

- **Entries** (`life/codex.lua`): one for each plant, fungus, animal and insect species
  of the data (`data/flora.lua`, `data/fungi.lua`, `data/creatures.lua`, the new
  `data/insects.lua`), for example `plant:oak` or `animal:wolf`.
- **Levels** grow Seen → Observed → Studied → Understood:
  - Seen: in sight, within 35° of the middle of the view and in range (plants 40 m,
    fungi 12 m, insects 25 m, animals 200 m).
  - Observed: looked at, the thing nearest the middle of the view (big things count their
    size), for 3 s, or 8 s for animals.
  - Studied: a sample taken (harvested, butchered, picked). Insects, which cannot be
    sampled yet, are studied by watching them for 30 s.
  - Understood: after three samples, or 90 s of watching an insect.
  - Each new level from Observed is told on the HUD. Looking shows its progress.
- **Pages** reveal more by level:
  - what it is;
  - where and when it lives (climate, habitat, seasons, activity, groups, prey);
  - what it gives and what that is worth;
  - what it is good for (cures, teas, fears, resistances) and that the right method
    gives more.
  - From Observed, the page and the news warn of dangers: poison, attack, bites.
- **Rewards:**
  - Once an entry is understood, what is taken from it (harvested, butchered or picked)
    yields a quarter more.
  - Eating avoids a poison only once the Codex has warned of it. An unknown mushroom is
    tasted like any food.
- **In the game:**
  - The Codex looks from the mage's eyes on foot, or along the chase camera in flight.
  - Gathering samples what it takes.
  - J opens the Codex at the entry last learned, turns the pages and closes it.
  - The Codex is saved with the player.
- **HUD news** for the pack and the Codex fades in real seconds (`player/news.lua`). Pack
  news used to fade in game seconds, a tenth of a real second.

## Impact

- New capability `codex`. `harvest-and-cooking`'s gathering reports what it sampled and
  takes a yield factor, and eating takes what the Codex has warned of.
- Minerals, phenomena, peoples and history, lab study, books and teachers come with later
  milestones. Looking does not yet check whether terrain hides a thing.
