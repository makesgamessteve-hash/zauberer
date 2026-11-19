# Proposal: Lore: chronicles from a side, legends, inscriptions, knowledge, the journal

## Why

History must reach the player as lore (concept 10.11): told from a people's side, drifting into
legend, carved in invented scripts, known by people, gathered in the Codex.

## What Changes

- `people/lore.lua`: war accounts (victory for the winners, treachery for the losers, plain for
  strangers), books of kings (enemy kings blackened), legends that drift with age and distance,
  inscriptions in a people's language and script, what people know (history, faith, grudges,
  songs, rumours), and the lore journal (Peoples, Faiths, Realms, History).
- `tools/gen/gen_scripts.py`: seven invented 5x7 scripts (`fonts/script_*.lua`).
- Books found in houses are read into the journal; talk teaches peoples and faiths.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `lore`. Co-op: the journal is shared by the heirs (the library of the home).
