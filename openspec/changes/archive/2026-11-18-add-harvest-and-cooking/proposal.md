# Proposal: Harvest and cooking

## Why

Surviving seven days (the M5 exit demo) means living off the land. The mage must be able
to forage, hunt and butcher, fetch water, make a fire, cook and boil, brew a remedy,
and eat and drink. Cooked food should be better and safer, boiled water clean, and herbal
tea should cure the sickness that swamp water brings.

## What Changes

- **Food and drink as data** (`data/items.lua`):
  - energy and water per item, the chance of sickness eaten raw, what cooking makes of
    it, what boiling makes of water, what herbs make steeped as tea, poison, cures,
    dirt and salt, how it spoils, and firewood value;
  - new items: meat raw and cooked, hides, feathers, storm feathers, cooked
    fiddleheads, roasted reed roots, fried mushrooms;
  - water by the cup from rivers, lakes, pools, swamps and the sea, boiled water,
    herbal, bark and spruce tea;
  - creatures name their products (`data/creatures.lua`).
- **The pack** (`player/pack.lua`): what the mage carries outside the bag, up to 12 kg,
  a stack per item with its microbe load. Food spoils as game time passes, and what
  does not fit is left behind and told.
- **Harvest** (`life/harvest.lua`), on foot within 3 m:
  - butcher a carcass (its products, less what predators ate of it);
  - take a part of a plant (trees are not felled by hand; bees nearby give more
    fruit);
  - fill four cups at the water's edge (of a kind by the water);
  - pick the mushrooms of the patch.
- **Fire and food** (`life/cooking.lua`):
  - lay a campfire from the pack's firewood, to be lit with Kindle or any flame;
  - over a fire within 3 m, raw food cooks, water boils, and herbs steep in boiled
    water into tea;
  - eat and drink the best of what is carried: cooked before raw, remedies when sick,
    clean water before dirty, the sea last.
- **Firewood in the fire simulation:** dry wood can be laid on a cell (also one burnt
  before), and `sim.add_fuel` does this from Lua.
- **In the game:** gather (T), campfire (B), cook (K), eat (Y) and drink (U); the HUD
  shows the pack and what happened; the pack is saved with the player.

## Impact

- New capability `harvest-and-cooking`; the fire simulation takes laid firewood.
- The endless bag (M6) will hold what the pack cannot. Tools, preservation by smoking
  and salting, and recipes beyond tea come later.
