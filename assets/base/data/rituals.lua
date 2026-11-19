-- SPDX-License-Identifier: CC-BY-4.0
-- Ritual magic (concept 3.4): slow, large-scale, lasting magic on a region.
--   mana     colour = mana channelled in all (from the land first, then the mage)
--   needs    item = count laid at the circle's points
--   timing   "any", "twilight" (dawn or dusk), "night" or "day"
--   channel  seconds of channelling, still and unhurt
--   effect   "rain" (rain over the region for hours, at rain mm/h), "ward" (creatures
--            keep out of radius metres for hours), "room" (a room of the pocket home
--            unfolds; by the pitched tent)
return {
  { id = "call_rain", name = "Call rain", mana = { water = 40, storm = 20 }, needs = { boiled_water = 2 },
    timing = "twilight", channel = 20, effect = "rain", hours = 6, rain = 6 },
  { id = "ward_camp", name = "Ward the camp", mana = { light = 20, earth = 10 }, needs = { resin = 2 },
    timing = "any", channel = 15, effect = "ward", hours = 12, radius = 40 },
  { id = "unfold_room", name = "Unfold a room", mana = { void = 40 }, needs = { cord = 2 },
    timing = "night", channel = 30, effect = "room" },
}
