-- SPDX-License-Identifier: CC-BY-4.0
-- The items of the base game: what plants, creatures and the land give, and
-- what is made of them. Mods add or replace items by id.
--   name, kind ("food", "herb", "material", "water"), weight in kg per item
return {
  -- From trees
  { id = "acorns", name = "Acorns", kind = "food", weight = 0.004 },
  { id = "beechnuts", name = "Beechnuts", kind = "food", weight = 0.002 },
  { id = "hazelnuts", name = "Hazelnuts", kind = "food", weight = 0.003 },
  { id = "pine_nuts", name = "Pine nuts", kind = "food", weight = 0.001 },
  { id = "coconut", name = "Coconut", kind = "food", weight = 1.2 },
  { id = "dates", name = "Dates", kind = "food", weight = 0.008 },
  { id = "spruce_tips", name = "Spruce tips", kind = "herb", weight = 0.002 },
  { id = "birch_sap", name = "Birch sap", kind = "water", weight = 0.25 },
  { id = "oak_bark", name = "Oak bark", kind = "herb", weight = 0.05 },
  { id = "birch_bark", name = "Birch bark", kind = "herb", weight = 0.05 },
  { id = "resin", name = "Resin", kind = "material", weight = 0.02 },
  { id = "acacia_gum", name = "Acacia gum", kind = "material", weight = 0.02 },
  { id = "wood", name = "Wood", kind = "material", weight = 1.0 },
  { id = "deadwood", name = "Dry deadwood", kind = "material", weight = 0.8 },
  { id = "hazel_rods", name = "Hazel rods", kind = "material", weight = 0.15 },
  { id = "palm_fibre", name = "Palm fibre", kind = "material", weight = 0.05 },
  { id = "kapok_fibre", name = "Kapok fibre", kind = "material", weight = 0.01 },
  -- From shrubs and ground cover
  { id = "saltbush_leaves", name = "Saltbush leaves", kind = "food", weight = 0.01 },
  { id = "cactus_fruit", name = "Cactus fruit", kind = "food", weight = 0.06 },
  { id = "grass_seed", name = "Grass seed", kind = "food", weight = 0.01 },
  { id = "dry_grass", name = "Dry grass", kind = "material", weight = 0.05 },
  { id = "fiddleheads", name = "Fiddleheads", kind = "food", weight = 0.01 },
  { id = "healing_herbs", name = "Healing herbs", kind = "herb", weight = 0.01 },
  { id = "reed_roots", name = "Reed roots", kind = "food", weight = 0.05 },
  { id = "reed_fibre", name = "Reed fibre", kind = "material", weight = 0.1 },
  -- Fungi
  { id = "chanterelles", name = "Chanterelles", kind = "food", weight = 0.02 },
  { id = "porcini", name = "Porcini", kind = "food", weight = 0.06 },
  { id = "fly_agaric", name = "Fly agaric", kind = "food", weight = 0.05 },
  { id = "meadow_mushrooms", name = "Meadow mushrooms", kind = "food", weight = 0.02 },
  { id = "tinder_fungus", name = "Tinder fungus", kind = "material", weight = 0.05 },
}
