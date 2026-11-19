-- SPDX-License-Identifier: CC-BY-4.0
-- The authored story (concept 1.4-1.7): the Great Weaves, the Circle of the
-- Loom, Azhdar's lieutenants, the revelations, the beats of Acts II and III
-- (after the prologue in story.prologue) and the endings. Beats declare
-- what site they need; the narrative director finds one in the generated
-- world and reserves it.
return {
  weaves = {
    { id = "wandering", name = "the Wandering Weave", colour = "storm", holder = "farrokh" },
    { id = "ash", name = "the Ash Weave", colour = "fire", holder = "azhdar" },
    { id = "tidal", name = "the Tidal Weave", colour = "water" },
    { id = "verdant", name = "the Verdant Weave", colour = "life" },
    { id = "deep", name = "the Deep Weave", colour = "earth" },
    { id = "dawn", name = "the Dawn Weave", colour = "light" },
    { id = "hollow", name = "the Hollow Weave", colour = "void" },
  },
  -- The other five of the Circle: where their remnants are found and what
  -- they give (a teacher teaches a discipline, every one reveals a part of
  -- the past, and holds a Weave until it is secured or taken).
  circle = {
    { id = "herbalist", role = "the old herbalist", site = { kind = "settlement", class = { "swamp", "forest" } },
      remnant = "teacher", discipline = "alchemy", weave = "verdant" },
    { id = "hermit", role = "the hermit of the heights", site = { kind = "tower", class = { "mountain" } },
      remnant = "teacher", discipline = "summoning", weave = "deep" },
    { id = "navigator", role = "the navigator's widow", site = { kind = "settlement", coast = true },
      remnant = "student", discipline = "ritual", weave = "tidal" },
    { id = "astronomer", role = "the astronomer", site = { kind = "grave" }, remnant = "grave",
      discipline = "naturalism", weave = "dawn" },
    { id = "silent", role = "the silent one", site = { kind = "elder ruin" }, remnant = "tower",
      discipline = "spellcasting", weave = "hollow" },
  },
  revelations = {
    "Azhdar was Farrokh's closest friend, the two youngest of the Circle.",
    "The ley network is dying, as it has been dying since the Age of Mana.",
    "Azhdar believes only a reweaving can save magic: all seven Weaves bound to one will, his own.",
    "He would burn the world's people to save the world's magic.",
    "Farrokh believed the Weaves had to stay apart: one master of all mana would be worse than a slow fading.",
  },
  lieutenants = {
    { id = "cinder", name = "the Cinder Widow", personality = "mad", colours = { "fire", "decay" } },
    { id = "smoke", name = "Smoke-of-Seven-Towns", personality = "territorial", colours = { "fire", "storm" } },
    { id = "silt", name = "the Silt King", personality = "greedy", colours = { "earth", "void" } },
  },
  -- The beats after the prologue. requires: beats done before; trigger:
  -- what completes it (story.narrative checks it).
  beats = {
    { id = "trail", act = 2, title = "The trail of ash", requires = {}, trigger = "visit_ruin",
      site = { kind = "burnt" } },
    { id = "circle", act = 2, title = "The Circle's legacy", requires = { "trail" }, trigger = "remnants",
      need = 3 },
    { id = "rivals", act = 2, title = "Rival mages", requires = { "trail" }, trigger = "rival_met" },
    { id = "truth", act = 2, title = "Revelations", requires = { "circle" }, trigger = "revelations", need = 4 },
    { id = "divination", act = 3, title = "Where the citadel drifts", requires = { "truth" },
      trigger = "divined", weaves = 3 },
    { id = "storm", act = 3, title = "The ley storm", requires = { "divination" }, trigger = "ready" },
    { id = "duel", act = 3, title = "The Ash Weaver", requires = { "storm" }, trigger = "azhdar_defeated" },
    { id = "ending", act = 3, title = "The reweaving", requires = { "duel" }, trigger = "ending_chosen" },
  },
  endings = {
    unravel = { name = "Unravel", ley = -0.08, text = "Azhdar is destroyed and every Weave unravelled. Mana fades year by year; people live freely." },
    bind = { name = "Bind", ley = 0, text = "Azhdar sleeps bound in his own Weave. The Weaves stay scattered; a prisoner whispers." },
    reweave = { name = "Reweave", ley = 0.5, requires = { ritual = 3, void = true },
      text = "The ley lines are rewoven and shared. The heirs are their guardians; the rival mages must be dealt with." },
    succession = { name = "Succession", ley = 0.2, tyranny = true,
      text = "The heirs take Azhdar's place: the new masters of all mana. The world remembers." },
  },
}
