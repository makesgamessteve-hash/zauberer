# Proposal: The alchemy laboratory

## Why

The healing draught of M7's exit demo is brewed in the tent's alchemy laboratory, from
gathered herbs, by a recipe that drives the chemistry for the hours it takes.

## What Changes

- **Recipes** (`data/recipes.lua`, `magic/alchemy.lua`): ingredients, then steps (grind, heat,
  filter, charge mana, reduce, distil, bottle) for hours. Healing draught (healing herbs, boiled
  water, life mana), astringent tincture, spirit of wine (dates), quintessence of light.
- **Brewing:** what is lacking is told; the hours pass; what was in the vessel after each step
  becomes known; the yield grows with alchemy's mastery.
- **In the game:** the laboratory is open in the pocket home from the start: "." chooses the
  recipe and K brews it. A healing draught drunk mends 40 health.

## Impact

- New capability `alchemy-lab`. `carpet-tent`'s rooms: the laboratory and the workshop open
  from the start. `survival`'s drinking hands the draught's healing to the body.
