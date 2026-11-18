-- SPDX-License-Identifier: CC-BY-4.0
-- The first creatures of the base game (concept 5.12): what the simulation
-- needs of each species. Mods add or replace species by id.
--   length m, mass kg, walk and run m/s, flies with a cruising height (m),
--   diet, pack, senses in metres (sight by day; hearing; smell on the wind),
--   health, aggression and fears 0..1, resistances per harm 0..1, and for
--   magical creatures their mana's colour and the motes they leave.
return {
  {
    id = "wolf", name = "Grey wolf", length = 1.4, mass = 40, walk = 1.6, run = 10,
    diet = "carnivore", pack = true, sight = 90, hearing = 150, smell = 300,
    health = 60, aggression = 0.8, fear_of_fire = 1.0, fear_of_player = 0.15,
    resist = { cold = 0.6 }, color = { 0.46, 0.45, 0.43 },
  },
  {
    id = "deer", name = "Red deer", length = 1.9, mass = 120, walk = 1.4, run = 12,
    diet = "herbivore", pack = true, sight = 110, hearing = 180, smell = 150,
    health = 70, aggression = 0, fear_of_fire = 1.0, fear_of_player = 0.9,
    color = { 0.55, 0.36, 0.22 },
  },
  {
    id = "storm_bird", name = "Storm bird", length = 1.3, mass = 9, walk = 7, run = 20,
    flies = true, cruise = 25, diet = "carnivore", sight = 160, hearing = 120, smell = 0,
    health = 45, aggression = 0.2, fear_of_fire = 0.4, fear_of_player = 0.5,
    resist = { impact = 0.2, smoke = 0.5 }, magical = true, mana = "storm", motes = 40,
    color = { 0.85, 0.82, 0.35 },
  },
}
