-- SPDX-License-Identifier: CC-BY-4.0
-- The weave grammar of the base game (concept 3.3): the eight verbs with their
-- words of the Old Tongue, the colours of the mana wheel with what one mana
-- buys of each, the eight forms of the gestures, the modifiers, and the named
-- weaves a mage can discover. All of it is data for balancing and for mods.
return {
  -- base: the cost of a weave per mana of power (before form, modifiers and place).
  verbs = {
    create = { word = "aen", base = 1.0, meaning = "Add energy or matter of the colour's domain" },
    unmake = { word = "nehl", base = 0.8, meaning = "Remove, negate or absorb it" },
    move = { word = "vash", base = 1.0, meaning = "Apply force or transport along the form" },
    shape = { word = "tor", base = 1.2, meaning = "Transform state or structure" },
    bind = { word = "kel", base = 1.5, meaning = "Make an effect persist or hold something" },
    sense = { word = "ith", base = 0.3, meaning = "Reveal information" },
    ward = { word = "ostra", base = 1.0, meaning = "Protect against the colour's domain" },
    call = { word = "vo", base = 3.0, meaning = "Summon a being of the colour" },
  },
  -- The wheel in order: neighbours are adjacent, opposites four apart.
  -- per_mana: what one mana does in that colour's domain. (Fire: the fire
  -- simulation heats the fuel with the top of the ground, so it takes about
  -- 1 MJ to kindle a square metre of dry grass.)
  colors = {
    { id = "fire", name = "Fire", word = "Atash", glyph = "^", per_mana = { joules = 1000000 } },
    { id = "storm", name = "Storm", word = "Vad", glyph = "%", per_mana = { impulse = 400 } },
    { id = "light", name = "Light", word = "Roshan", glyph = "o", per_mana = { seconds = 20 } },
    { id = "life", name = "Life", word = "Jan", glyph = "+", per_mana = { health = 4 } },
    { id = "water", name = "Water", word = "Ab", glyph = "v", per_mana = { cubic_metres = 0.002 } },
    { id = "earth", name = "Earth", word = "Khak", glyph = "=", per_mana = { tonne_metres = 1 } },
    { id = "void", name = "Void", word = "Khala", glyph = "@", per_mana = { metres = 2 } },
    { id = "decay", name = "Decay", word = "Zaval", glyph = "x", per_mana = { health = 3 } },
  },
  -- factor: cost multiplier; reach: metres from the caster (tap: touch);
  -- radius: metres of the effect at power 10 (it grows with the fourth root of
  -- the power, so stronger weaves are fiercer, not only wider).
  forms = {
    tap = { factor = 0.8, reach = 3, radius = 1.5, meaning = "Self or touch" },
    line = { factor = 1.0, reach = 60, radius = 1, meaning = "Bolt or beam" },
    circle = { factor = 1.5, reach = 40, radius = 4, meaning = "Sphere or area" },
    spiral = { factor = 1.2, reach = 20, radius = 3, meaning = "Vortex or sustained" },
    triangle = { factor = 1.3, reach = 12, radius = 6, meaning = "Cone or burst" },
    square = { factor = 1.6, reach = 25, radius = 5, meaning = "Wall or barrier" },
    zigzag = { factor = 1.4, reach = 40, radius = 1, meaning = "Chain or arc" },
    wave = { factor = 1.3, reach = 30, radius = 3, meaning = "Stream or spray" },
  },
  modifiers = {
    far = { word = "dur", cost = 1.4, reach = 3 },
    wide = { word = "pahn", cost = 1.5, radius = 2 },
    lasting = { word = "mand", cost = 1.6, duration = 4 },
    many = { word = "bas", cost = 2.0, count = 3 },
    delayed = { word = "dir", cost = 1.1, delay = 3 },
    upon_touch = { word = "das", cost = 1.2 },
    returning = { word = "baz", cost = 1.3 },
  },
  -- Named weaves: what the journal calls a combination once discovered, and
  -- whether a teacher or book can reveal it. key: verb, colours, form.
  named = {
    { name = "Kindle", verb = "create", colors = { "fire" }, form = "tap" },
    { name = "Fire Bloom", verb = "create", colors = { "fire" }, form = "circle" },
    { name = "Fireball", verb = "move", colors = { "fire" }, form = "line" },
    { name = "Thunderball", verb = "move", colors = { "fire", "storm" }, form = "line" },
    { name = "Unmake Fire", verb = "unmake", colors = { "fire" }, form = "circle" },
    { name = "Snuff", verb = "unmake", colors = { "fire" }, form = "tap" },
    { name = "Heat Sight", verb = "sense", colors = { "fire" }, form = "circle" },
    { name = "Fire Ward", verb = "ward", colors = { "fire" }, form = "tap" },
    { name = "Gust", verb = "move", colors = { "storm" }, form = "line" },
    { name = "Gale", verb = "move", colors = { "storm" }, form = "triangle" },
    { name = "Spark", verb = "create", colors = { "storm" }, form = "tap" },
    { name = "Read the Wind", verb = "sense", colors = { "storm" }, form = "circle" },
    { name = "Light", verb = "create", colors = { "light" }, form = "tap" },
    { name = "Lantern", verb = "create", colors = { "light" }, form = "circle" },
    { name = "Mend", verb = "create", colors = { "life" }, form = "tap" },
    { name = "Detect Life", verb = "sense", colors = { "life" }, form = "circle" },
    { name = "Wither", verb = "unmake", colors = { "life" }, form = "line" },
    { name = "Healing Rain", verb = "create", colors = { "water", "life" }, form = "circle" },
    { name = "Condense", verb = "create", colors = { "water" }, form = "circle" },
    { name = "Water Jet", verb = "move", colors = { "water" }, form = "line" },
    { name = "Dry", verb = "unmake", colors = { "water" }, form = "circle" },
    { name = "Freeze", verb = "shape", colors = { "water" }, form = "circle" },
    { name = "Ice Bridge", verb = "shape", colors = { "water" }, form = "line" },
    { name = "Dowse", verb = "sense", colors = { "water" }, form = "circle" },
    { name = "Cold Ward", verb = "ward", colors = { "water" }, form = "tap" },
    { name = "Raise Earth", verb = "move", colors = { "earth" }, form = "square" },
    { name = "Dig", verb = "unmake", colors = { "earth" }, form = "circle" },
    { name = "Hurl Stone", verb = "move", colors = { "earth" }, form = "line" },
    { name = "Conjure Stone", verb = "create", colors = { "earth" }, form = "tap" },
    { name = "Stone to Mud", verb = "shape", colors = { "earth", "water" }, form = "circle" },
    { name = "Stone Skin", verb = "ward", colors = { "earth" }, form = "tap" },
    { name = "Blink", verb = "move", colors = { "void" }, form = "line" },
    { name = "Sense Void", verb = "sense", colors = { "void" }, form = "circle" },
    { name = "Rot", verb = "create", colors = { "decay" }, form = "circle" },
    { name = "Purify", verb = "unmake", colors = { "decay" }, form = "tap" },
    { name = "Steam Blast", verb = "move", colors = { "fire", "water" }, form = "triangle" },
  },
}
