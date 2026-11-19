# Proposal: People: generation, daily life, perception, dialogue, services

## Why

Settlements need their people (concept 6.10): generated lazily, living by the day, seeing the
heirs, talking by topics, offering services.

## What Changes

- `people/npcs.lua`, `people/folk.lua`, `data/dialogue.lua`: households with names in their
  language, roles, trades, faiths and traits (Farrokh in Anarvan); schedules (sleep, meals, work,
  prayers, bazaar, teahouse, festivals; shelter from rain; buckets to fires; raids); attitude
  from standing, cloak, magic seen and guest right; topics (rumours, directions, history, faith,
  songs, grudges, prices, work); services (lodging, healing, teaching, trade, guides); requests
  from the world; met people remembered. Drawn as `sprites/person`.
- M8. No platform differences: all of it is Lua on top of the engine's world queries, the same on desktop, web and WebXR.

## Impact

- New capability `npcs`. Co-op: people are the same for every heir and move by the host's clock;
  reputation is per settlement for the family, and memory of meetings is shared.
