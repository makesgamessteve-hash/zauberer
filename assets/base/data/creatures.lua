-- SPDX-License-Identifier: CC-BY-4.0
-- The creatures of the base game (concept 5.12): what the simulation needs
-- of each species. Mods add or replace species by id.
--   length m, mass kg, walk and run m/s, flies with a cruising height (m),
--   diet, pack, senses in metres (sight by day; hearing; smell on the wind),
--   health, aggression and fears 0..1, resistances per harm 0..1, and for
--   magical creatures their mana's colour and the motes they leave.
-- Ecology (concept 5.13):
--   density  individuals per km^2 where the habitat suits it best
--   habitat  how well each class of land suits it, 0..1 (forest, open,
--            wetland, dry, cold)
--   growth   per game year (48 days) at low numbers
--   group    { least, most } in a herd, pack or flock (omitted: alone)
--   eats     prey species and how hard it hunts each (per year)
return {
  {
    id = "wolf", name = "Grey wolf", length = 1.4, mass = 40, walk = 1.6, run = 10,
    diet = "carnivore", pack = true, sight = 90, hearing = 150, smell = 300,
    health = 60, aggression = 0.8, fear_of_fire = 1.0, fear_of_player = 0.15,
    resist = { cold = 0.6 }, color = { 0.46, 0.45, 0.43 },
    density = 1.5, habitat = { forest = 1, open = 0.8, cold = 0.5 }, growth = 0.4, group = { 3, 5 },
    eats = { deer = 0.3, hare = 0.3, boar = 0.1 },
  },
  {
    id = "deer", name = "Red deer", length = 1.9, mass = 120, walk = 1.4, run = 12,
    diet = "herbivore", pack = true, sight = 110, hearing = 180, smell = 150,
    health = 70, aggression = 0, fear_of_fire = 1.0, fear_of_player = 0.9,
    color = { 0.55, 0.36, 0.22 },
    density = 15, habitat = { forest = 1, open = 0.7, wetland = 0.3 }, growth = 0.6, group = { 3, 6 },
  },
  {
    id = "hare", name = "Brown hare", length = 0.6, mass = 4, walk = 1.2, run = 14,
    diet = "herbivore", sight = 80, hearing = 160, smell = 80,
    health = 12, aggression = 0, fear_of_fire = 1.0, fear_of_player = 1.0,
    color = { 0.6, 0.48, 0.32 },
    density = 40, habitat = { open = 1, forest = 0.5, dry = 0.3, cold = 0.4 }, growth = 2.0,
  },
  {
    id = "fox", name = "Red fox", length = 0.9, mass = 7, walk = 1.3, run = 11,
    diet = "omnivore", sight = 70, hearing = 200, smell = 250,
    health = 18, aggression = 0.1, fear_of_fire = 1.0, fear_of_player = 0.7,
    color = { 0.78, 0.4, 0.16 },
    density = 3, habitat = { forest = 0.8, open = 1, wetland = 0.4, dry = 0.3 }, growth = 0.8,
    eats = { hare = 0.5 },
  },
  {
    id = "boar", name = "Wild boar", length = 1.5, mass = 90, walk = 1.2, run = 9,
    diet = "omnivore", pack = true, sight = 40, hearing = 120, smell = 250,
    health = 80, aggression = 0.4, fear_of_fire = 0.9, fear_of_player = 0.4,
    resist = { impact = 0.2 }, color = { 0.28, 0.24, 0.2 },
    density = 8, habitat = { forest = 1, wetland = 0.6 }, growth = 0.8, group = { 2, 5 },
  },
  {
    id = "crane", name = "Common crane", length = 1.1, mass = 5, walk = 1.0, run = 12,
    flies = true, cruise = 15, diet = "omnivore", pack = true, sight = 120, hearing = 120, smell = 0,
    health = 20, aggression = 0, fear_of_fire = 0.8, fear_of_player = 0.8,
    color = { 0.7, 0.72, 0.74 },
    density = 10, habitat = { wetland = 1, open = 0.3 }, growth = 0.5, group = { 2, 4 },
  },
  {
    id = "storm_bird", name = "Storm bird", length = 1.3, mass = 9, walk = 7, run = 20,
    flies = true, cruise = 25, diet = "carnivore", sight = 160, hearing = 120, smell = 0,
    health = 45, aggression = 0.2, fear_of_fire = 0.4, fear_of_player = 0.5,
    resist = { impact = 0.2, smoke = 0.5 }, magical = true, mana = "storm", motes = 40,
    color = { 0.85, 0.82, 0.35 },
    density = 0.5, habitat = { open = 0.6, cold = 0.4 }, growth = 0.3, eats = { hare = 0.1 },
  },
}
