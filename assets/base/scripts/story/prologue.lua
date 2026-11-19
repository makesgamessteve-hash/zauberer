-- SPDX-License-Identifier: MIT
-- Vertical Slice B: the prologue in Anarvan and Act I to leaving the valley
-- (concept 1.3, 1.4), as a narrative director of beats:
--  village   chores (water at the qanat, the goats, pomegranates, knots at
--            the loom) and a word with Farrokh, who lights a lamp "by a
--            weaver's sleight of hand";
--  arrival   dusk: Azhdar, the Ash Weaver, descends on his dark carpet and
--            demands the Weave;
--  handover  Farrokh presses a plain rug and a bag into the heir's arms and
--            opens the hatch under the loom: "Hide.";
--  hiding    in the warded cellar, the hatch shut; through the cracks:
--            fire, screams, Farrokh's magic, Azhdar burning carpet after
--            carpet; the village burns;
--  ashes     morning: the village is ash; the heir may bury Farrokh and
--            light a lamp (the first journal entry);
--  sanctum   the cellar is larger than a cellar should be: the letter, the
--            Primer (Kindle, Light, Lift) and the endless bag;
--  carpet    the heir sleeps on the carpet and in a dream it chooses them;
--  flying    a guided first flight over the orchards and the qanat line;
--  survival  foraging and boiling water: the stores are burnt;
--  leaving   the heir follows the scorched trail out of the valley.
-- With several heirs each is handed the rug and hides in the cellar; the
-- director keeps one story for all of them (the host's).
local spellbook = require("magic.spellbook")
local pack = require("player.pack")

local prologue = {}

prologue.BEATS = { "village", "arrival", "handover", "hiding", "ashes", "sanctum", "carpet", "flying", "survival",
  "leaving", "done" }
prologue.REACH = 4          -- metres to a chore's place
prologue.ARRIVAL = 20       -- seconds of Azhdar's demands
prologue.HIDING = 90        -- seconds in the cellar
prologue.RING = 30          -- metres within which a flight ring is passed
prologue.LEAVE = 3000       -- metres from Anarvan that is out of the valley
prologue.PRIMER = {         -- the first weaves of Farrokh's Primer
  { verb = "create", colors = { "fire" }, form = "tap", name = "Kindle" },
  { verb = "create", colors = { "light" }, form = "tap", name = "Light" },
  { verb = "move", colors = { "earth" }, form = "line", name = "Lift" },
}
prologue.FRAGMENTS = { "Fire roars over the roofs.", "Screams in the street.",
  "Farrokh's magic flares, blue and bright, and is gone.", "Azhdar burns carpet after carpet: \"Where is it?\"",
  "\"It is near. I feel it near.\" Then hoofbeats, and silence." }

local function said(S, text)
  S.lines[#S.lines + 1] = text
  if #S.lines > 6 then table.remove(S.lines, 1) end
end

-- Begins the prologue. places: { qanat, pen, farrokh (buildings), orchard
-- (a position), square (a position), sanctum (the dungeon) }.
function prologue.new(places)
  return { beat = "village", t = 0, chores = {}, talked = false, lines = {}, bonded = false, ring = 1,
    places = places, journal = {}, fragment = 0 }
end

function prologue.snapshot(S)
  return { beat = S.beat, chores = S.chores, talked = S.talked, bonded = S.bonded, ring = S.ring,
    journal = S.journal, out = S.out, primer = S.primer, bag = S.bag }
end

local function go(S, beat)
  S.beat, S.t = beat, 0
end

local function near(a, b, r)
  return a and b and world.distance(a, b) <= r and math.abs((a.alt or 0) - (b.alt or 0)) < 6
end

-- The rings of the first flight: over the orchards, the square and along
-- the qanat line into the desert.
function prologue.rings(S)
  local sq = S.places.square
  local out = {}
  for i, a in ipairs({ math.pi, 0.5 * math.pi, 0, -0.5 * math.pi }) do
    local p = world.move(sq, a, 250 + i * 50, 0, 0)
    p.alt = world.height(p) + 40
    out[i] = p
  end
  return out
end

-- Moves the story on. ctx: { pos (the heir), used (use pressed), slept
-- (slept this tick), dt, now (world seconds), set_time(seconds),
-- talked_to (the name of whom the heir talked with), flying (on the
-- carpet in the air), mage (for the Primer: the caster's book), pack,
-- burn(building) (sets it alight), gathered (anything gathered), boiled
-- (boiled water made) }. Returns what happened, if anything.
function prologue.tick(S, ctx)
  local P = S.places
  S.t = S.t + (ctx.dt or 0)
  local b = S.beat
  if b == "village" then
    if ctx.used then
      if near(ctx.pos, P.qanat_pos, prologue.REACH) and not S.chores.water then
        S.chores.water = true
        pack.add(ctx.pack, "river_water", 2, 0)
        said(S, "You fill the jar at the qanat outlet")
      elseif near(ctx.pos, P.pen_pos, prologue.REACH + 2) and not S.chores.goats then
        S.chores.goats = true
        said(S, "The goats crowd round for their fodder")
      elseif near(ctx.pos, P.orchard, 40) and not S.chores.pomegranates then
        S.chores.pomegranates = true
        pack.add(ctx.pack, "pomegranate", 3, 0)
        said(S, "You pick three pomegranates")
      elseif near(ctx.pos, P.loom_pos, prologue.REACH) and not S.chores.loom then
        S.chores.loom = true
        said(S, "You tie knots at the loom: the pattern grows under your fingers")
      end
    end
    if ctx.talked_to == "Farrokh" and not S.talked then
      S.talked = true
      said(S, "Farrokh lights the oil lamp with a word - \"a weaver's sleight of hand\", he smiles")
    end
    local done = 0
    for _ in pairs(S.chores) do done = done + 1 end
    if done >= 3 and S.talked then
      ctx.set_time(math.floor(ctx.now / 86400) * 86400 + 18.5 * 3600)
      go(S, "arrival")
      said(S, "Dusk. A dark carpet comes down over the square.")
      return "arrival"
    end
  elseif b == "arrival" then
    S.azhdar = world.move(P.square, 0, 0, 0, 0)
    S.azhdar.alt = world.height(S.azhdar) + math.max(2, 30 - S.t * 2)
    if S.t > 4 and not S.demanded then
      S.demanded = true
      said(S, "Azhdar: \"Give me the Weave!\" No one understands him.")
    end
    if S.t >= prologue.ARRIVAL then
      go(S, "handover")
      P.sanctum.closed = false
      pack.add(ctx.pack, "rug", 1, 0)
      said(S, "Farrokh presses a plain rug and a small bag into your arms: "
        .. "\"Hide. Don't come out, whatever you hear.\"")
      return "handover"
    end
  elseif b == "handover" then
    if ctx.pos.alt < P.sanctum.origin.alt + 1.5 then -- down in the cellar
      P.sanctum.closed = true
      go(S, "hiding")
      said(S, "The hatch closes over you. The cellar is warded.")
      for _, house in ipairs(P.houses or {}) do ctx.burn(house) end
      return "hiding"
    end
  elseif b == "hiding" then
    local k = math.min(#prologue.FRAGMENTS, math.floor(S.t / (prologue.HIDING / #prologue.FRAGMENTS)) + 1)
    if k > S.fragment then
      S.fragment = k
      said(S, prologue.FRAGMENTS[k])
    end
    if S.t >= prologue.HIDING then
      ctx.set_time(math.floor(ctx.now / 86400 + 1) * 86400 + 6 * 3600)
      P.sanctum.closed = false
      go(S, "ashes")
      if ctx.ruin then ctx.ruin() end
      said(S, "Morning. You climb out. Anarvan is ash, and no one is left.")
      return "ashes"
    end
  elseif b == "ashes" then
    if ctx.used and near(ctx.pos, P.farrokh_pos, 8) and not S.journal.burial then
      S.journal.burial = "You buried your father as the village does and lit a lamp for him."
      said(S, "You bury Farrokh and light a lamp. (Journal)")
    end
    if ctx.pos.alt > P.sanctum.origin.alt + 3 then S.out = true end -- (climbed out into the morning)
    if S.out and ctx.pos.alt < P.sanctum.origin.alt + 1.5 then
      go(S, "sanctum")
      said(S, "The cellar is larger than a cellar should be.")
      return "sanctum"
    end
  elseif b == "sanctum" then
    if ctx.found == "Farrokh's letter" then
      S.journal.letter = "My child, if you read this, I could not keep you from what I was..."
      said(S, "A letter in Farrokh's hand. It breaks off. (Journal)")
    elseif ctx.found == "the Primer" and ctx.book then
      for _, w in ipairs(prologue.PRIMER) do
        spellbook.learn(ctx.book, { verb = w.verb, colors = w.colors, form = w.form, power = 1 }, "book")
      end
      S.primer = true
      said(S, "Farrokh's Primer: Kindle, Light and Lift")
    elseif ctx.found == "the endless bag" then
      S.bag = true
      said(S, "The endless bag: it opens into a fold larger than a house")
    end
    if S.primer and S.bag and S.journal.letter then
      go(S, "carpet")
      said(S, "The rug is warm. Sleep on it.")
      return "carpet"
    end
  elseif b == "carpet" then
    if ctx.slept then
      S.bonded = true
      go(S, "flying")
      said(S, "In a dream the carpet chooses you. It stirs under your hands.")
      return "flying"
    end
  elseif b == "flying" then
    local rings = prologue.rings(S)
    local r = rings[S.ring]
    if ctx.flying and r and world.distance(ctx.pos, r) < prologue.RING
      and math.abs(ctx.pos.alt - r.alt) < prologue.RING then
      S.ring = S.ring + 1
      said(S, S.ring > #rings and "You can fly." or ("Ring " .. (S.ring - 1) .. " of " .. #rings))
    end
    if S.ring > #rings then
      go(S, "survival")
      said(S, "The stores are burnt. Forage, and boil water.")
      return "survival"
    end
  elseif b == "survival" then
    S.gathered = S.gathered or ctx.gathered
    S.boiled = S.boiled or ctx.boiled
    if S.gathered and S.boiled then
      go(S, "leaving")
      said(S, "Follow the scorched earth Azhdar left, out of the valley.")
      return "leaving"
    end
  elseif b == "leaving" then
    if world.distance(ctx.pos, P.square) > prologue.LEAVE then
      go(S, "done")
      said(S, "You leave the valley behind.")
      return "done"
    end
  end
  return nil
end

-- Whether the carpet will fly yet (only once it has chosen the heir).
function prologue.can_fly(S)
  return S.bonded or S.beat == "done"
end

-- What the heir is to do now.
local GOALS = { village = "Do your chores and talk to Farrokh", arrival = "...", handover = "Hide in the cellar",
  hiding = "Stay hidden", ashes = "Go back down into the cellar", sanctum = "Search the cellar",
  carpet = "Sleep on the rug", flying = "Fly through the rings", survival = "Forage and boil water",
  leaving = "Leave the valley", done = "The valley lies behind you" }

function prologue.hud_lines(S)
  local out = { "Story: " .. GOALS[S.beat] }
  for _, l in ipairs(S.lines) do out[#out + 1] = l end
  return out
end

return prologue
