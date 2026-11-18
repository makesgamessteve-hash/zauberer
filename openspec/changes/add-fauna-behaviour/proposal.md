# Proposal: Fauna behaviour

## Why

The exit demo of M5 asks to "watch predators follow scent downwind". So far the
creatures only flee fire, hunt a player on foot and wander. They should live by their
needs: eat, drink and rest. Predators should hunt their prey, herds and packs should
move as one, and all of them should leave trails in the air for noses downwind.

## What Changes

- **Needs and the day:**
  - thirst grows while a creature is about; a thirsty creature looks for open water
    nearby, walks to its edge and drinks;
  - each species is about by day, by night, from dusk to dawn, or at any hour, and
    rests the rest of the time (never with a predator in view).
- **Hunting prey** (the food web of `data/creatures.lua`):
  - a hungry predator stalks the nearest prey it notices, gives chase when it is close
    or the prey bolts, and bites (harder for heavier hunters);
  - it gives up when spent and leaves prey alone for a while;
  - it eats the kill, and the carcass is eaten away;
  - a death tells its killer.
  - Runners have stamina and slow to a trot when spent: deer sprint, wolves last.
  - At night creatures notice others within earshot as well as in sight.
- **Prey:** watches a predator far off, flees one close by, and a herd stampedes when
  one of them takes fright.
- **Herds and packs:** keep apart, head the same way and stay together.
- **Scent:** every creature leaves its scent in the air. A hungry hunter that smells it
  follows the strongest trail upwind. The mage on foot, or low over the ground, leaves
  scent too.
- **Lua:**
  - `sim.define_species` takes activity, thirst, stamina and eats;
  - `sim.creature` gives thirst, the target and how much of a carcass is eaten;
  - `sim.set_needs(id, { hunger, thirst, energy })`;
  - deaths give `by` and `by_species`;
  - the HUD tells kills ("A grey wolf killed a red deer").

## Impact

- New capability `fauna-behaviour`; the creature budget now includes hunting, herds,
  thirst and scent.
- Territories, young, migration over seasons and hibernation come later.
