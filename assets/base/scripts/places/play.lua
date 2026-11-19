-- SPDX-License-Identifier: MIT
-- Places and people in the game: the world is peopled in slices as play
-- begins (from the save's seed, Anarvan where the carpet set out); near
-- settlements come to be around every heir present; the walker stands on
-- floors and is stopped by walls; "use" opens doors and containers and
-- takes what is in them (books are read into the lore journal); fire burns
-- timber buildings. Drawn with places.view.
local places = require("places.places")
local view = require("places.view")
local history = require("people.history")
local lore = require("people.lore")
local pack = require("player.pack")
local items = require("life.items")
local folk = require("people.folk")

local play = {}

-- Sets up places for a game. saved: what the save kept (deltas, journal);
-- seed: the world's; anchor: where the story begins; env: the walker's
-- ground (gets structure and blocked).
function play.setup(saved, seed, anchor, env, variant)
  saved = type(saved) == "table" and saved or {}
  local s = { places = places.new(saved.places), journal = lore.new(saved.journal), said = nil,
    folk = folk.new(saved.folk), variant = saved.variant or variant or "settled" }
  places.begin(s.places, seed, anchor, s.variant, type(saved.history) == "table" and history.load(saved.history) or nil)
  if env then
    env.structure = function(pos, alt)
      if not s.places.ready then return nil end
      return places.floor(s.places, pos, alt)
    end
    env.blocked = function(a, b, alt) return s.places.ready and places.blocked(s.places, a, b, alt) or false end
  end
  return s
end

function play.snapshot(s)
  local out = { places = places.snapshot(s.places), journal = lore.snapshot(s.journal), folk = folk.snapshot(s.folk),
    variant = s.variant }
  if s.variant == "rise" and s.places.ready then out.history = history.save(s.places.db) end -- (too long to make again)
  return out
end

-- A book's text: one of the chronicles of the household's people.
function play.read(s, b)
  local db = s.places.db
  local st = history.get(db, b.settlement)
  local culture = st and st.culture or db.culture[1].id
  local wars = db.wars or {}
  if #wars > 0 then
    local war = wars[(b.n or 1) % #wars + 1]
    local account = lore.war_account(db, war, culture)
    local sec = lore.learn(s.journal, db, war, "book")
    return account.title, account.lines, sec
  end
  local realm = st and st.realm or db.polity[1].id
  local lines = lore.book_of_kings(db, realm, culture)
  return lines[1], lines, lore.learn(s.journal, db, realm, "book")
end

-- Uses what is in front of a walker: talks with someone (or, running, asks
-- their service: heir = { pack, rest, heal, learn, cloak, magic }), opens a
-- door or a container, a dungeon's key, lock or treasure. Returns what was
-- said.
function play.use(s, w, p, heir)
  local P = s.places
  local person = folk.near(P, w.pos, w.heading)
  if person then
    heir = heir or { pack = p }
    if heir.serve then return folk.serve(s.folk, P, person, heir) end
    local line = folk.talk(s.folk, P, person, { cloak = heir.cloak, magic = heir.magic })
    for _, id in ipairs({ person.culture, person.faith }) do lore.learn(s.journal, P.db, id, "talk") end
    return line
  end
  local underground = require("places.underground")
  local g, dp = underground.nearby(P.under, w.pos, w.heading)
  if g then
    local said = underground.use(P.under, g, dp)
    if dp.treasure and dp.gone then pack.add(p, "coin", 20, 0) end
    return said
  end
  local b, piece, kind = places.nearby(P, w.pos, w.heading)
  if not b then return nil end
  if kind == "door" then
    local open = places.toggle_door(P, b, piece)
    return open and "The door opens" or "The door closes"
  end
  local got = {}
  for _, it in ipairs(places.container(P, b, piece.container)) do
    local n = places.take(P, b, piece.container, it[1], pack.add(p, it[1], it[2], 0))
    if n > 0 then got[#got + 1] = items.name(it[1]):lower() .. " " .. n end
    if it[1] == "book" then
      local title = play.read(s, b)
      got[#got] = got[#got] .. " (" .. title .. ")"
    end
  end
  if #got == 0 then return "Empty" end
  return "Taken " .. table.concat(got, ", ")
end

-- Each tick: peopling goes on, places near the heirs come to be, and
-- burning ones burn. viewers: the heirs' positions.
function play.tick(s, viewers, senses, dt)
  local P = s.places
  places.tick(P, viewers)
  if not P.ready then return end
  local t = world.time()
  folk.tick(P, viewers, dt or 1 / 60, { hour = t.hour + t.minute / 60, day = math.floor(t.time / 86400),
    rain = (sim.weather().rain or 0) > 0.3 })
  if senses then -- puzzles under ground give way to what the heirs bring
    for _, opened in ipairs(require("places.underground").puzzles(P.under, viewers, senses)) do
      s.said = "The way opens: " .. require("places.dungeons").PUZZLES[opened.puzzle].text .. " gives way"
    end
  end
  local planned = {} -- fire: two built-up buildings looked at a tick, in turn
  for _, plan in pairs(P.plans) do
    for _, b in ipairs(plan.buildings) do
      if (b.stage or 1) >= 2 then planned[#planned + 1] = b end
    end
  end
  table.sort(planned, function(a, b) return a.id < b.id end)
  for _ = 1, math.min(2, #planned) do
    s.burn_turn = (s.burn_turn or 0) % #planned + 1
    places.burn(P, planned[s.burn_turn])
  end
end

function play.draw(s, eye, person_sprite)
  local n = view.draw(s.places, eye)
  if person_sprite then n = n + folk.draw(s.places, eye, person_sprite, world.time().time) end
  return n
end

-- A line for the HUD: the place one is in or near.
function play.line(s, pos)
  local P = s.places
  if not P.ready then return "The world is being peopled..." end
  local b = places.at(P, pos)
  if b then
    local st = history.get(P.db, b.settlement)
    return string.format("%s, %s", (b.type:gsub("_", " ")), st.name)
  end
  local near = require("people.peopling").near(P.db, P.grid, pos, 3000)[1]
  if near then
    return string.format("%s (%s) %.1f km", near.settlement.name, near.settlement.type:gsub("_", " "),
      near.distance / 1000)
  end
  return nil
end

return play
