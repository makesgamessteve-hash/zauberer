-- SPDX-License-Identifier: MIT
-- The first creatures in play (concept 5.12): the species come from
-- data/creatures.lua (later packages add or replace them by id), a few of
-- each live near where the carpet starts, the creatures learn where the player
-- is every tick, and what happens to them (deaths, flight, attacks) is news
-- for the HUD and events for the rest of the game.
local fauna = {}

local NEWS_TIME = 4 -- seconds a line of news stays on the HUD
fauna.SCENT = 3e-5 -- kg of scent a second the mage leaves on the ground
fauna.SCENT_REACH = 5 -- metres above the ground within which the carpet leaves scent
local RECENT = 32 -- events kept for whoever asks later

-- Who lives near the start: species, how many, metres away, bearing (radians
-- from the carpet's heading).
fauna.GROUPS = {
  { species = "deer", count = 5, distance = 140, bearing = 0.6 },
  { species = "wolf", count = 3, distance = 260, bearing = 2.4 },
  { species = "storm_bird", count = 1, distance = 120, bearing = -0.9 },
}

local species -- id -> data, loaded on first use

-- The species of the game by id.
function fauna.species()
  if not species then
    local text = assert(vfs.read("data/creatures.lua"))
    local list = assert(load(text, "=data/creatures.lua", "t", {}))()
    species = {}
    for _, s in ipairs(list) do species[s.id] = s end
  end
  return species
end

-- Defines every species in the simulation (in id order, so indices are stable).
function fauna.define()
  local ids = {}
  for id in pairs(fauna.species()) do ids[#ids + 1] = id end
  table.sort(ids)
  for _, id in ipairs(ids) do sim.define_species(fauna.species()[id]) end
end

local function name_of(id)
  local s = fauna.species()[id]
  return s and s.name or id
end

-- The creatures around home (the carpet's start) facing heading.
function fauna.new(home, heading)
  fauna.define()
  local f = { ids = {}, news = {}, time = 0, recent = {} }
  for _, g in ipairs(fauna.GROUPS) do
    local centre = world.move(home, (heading or 0) + g.bearing, g.distance, 0)
    for k = 1, g.count do
      local at = world.move(centre, k * 2.4, 3 + 3 * k, 0)
      f.ids[#f.ids + 1] = sim.spawn_creature(g.species, at)
    end
  end
  return f
end

-- Takes the creatures out of the world (leaving flight, a new world).
function fauna.clear(f)
  for _, id in ipairs(f.ids) do sim.remove_creature(id) end
  f.ids = {}
end

local function say(f, text)
  for _, n in ipairs(f.news) do
    if n.text == text then -- the same news again: it stays a little longer
      n.until_time = f.time + NEWS_TIME
      return
    end
  end
  f.news[#f.news + 1] = { text = text, until_time = f.time + NEWS_TIME }
end

local TOLD = {
  died = function(ev)
    if ev.by_species then
      return "A " .. name_of(ev.by_species):lower() .. " killed a " .. name_of(ev.species):lower()
    end
    return "The " .. name_of(ev.species):lower() .. " died (" .. ev.cause .. ")"
  end,
  fled = function(ev) return "A " .. name_of(ev.species):lower() .. " flees" end,
  attacked = function(ev) return "A " .. name_of(ev.species):lower() .. " attacks you" end,
}

-- One tick: tells the creatures where the player is (and how they see the
-- player: seen = { visibility, presence }, optional) and returns what
-- happened to them since the last tick (see sim.creature_events).
function fauna.tick(f, dt, player_pos, on_foot, seen)
  f.time = f.time + dt
  sim.set_player(player_pos, on_foot, seen)
  -- The mage's scent on the ground, for noses downwind.
  if on_foot or player_pos.alt - world.ground_height(player_pos) < fauna.SCENT_REACH then
    sim.emit(player_pos, "scent", fauna.SCENT * dt)
  end
  local events = sim.creature_events()
  for _, ev in ipairs(events) do
    say(f, TOLD[ev.kind](ev))
    f.recent[#f.recent + 1] = ev
    if #f.recent > RECENT then table.remove(f.recent, 1) end
  end
  local kept = {}
  for _, n in ipairs(f.news) do
    if n.until_time > f.time then kept[#kept + 1] = n end
  end
  f.news = kept
  return events
end

-- The HUD lines: the news about creatures.
function fauna.hud_lines(f)
  local lines = {}
  for _, n in ipairs(f.news) do lines[#lines + 1] = n.text end
  return lines
end

return fauna
