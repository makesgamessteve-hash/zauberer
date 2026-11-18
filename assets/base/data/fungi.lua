-- SPDX-License-Identifier: CC-BY-4.0
-- The fungi of the base game (concept 5.12): mushrooms fruit in damp places
-- after rain, most in autumn. Each grows where its habitat is (forest, open,
-- or on dead trees) in its season (0 .. 4 from the local spring), up to
-- density mushrooms in a 32 m patch where it suits best.
return {
  { id = "chanterelle", name = "Chanterelle", item = "chanterelles", habitat = "forest", season = { 1.4, 2.8 },
    density = 6 },
  { id = "porcini", name = "Porcini", item = "porcini", habitat = "forest", season = { 1.6, 2.9 }, density = 4 },
  { id = "fly_agaric", name = "Fly agaric", item = "fly_agaric", habitat = "forest", season = { 1.8, 3.0 },
    density = 4 },
  { id = "meadow_mushroom", name = "Meadow mushroom", item = "meadow_mushrooms", habitat = "open",
    season = { 1.0, 2.7 }, density = 5 },
  { id = "tinder_bracket", name = "Tinder bracket", item = "tinder_fungus", habitat = "deadwood", season = { 0, 4 },
    density = 3 },
}
