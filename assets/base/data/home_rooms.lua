-- SPDX-License-Identifier: CC-BY-4.0
-- The rooms of the pocket home inside the tent (concept 2.3), in the order
-- the mage walks through them. Mods add rooms.
--   open   open from the start (else opened by folding more space)
--   does   what the room offers: "cook" (the hearth, always burning),
--          "sleep" (safe, warm, mending), "read" (the Codex), "vault" (the
--          bag's shelves), "brew", "craft", "repair", "ritual", "grow", "keep"
--   text   what the HUD says of it
return {
  { id = "hearth", name = "Hearth room", open = true, does = { "cook" },
    text = "the fire never dies here: K to cook, Y to eat, U to drink" },
  { id = "bedchamber", name = "Bedchamber", open = true, does = { "sleep" },
    text = "L to sleep in safety, and mend" },
  { id = "study", name = "Study", open = true, does = { "read" },
    text = "Farrokh's books and the map table: J for the Codex" },
  { id = "vault", name = "Vault", open = true, does = { "vault" },
    text = "the bag's shelves: I shelves all you carry, O takes what you need" },
  { id = "pantry", name = "Pantry and cold store", open = false, does = { "keep" },
    text = "a shelf where time all but stops" },
  { id = "laboratory", name = "Alchemy laboratory", open = false, does = { "brew" },
    text = "furnace, alembic, mortar and glassware" },
  { id = "workshop", name = "Workshop", open = false, does = { "craft", "repair" },
    text = "workbench, loom and enchanting table" },
  { id = "garden", name = "Garden courtyard", open = false, does = { "grow" },
    text = "four beds, an enchanted sun and a fountain" },
  { id = "ritual", name = "Ritual chamber", open = false, does = { "ritual" },
    text = "an inlaid circle over the tent's ley anchor" },
  { id = "menagerie", name = "Menagerie", open = false, does = {},
    text = "habitats for familiars and bound creatures" },
}
