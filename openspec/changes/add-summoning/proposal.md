# Proposal: Summoning and familiars

## Why

M7's exit demo summons a water elemental from a lake and bonds a familiar. Summoning
(concept 3.4) calls beings formed from local matter, kept by mana, and bonds wild animals.

## What Changes

- **Bound beings** (C, `sim.bond`): a bound creature follows the player within some metres and
  neither fears nor hunts the player; new creature action `follow`.
- **Summons** (`data/summons.lua`, `magic/summoning.lua`): a water elemental formed from water
  within 20 m, a wisp of light anywhere; mana to call and upkeep each second; unpaid, the
  binding weakens by the being's will until it fades. Summoned species are not wildlife.
- **Familiars:** a wild fox, hare or crane close by, offered what it eats and life mana, unless
  too afraid, bonds for good: it leaves the ecology, follows the mage, and is saved.
- **In the game:** "," summons, ";" bonds, "-" dismisses.

## Impact

- New capability `summoning`. `creatures` gains bonds; `ecology` releases a bonded animal;
  `codex` skips summoned species.
