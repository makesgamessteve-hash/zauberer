# Proposal: Chemistry: substances, reactions and processes

## Why

Alchemy (concept 5.11) rests on a chemistry: real substances mixed in vessels, reactions that
run in proportion within a range of temperature, and the processes of the apparatus. M7's
healing draught and quintessence of light are brewed through it.

## What Changes

- **Substances** (`data/substances.lua`): phase, boiling point, solvent, whether they dissolve,
  and the item they are bottled as. Items say what substances they contain (`contains`).
- **Reactions** (`data/reactions.lua`): reactants to products in proportions, a temperature
  range with a best temperature, a rate (twice as fast ground), and some need a solvent, a
  catalyst or mana of a colour charged into the vessel. Steeping, bark extraction, the healing
  draught (life mana), the astringent, fermentation (yeast) and the quintessence of light.
- **Processes** (`life/chemistry.lua`): adding goods, charging mana, grinding, heating (reactions
  run; liquids boil off above their boiling point), distilling, filtering, reducing and bottling.
- **Knowing:** substances are unknown until analysed.

## Impact

- New capability `chemistry`. New items: spirit, healing draught, astringent tincture,
  quintessence of light (and later the M7 crafts).
