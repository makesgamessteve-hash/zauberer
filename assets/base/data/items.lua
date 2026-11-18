-- SPDX-License-Identifier: CC-BY-4.0
-- The items of the base game: what plants, fungi, creatures and the land
-- give, and what is made of them. Mods add or replace items by id.
--   name, kind ("food", "herb", "material", "water"), weight in kg per item
-- Food and drink (concept 7.2):
--   kcal      energy per item
--   water     litres per item
--   raw       chance of falling sick eaten raw (cooking makes it safe)
--   cooked    the item it becomes over a fire
--   boiled    the item water becomes boiled over a fire
--   brew      the drink it makes steeped in boiled water
--   poison    how strongly it poisons (0 .. 1)
--   cures     { sick = .., poison = .. }: how much of each it takes away
--   dirty     chance-weight of falling sick from it (water)
--   salty     makes one thirstier
--   spoils    microbes grow on it ("fresh", "cooked", "dried"; omitted: it keeps)
--   fuel      kg of firewood it makes
return {
  -- From trees
  { id = "acorns", name = "Acorns", kind = "food", weight = 0.004, kcal = 12, raw = 0.02, spoils = "dried" },
  { id = "beechnuts", name = "Beechnuts", kind = "food", weight = 0.002, kcal = 11, spoils = "dried" },
  { id = "hazelnuts", name = "Hazelnuts", kind = "food", weight = 0.003, kcal = 19, spoils = "dried" },
  { id = "pine_nuts", name = "Pine nuts", kind = "food", weight = 0.001, kcal = 7, spoils = "dried" },
  { id = "coconut", name = "Coconut", kind = "food", weight = 1.2, kcal = 350, water = 0.3, spoils = "dried" },
  { id = "dates", name = "Dates", kind = "food", weight = 0.008, kcal = 22, spoils = "dried" },
  { id = "spruce_tips", name = "Spruce tips", kind = "herb", weight = 0.002, kcal = 1, brew = "spruce_tea",
    spoils = "fresh" },
  { id = "birch_sap", name = "Birch sap", kind = "water", weight = 0.25, kcal = 2, water = 0.25, spoils = "fresh" },
  { id = "oak_bark", name = "Oak bark", kind = "herb", weight = 0.05, brew = "bark_tea" },
  { id = "birch_bark", name = "Birch bark", kind = "herb", weight = 0.05, brew = "bark_tea" },
  { id = "resin", name = "Resin", kind = "material", weight = 0.02 },
  { id = "acacia_gum", name = "Acacia gum", kind = "material", weight = 0.02, kcal = 5 },
  { id = "wood", name = "Wood", kind = "material", weight = 1.0, fuel = 1.0 },
  { id = "deadwood", name = "Dry deadwood", kind = "material", weight = 0.8, fuel = 0.8 },
  { id = "hazel_rods", name = "Hazel rods", kind = "material", weight = 0.15, fuel = 0.15 },
  { id = "palm_fibre", name = "Palm fibre", kind = "material", weight = 0.05 },
  { id = "kapok_fibre", name = "Kapok fibre", kind = "material", weight = 0.01 },
  { id = "kapok_bark", name = "Kapok bark", kind = "herb", weight = 0.05, brew = "bark_tea" },
  -- From shrubs and ground cover
  { id = "saltbush_leaves", name = "Saltbush leaves", kind = "food", weight = 0.01, kcal = 2, salty = true,
    spoils = "fresh" },
  { id = "cactus_fruit", name = "Cactus fruit", kind = "food", weight = 0.06, kcal = 25, water = 0.04, spoils = "fresh" },
  { id = "grass_seed", name = "Grass seed", kind = "food", weight = 0.01, kcal = 3, spoils = "dried" },
  { id = "dry_grass", name = "Dry grass", kind = "material", weight = 0.05, fuel = 0.05 },
  { id = "fiddleheads", name = "Fiddleheads", kind = "food", weight = 0.01, kcal = 3, raw = 0.4,
    cooked = "cooked_fiddleheads", spoils = "fresh" },
  { id = "cooked_fiddleheads", name = "Cooked fiddleheads", kind = "food", weight = 0.01, kcal = 3,
    spoils = "cooked" },
  { id = "healing_herbs", name = "Healing herbs", kind = "herb", weight = 0.01, cures = { sick = 0.1 },
    brew = "herbal_tea", spoils = "fresh" },
  { id = "reed_roots", name = "Reed roots", kind = "food", weight = 0.05, kcal = 30, raw = 0.15,
    cooked = "roasted_reed_roots", spoils = "fresh" },
  { id = "roasted_reed_roots", name = "Roasted reed roots", kind = "food", weight = 0.05, kcal = 35,
    spoils = "cooked" },
  { id = "reed_fibre", name = "Reed fibre", kind = "material", weight = 0.1, fuel = 0.1 }, -- dry bundles burn
  -- Fungi
  { id = "chanterelles", name = "Chanterelles", kind = "food", weight = 0.02, kcal = 8, raw = 0.1,
    cooked = "fried_mushrooms", spoils = "fresh" },
  { id = "porcini", name = "Porcini", kind = "food", weight = 0.06, kcal = 15, raw = 0.1, cooked = "fried_mushrooms",
    spoils = "fresh" },
  { id = "meadow_mushrooms", name = "Meadow mushrooms", kind = "food", weight = 0.02, kcal = 6, raw = 0.1,
    cooked = "fried_mushrooms", spoils = "fresh" },
  { id = "fried_mushrooms", name = "Fried mushrooms", kind = "food", weight = 0.03, kcal = 12, spoils = "cooked" },
  { id = "fly_agaric", name = "Fly agaric", kind = "food", weight = 0.05, kcal = 6, poison = 0.6, spoils = "fresh" },
  { id = "tinder_fungus", name = "Tinder fungus", kind = "material", weight = 0.05, fuel = 0.05 },
  -- From creatures
  { id = "raw_meat", name = "Raw meat", kind = "food", weight = 0.25, kcal = 400, raw = 0.3, cooked = "cooked_meat",
    spoils = "fresh" },
  { id = "cooked_meat", name = "Cooked meat", kind = "food", weight = 0.2, kcal = 450, spoils = "cooked" },
  { id = "hide", name = "Hide", kind = "material", weight = 2.0 },
  { id = "feathers", name = "Feathers", kind = "material", weight = 0.01 },
  { id = "storm_feather", name = "Storm feather", kind = "material", weight = 0.02 },
  -- Water, by cup (half a litre)
  { id = "river_water", name = "River water", kind = "water", weight = 0.5, water = 0.5, dirty = 0.1,
    boiled = "boiled_water" },
  { id = "lake_water", name = "Lake water", kind = "water", weight = 0.5, water = 0.5, dirty = 0.2,
    boiled = "boiled_water" },
  { id = "pool_water", name = "Pool water", kind = "water", weight = 0.5, water = 0.5, dirty = 0.4,
    boiled = "boiled_water" },
  { id = "swamp_water", name = "Swamp water", kind = "water", weight = 0.5, water = 0.5, dirty = 0.8,
    boiled = "boiled_water" },
  { id = "sea_water", name = "Sea water", kind = "water", weight = 0.5, water = 0.5, salty = true },
  { id = "boiled_water", name = "Boiled water", kind = "water", weight = 0.5, water = 0.5 },
  { id = "herbal_tea", name = "Herbal tea", kind = "water", weight = 0.5, water = 0.45, cures = { sick = 0.5,
    poison = 0.3 } },
  { id = "bark_tea", name = "Bark tea", kind = "water", weight = 0.5, water = 0.45, cures = { sick = 0.3 } },
  { id = "spruce_tea", name = "Spruce tea", kind = "water", weight = 0.5, water = 0.45, kcal = 5,
    cures = { sick = 0.1 } },
}
