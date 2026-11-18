-- SPDX-License-Identifier: MIT
-- Test fixture data: a new material, a patch of a base one, a broken entry.
return {
  { id = "quicksilver", name = "Quicksilver", phase = "liquid", density = 13534, specific_heat = 140,
    conductivity = 8.3, freezes = { at = -38.8, to = "quicksilver_ice", latent = 11400 },
    boils = { at = 356.7, to = "steam", latent = 295000 }, friction = 0.1,
    mana = { conductivity = 0.9, void = 0.6, water = 0.4 }, color = { 0.75, 0.76, 0.78 } },
  { id = "quicksilver_ice", name = "Frozen quicksilver", phase = "solid", density = 14184, specific_heat = 140,
    conductivity = 30, melts = { at = -38.8, to = "quicksilver", latent = 11400 }, hardness = 1,
    color = { 0.70, 0.71, 0.74 } },
  { patch = "base:sand", hardness = 6.5, mana = { conductivity = 0.15, earth = 0.5, light = 0.4 } },
  { id = "phlogiston", name = "Phlogiston", phase = "plasma", density = 1, specific_heat = 1, conductivity = 1,
    color = { 1, 0.5, 0 } },
}
