# Proposal: Peopling: peoples, languages, faiths, realms and the history database

## Why

M8 (Places and People) begins with who lives on the planet. The concept (10.4-10.11) has the
world peopled after deep time, either directly (the default *Settled world*) or by a simulation;
both write one history database that settlements, people and lore read.

## What Changes

- **The land in regions** (`people/land.lua`): about 100 km regions with height, relief, climate
  (from a created world, or from latitude, height and a seeded noise in the noise world), rivers
  from rain running downhill, oases, fertility, minerals, the ley; neighbours across cube faces.
- **Peoples** (`people/peopling.lua`, `data/style_families.lua`, `data/peoples.lua`): cradles in
  the most fertile spaced regions, spread by cheapest paths (rivers and coasts easy, deserts and
  mountains hard), a style family by the homeland, subsistence, values, magic, calendar.
- **Languages** (`people/language.lua`): phonology per family, daughters by sound changes,
  names by kind, a lexicon of roots, kinship.
- **Faiths**, **realms** (types, laws on mages, borders along rivers), **settlements** sited and
  typed, **roads** and caravanserais a day apart.
- **The history database** (`people/history.lua`) and **the chronicle** (`people/chronicle.lua`):
  written backwards from the present (wars won by today's holders, dynasties, tombs, ruins,
  elder ruins), with the story woven in (Anarvan where the carpet sets out, Farrokh, the Circle
  of the Loom, Azhdar). Generation runs as a job in slices (`people/job.lua`).
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `peoples-and-cultures`. Co-op: every heir's game makes the same history from
  the seed; the host's changes travel as deltas.
