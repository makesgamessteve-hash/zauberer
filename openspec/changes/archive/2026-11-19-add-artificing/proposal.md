# Proposal: Artificing: enchanting things

## Why

M7's exit demo enchants a lantern of perpetual light. Artificing (concept 3.4) binds a weave
into a thing that carries it, as a lasting artifact or into the carpet and the bag.

## What Changes

- **Enchantments** (`data/enchantments.lua`, `magic/artificing.lua`): the lantern of perpetual
  light (on a horn lantern; an artifact that draws the land's ambient light mana and holds a
  charge), the carpet's upgrades woven into its slots (weatherproof weave, fire-ward lining,
  swift weave, silent glide) and the folding knot (six more folds in the bag).
- **Cost:** mana of the enchantment's colours, less the more the thing conducts that colour
  and the higher artificing's mastery.
- **At work:** artifacts tick: the lantern draws from the land, burns on its charge, and goes
  dark when it is spent; the game draws its light.
- **In the game:** the workshop's "." chooses among the crafts and enchantments and K does it;
  artifacts are saved with the game.

## Impact

- New capability `artificing`. `carpet-condition`'s upgrades can now be woven in the workshop.
