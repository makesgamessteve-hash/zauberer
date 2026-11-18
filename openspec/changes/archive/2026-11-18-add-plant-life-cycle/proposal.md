# Proposal: Plant life cycle

## Why

M5 brings living things and survival. Plants are the base of it: what can be
foraged, when, and from which plant. Wood, bark, nuts, herbs and fibre all come from
plants, and the land should heal after it is used. So far plants are placements
without a life.

## What Changes

- **Life cycles as data** (`data/plants.lua`, CC BY 4.0), one per flora species:
  - when it is in leaf, in flower and in fruit, in seasons of the local year;
  - its parts: the item each gives, when it can be taken (with fruit, flowers or
    leaves, or all year), how many for the smallest and the largest plant, how many
    game days until the same plant gives it again, and whether taking it takes the
    whole plant.
- **Items as data** (`data/items.lua`, CC BY 4.0): the first items, those plants give
  (nuts, fruit, bark, resin, sap, wood, fibre, herbs, roots), with a kind and a weight.
  Later changes add creatures' products, food values and cooking.
- **Plants through the seasons** (`life/plants.lua`):
  - each plant follows its local season (half a year apart across the equator), up
    to about a week early or late by its own variant;
  - it offers its parts by stage, with counts by its size that differ from year to
    year but not between one look and the next;
  - taking a part remembers it until the part has regrown;
  - taking a whole plant cuts it.
- **Cut plants grow back** (succession for felling): a new edit of the world keeps
  when a plant was cut. It is gone until its layer has grown back (ground cover in
  15 days, shrubs in a year, trees in five), and the chunk is generated again when it
  is due.
- **Lua**: plants carry an id, size and variant; `world.cut_plant(id)`.

## Impact

- New capability `plant-life-cycle`; `world-persistence` keeps cut plants.
- Harvesting in play (the pack, the gather action) comes with
  `add-harvest-and-cooking`, food values with survival. Planting saplings, drought
  stress and fruiting shown in the plant sprites come later.
