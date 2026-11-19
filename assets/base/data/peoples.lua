-- SPDX-License-Identifier: MIT
-- What peopling (concept 10.5, 10.7, 10.9, 10.10) draws on: how peoples live
-- off each kind of land, their values and stances on magic, the shapes their
-- faiths take and the kinds of realm they form. All peoples and faiths made
-- from this are fictional.
return {
  -- How a people lives off a region of a class.
  subsistence = {
    desert = "oasis farming", steppe = "herding", tundra = "herding", grassland = "farming", savanna = "herding",
    forest = "farming", rainforest = "gardening", swamp = "fishing", mountain = "terrace farming",
    coast = "fishing",
  },
  values = { "hospitality", "honour", "learning", "piety", "martial prowess", "commerce" },
  -- Attitude to magic, weighted by the ley's strength near the homeland.
  magic = { "revered", "regulated", "feared", "forbidden" },
  calendars = { "solar", "lunar", "lunisolar" },
  festivals = {
    solar = { "the longest day", "the turning of the year", "the spring equinox" },
    lunar = { "the new moon of harvest", "the full moon of the flocks", "the dark moon of mourning" },
    lunisolar = { "the first rains", "the gathering of the dates", "the blessing of the looms" },
  },
  -- Faiths: the core concept, imagery by the homeland, doctrine on magic,
  -- burial custom by the land, and their institutions.
  faith = {
    concepts = { "a pantheon", "a single god", "two principles", "the ancestors", "a philosophy without gods",
      "a colour of mana" },
    imagery = {
      river = { "the river mother", "water" }, desert = { "the star shepherd", "stars" },
      mountain = { "the wrathful mountain", "fire" }, steppe = { "the eternal sky", "sky" },
      forest = { "the green grove", "trees" }, sea = { "the deep father", "the sea" },
      grassland = { "the sower", "harvest" }, ice = { "the white silence", "snow" },
    },
    stances = { "a sacred gift", "a power to be licensed", "an abomination" },
    burial = { desert = "rock tombs", steppe = "sky burial under mounds", river = "water burial",
      forest = "cremation", mountain = "cliff tombs", grassland = "earth graves", sea = "burial at sea",
      ice = "cairns" },
    orders = { "temple priests", "wandering mendicants", "a monastery on the heights", "an inquisition of mages" },
  },
  polities = {
    { type = "nomad confederation", subsistence = "herding" },
    { type = "city-state", max_cells = 6 },
    { type = "kingdom" },
    { type = "empire", min_cells = 45 },
    { type = "theocracy", holy = true },
    { type = "merchant republic", coast = true },
    { type = "mage-ruled domain", nexus = true },
  },
  titles = { kingdom = "king", empire = "emperor", ["city-state"] = "prince", theocracy = "high priest",
    ["merchant republic"] = "doge", ["nomad confederation"] = "khan", ["mage-ruled domain"] = "magus" },
  epithets = {
    good = { "the Just", "the Builder", "the Wise", "the Generous", "the Lawgiver", "the Brave" },
    bad = { "the Cruel", "the Usurper", "the Mad", "the Grasping", "the Faithless", "the Bloody" },
  },
  war_causes = { "land", "succession", "faith", "revenge", "raids", "trade" },
  ruin_causes = { "war", "fire", "abandonment", "flood", "magic" },
}
