-- SPDX-License-Identifier: MIT
-- The merged material registry from Lua, with the alchemy fixture mod installed.
-- spec: materials: Materials from Lua and mods
scenario { name = "materials" }
local keys = registry.keys("materials")
expect(#keys >= 39, "base and mod materials")
expect_eq(keys[1], "base:air")
local base_count = 0
for i, key in ipairs(keys) do
  if key:sub(1, 5) == "base:" then
    base_count = i
  end
end
expect_eq(keys[base_count + 1], "alchemy:quicksilver", "mod entries follow the base ones")
-- The raw entry as the packages left it.
local sand = registry.get("materials", "sand")
expect_eq(sand.key, "base:sand")
expect_eq(sand.package, "base")
expect_eq(sand.changed, "alchemy")
expect_eq(sand.hardness, 6.5)
expect_eq(sand.density, 1600)
expect_eq(registry.get("materials", "phlogiston"), nil, "the broken entry is left out")
expect_eq(registry.get("materials", "nothing"), nil)
expect_error(function() registry.keys("spells") end, "unknown registry")
-- What the simulations use.
local water = sim.material("water")
expect_eq(water.phase, "liquid")
expect_eq(water.freezes.to, "base:ice")
expect_eq(water.boils.latent, 2257000)
expect_eq(water.mana.water, 1)
expect_eq(#water.color, 3)
local wood = sim.material("base:wood")
expect_near(wood.burns.products["base:charcoal"], 0.15, 1e-6)
expect_eq(wood.burns.ignition, 300)
expect_eq(sim.material("rock").burns, nil)
local hg = sim.material("quicksilver")
expect_eq(hg.key, "alchemy:quicksilver")
expect_near(hg.density, 13534, 1e-3)
expect_eq(sim.material("nothing"), nil)
expect_eq(sim.material_at("quicksilver", -50), "alchemy:quicksilver_ice")
expect_eq(sim.material_at("ice", 150), "base:steam")
expect_eq(sim.material_at("lava", 20), "base:basalt")
expect_error(function() sim.material_at("nothing", 20) end, "no material nothing")
