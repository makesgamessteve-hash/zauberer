# Proposal: Damage model

## Why

Magic and creatures need consequences. Wolves should be dangerous to a mage on foot,
fire should hurt whoever stands in it, and weaves should harm and heal the creatures
they reach. Without a damage model, Vertical Slice A has no stakes and the storm bird
cannot be brought down by magic.

## What Changes

- **The player's health** (`player/health.lua`):
  - one pool of 100 hurt through the same harms as creatures (heat, impact, cold,
    poison, smoke, magic);
  - harm from flames and heat under a player on foot or within 3 m of the ground, thick
    smoke, bites while on foot, hard landings, and weaves going off in the hand;
  - a ward of the harm's colour lets only 30 % through (Fire against heat, Earth against
    blows, Water against cold, Storm against smoke);
  - healing weaves mend, and health comes back slowly after ten seconds unharmed;
  - at no health the player is defeated, lies still for three seconds, and gets up
    again at half health where the carpet first set out;
  - health and the count of defeats are saved with the player.
- **Magic against creatures** (`magic/effects.lua`):
  - a bolt passing within 1.5 m of a living creature strikes it (fire burns, water and
    stone strike) and goes off there;
  - withering hurts the creatures it reaches and healing mends them;
  - a spark jolts what it touches.
- **In play**: the HUD shows health and what hurt; a defeated player's controls rest
  until revived.

## Impact

- New capability `damage-model`.
- Survival needs (hunger, body temperature, disease) follow in M5 (`add-survival`).
