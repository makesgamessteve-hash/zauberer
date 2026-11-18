# Proposal: The carpet as a cloak

## Why

The cloak is the carpet's second mode (concept 2.2): warmth, stealth and a safe way down.
M6's exit demo has the mage camouflage past a wary herd and survive a blizzard in the
cloak.

## What Changes

- **Wearing it** (`flight/cloak.lua`):
  - On foot by the carpet, H wraps it around the mage's shoulders. The carpet leaves the
    ground and goes with the mage, and there is nothing to step on.
  - H again spreads it out beside the mage.
  - In flight, H wraps it around the mage in the air. It spreads into a glider: the mage
    sinks at no more than 3 m/s and lands unhurt.
- **Warmth** (`player/survival.lua`):
  - The cloak is worth up to 20 degrees, adaptive like clothes: nothing in the heat. Wet,
    it is worth less.
  - It halves the wind's chill and keeps off rain.
  - Worn, the carpet itself is out of the rain and dries.
- **Colour:** "." shifts it along one slider: drab, plain, vivid.
- **The visibility model** (`sim.set_player(pos, on_foot, seen)`, C):
  - Creatures see the player at their sight range (scaled by daylight) times the player's
    visibility, 0 .. 2. Visibility is the cloak's contrast (drab 0.3, plain 0.8,
    vivid 1.4; plain clothes 1) times movement (still 0.6, walking 1, running 1.3).
  - On the carpet in the air the mage shows at 1.5, and half that with the silent glide
    upgrade.
  - Presence, 0 .. 3, scales how much a seen player frightens a creature: vivid awes
    at 2.5.
  - Hearing, at a quarter of a creature's range, is unchanged.
- **Wary creatures:** species get a flight distance, the range within which a mage they see
  frightens them (default 25 m). Deer keep 60 m.

## Impact

- New capability `carpet-cloak`.
- `creatures` gain the visibility model and flight distances; `survival` gains the
  cloak's warmth; `fauna.tick` passes how the player is seen.
