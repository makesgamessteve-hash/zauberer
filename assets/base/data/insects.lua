-- SPDX-License-Identifier: CC-BY-4.0
-- The insects of the Codex (concept 7.3): the swarm kinds of
-- life/smalllife.lua, with what each level of an entry tells.
--   observed, studied, understood   lines the page shows from that level
--   danger                          the Codex's warning, from observed
return {
  { id = "bee", name = "Honey bee",
    observed = { "About flowers by day while plants grow." },
    studied = { "Carries pollen from flower to flower." },
    understood = { "Where bees work, plants bear a fifth more fruit." } },
  { id = "butterfly", name = "Butterfly",
    observed = { "Flutters over flowering meadows on bright days." },
    studied = { "Lays its eggs on the plants its young will eat." },
    understood = { "Many butterflies mean open, flowering land nearby." } },
  { id = "mosquito", name = "Mosquito", danger = "it bites, and its bites can make you sick",
    observed = { "Dances over wet ground at dusk in warm weather." },
    studied = { "Breeds in standing water; only the females bite." },
    understood = { "Keep away from still water at dusk, or keep moving." } },
  { id = "firefly", name = "Firefly",
    observed = { "Glows over open land on summer nights." },
    studied = { "Its light calls a mate." },
    understood = { "Fireflies stay where the air is clean and the grass long." } },
}
