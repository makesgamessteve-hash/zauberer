# Proposal: Factions and the economy

## Why

Trade, prices and standing make places differ (concept 6.11, 6.12): salt is cheap on the coast
and dear in the mountains, towns take coin, villages barter, caravans travel.

## What Changes

- `people/economy.lua`: production by land and people, supply along roads, prices by supply
  and demand, coins in towns and barter in villages, factions (realms, faiths, guilds, free
  mages, the Ashen, bandits, nomads) with stances, standing by deeds (prosperity follows),
  caravans on trade routes at a caravan's pace. New trade goods in `data/items.lua`.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `factions-and-economy`. Co-op: prices and caravans are the host's; standing is
  the family's.
