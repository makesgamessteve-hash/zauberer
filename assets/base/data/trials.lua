-- SPDX-License-Identifier: CC-BY-4.0
-- The trials of the disciplines (concept 3.4): what must be done, besides
-- the practice and study, to pass into each tier (2 Apprentice, 3
-- Journeyman, 4 Adept, 5 Master). event: the kind of practice counted;
-- distinct: how many different ones; count: how many in all.
return {
  spellcasting = { { event = "cast", distinct = 3 }, { event = "cast", distinct = 6 }, { event = "cast", count = 200 },
    { event = "cast", distinct = 12 } },
  ritual = { { event = "ritual", count = 1 }, { event = "ritual", distinct = 3 }, { event = "ritual", count = 10 },
    { event = "ritual", count = 25 } },
  alchemy = { { event = "brew", distinct = 2 }, { event = "brew", distinct = 4 }, { event = "brew", count = 20 },
    { event = "brew", count = 50 } },
  artificing = { { event = "enchant", count = 1 }, { event = "enchant", distinct = 3 },
    { event = "enchant", count = 8 }, { event = "enchant", distinct = 6 } },
  summoning = { { event = "summon", count = 1 }, { event = "bond", count = 1 }, { event = "summon", distinct = 2 },
    { event = "summon", count = 30 } },
  naturalism = { { event = "learn", count = 10 }, { event = "learn", count = 40 }, { event = "learn", count = 100 },
    { event = "learn", count = 200 } },
}
