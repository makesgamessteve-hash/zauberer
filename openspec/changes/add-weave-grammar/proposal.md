# Proposal: Weave grammar

## Why

Magic is the heart of the game (concept 3). A weave combines five things:
- a verb (the words);
- one or two colours (the essence);
- a form (the gesture);
- modifiers;
- power (the charge).

The grammar is open: any combination can be tried, and the world answers through the
simulation, never through canned effects. With mana fields and gestures in place, this
change makes weaves castable: priced, judged, resolved, and carried out as heat, water,
force, ground, light and the rest.

## What Changes

- **The grammar as data** (`data/weaves.lua`, CC BY 4.0):
  - the eight verbs with their Old Tongue words and base costs;
  - the colours in wheel order, with what one mana does in each domain;
  - the eight forms with cost factors, reach and radius;
  - the modifiers (far, wide, lasting, many, delayed, upon touch, returning);
  - 36 named weaves for the journal (Kindle, Fireball, Unmake Fire, Freeze, Raise Earth,
    Blink, Healing Rain and more).
- **The rules** (`magic/weave.lua`):
  - validation, keys, names and the spoken words;
  - relations on the wheel (opposites, neighbours);
  - extent (reach, radius growing with the fourth root of power, duration, count,
    delay);
  - the cost: base × form × modifiers × distance × the place's mana (rich is cheap,
    thin is dear) × (1 − efficiency), with matter from nothing four times (stone) or
    twice (water) as dear;
  - familiarity tiers (Unfamiliar, Practised, Known, Mastered);
  - control from the gesture's accuracy and familiarity;
  - the misfire chance: poor control, opposing colours, overcharge and first casts raise
    it, neighbouring colours lower it;
  - deterministic resolution into a plan, with simulated misfires: backfire at the hand,
    stray, weak, late, overshoot.
- **The mage** (`magic/mage.lua`):
  - one reservoir for all colours;
  - a channeling rate;
  - attunement per colour, growing with casts and with time in rich places;
  - strain from overcharging, which blurs control and burns out at 90.
- **Effects through the simulation** (`magic/effects.lua`):
  - Fire: heat, heat sinks that put fires out, bolts that ignite where they land.
  - Water: condensing, drying, freezing and thawing, jets that douse.
  - Earth: raising and digging ground, conjured stones, hurled stones, stone to mud.
  - Storm: gusts that push bodies along their path, sparks.
  - Light: lights that fade.
  - Life: healing and withering events, plants removed.
  - Void: blinking the carpet, sensing ley lines, dispelling.
  - Decay: rot and purification.
  - Wards and the senses.
  - Two-colour pairs: Thunderball, Steam Blast, Healing Rain, Stone to Mud.
  - Bind and Call find no hold yet.
- **Casting** (`magic/cast.lua`):
  - aims along a ray;
  - pays from the reservoir (refusing when short or burnt out);
  - resolves and applies the weave;
  - records the cast (discovery, familiarity) and attunes the mage.
- **New simulation primitives**:
  - `sim.cool` (a heat sink; burning cells losing over 50 kJ/m² go out);
  - `sim.remove_water` (counted in the water balance as `removed`);
  - `sim.freeze` and `sim.thaw`: ice, on standing water and on the sea, lakes and rivers
    alike, which neither flows nor takes water in, can be walked on, melts after its
    time and is drawn as pale ice.

## Capabilities

### New Capabilities

- `weave-grammar`: weaves, their cost, control, misfires and effects, and the mage's mana.

### Modified Capabilities

- `shallow-water`: removal and freezing, and the balance counts removed water.
- `thermal-and-fire`: heat sinks.

## Impact

- New: `data/weaves.lua`, `magic/weave.lua`, `magic/mage.lua`, `magic/effects.lua`,
  `magic/cast.lua`.
- Changed: `sim/fire` (`zb_fire_cool`), `sim/water` (remove, freeze, thaw, melt),
  `app/water_view` (ice), `script/script_sim`.
- The exchange rate of Fire is 1 MJ per mana rather than the concept's 100 kJ: the
  fire simulation heats the fuel together with the top of the ground.
- The in-game controls, the HUD and keeping the mage and the journal in saves come with
  `add-spellbook`. Healing and withering reach bodies with `add-damage-model`.
