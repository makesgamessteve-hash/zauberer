-- SPDX-License-Identifier: MIT
-- A gameplay bot: plays the game for days through the controls only (steer,
-- climb, boost, mount, use, story, sleep, gather, campfire, cook, eat, drink,
-- cast), as a player who reads the HUD would, and writes down what happens:
-- every message the game shows, the story's turns, hurts and defeats, the
-- places it comes to, the creatures it meets, and its own plans and troubles.
-- Run by tools/run_bots.py (STYLE and BUDGET are filled in per run):
--   zauberer --headless --seed <n> --scenario <this file>
-- Styles: "story" follows the story; "explorer" also visits places and talks
-- to people; "hunter" also fights what it meets; "camper" also lives off the land.
scenario { name = "play bot" }
local STYLE = "story" -- (set per run)
local BUDGET = 60000 -- ticks at most (set per run)

local game = demo.game
local P = game.world_places.places
local places = require("places.places")
local prologue = require("story.prologue")
local news = require("player.news")
local walkerm = require("player.walker")
local underground = require("places.underground")
local peopling = require("people.peopling")
local history = require("people.history")
local pack = require("player.pack")

-- ---- the log ---------------------------------------------------------------
local ticks = 0
local function clock()
  local t = world.time().time
  local day = math.floor(t / 86400) + 1
  local s = t % 86400
  return string.format("d%d %02d:%02d", day, math.floor(s / 3600), math.floor(s % 3600 / 60))
end
local function log(kind, text) print(string.format("BOT %s %-7s %s", clock(), kind, text)) end
local recent = {}
local orig_say = news.say
news.say = function(n, text, seconds)
  local key = tostring(text)
  if not recent[key] or world.time().time - recent[key] > 600 then log("SAY", key) end
  recent[key] = world.time().time
  return orig_say(n, text, seconds)
end
local seen_lines = {}
local function diff_lines(kind, lines)
  for _, l in ipairs(lines or {}) do
    local key = kind .. tostring(l)
    if not seen_lines[key] then
      seen_lines[key] = true
      log(kind, tostring(l))
    end
  end
end

-- ---- senses ----------------------------------------------------------------
local function me() return game.walker and game.walker.pos or game.carpet.pos end
local function wrap(a) return (a + math.pi) % (2 * math.pi) - math.pi end
local function bearing(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  return math.atan(b[1] - a[1], -(b[3] - a[3]))
end
local function dist(a, b) return world.distance(a, b) end
local function hour() return (world.time().time % 86400) / 3600 end
local function place_line()
  local ok, line = pcall(require("places.play").line, game.world_places, me())
  return ok and line or nil
end

-- A small generator of its own (the bot's choices vary with the seed).
local rs = engine.info().seed % 2147483647 + 1
local function rand() rs = (rs * 48271) % 2147483647 return rs / 2147483647 end

-- ---- acting ----------------------------------------------------------------
local pulse -- (watch, set below: called every half second, whatever the bot is doing)
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
  ticks = ticks + 2
  if pulse and ticks % 30 < 2 then pulse() end
end
local function tick_once()
  step(1)
  ticks = ticks + 1
  if pulse and ticks % 30 == 0 then pulse() end
end
local function wait(seconds, cond)
  for _ = 1, math.floor(seconds * 60) do
    tick_once()
    if cond and cond() then return true end
    if ticks > BUDGET then return false end
  end
  return false
end

local troubles = {}
local function trouble(what)
  troubles[what] = (troubles[what] or 0) + 1
  if troubles[what] <= 3 then log("STUCK", what) end
end

-- ---- finding the way ------------------------------------------------------
-- A binary heap of { f, key }.
local function heap_push(h, f, key)
  h[#h + 1] = { f, key }
  local i = #h
  while i > 1 do
    local up = i // 2
    if h[up][1] <= h[i][1] then break end
    h[up], h[i] = h[i], h[up]
    i = up
  end
end
local function heap_pop(h)
  local top = h[1]
  h[1] = h[#h]
  h[#h] = nil
  local i = 1
  while true do
    local l, r, m = i * 2, i * 2 + 1, i
    if h[l] and h[l][1] < h[m][1] then m = l end
    if h[r] and h[r][1] < h[m][1] then m = r end
    if m == i then break end
    h[m], h[i] = h[i], h[m]
    i = m
  end
  return top
end

-- Where walls stand near a point, as a map of quarter-metre cells (east and
-- north of la, the local position of the search's start): the ground-floor
-- walls, windows, posts and wells of every building near, with the game's
-- 0.2 m to spare (doors count as open: the bot opens them on the way), and
-- which building's footprint each cell is in.
local CELL = 0.25
local BLOCKING = { wall = true, window = true, post = true, well = true, windcatcher = true }
local function cell_key(x, n) return math.floor(x / CELL) * 1000003 + math.floor(n / CELL) end
local function wall_map(la, reach)
  local occ, house = {}, {}
  for _, plan in pairs(P.plans) do
    for _, bd in ipairs(plan.buildings) do
      if (bd.stage or 1) >= 2 and bd.pieces then
        local lo = world.to_local((places.origin(P, bd)))
        local ox, on = lo[1] - la[1], -(lo[3] - la[3])
        if math.abs(ox) < reach and math.abs(on) < reach then
          local c, s = math.cos(bd.yaw), math.sin(bd.yaw)
          -- The cells whose centres lie in a box of the building's frame.
          local function mark(map, x0, x1, z0, z1, val)
            local r = math.sqrt(math.max(x0 * x0, x1 * x1) + math.max(z0 * z0, z1 * z1))
            for u = math.floor((ox - r) / CELL), math.floor((ox + r) / CELL) do
              local dx = (u + 0.5) * CELL - ox
              for v = math.floor((on - r) / CELL), math.floor((on + r) / CELL) do
                local dz = -((v + 0.5) * CELL - on) -- (local z points south)
                local bx, bz = dx * c + dz * s, dx * s - dz * c
                if bx >= x0 and bx <= x1 and bz >= z0 and bz <= z1 then map[u * 1000003 + v] = val end
              end
            end
          end
          mark(house, -bd.w / 2, bd.w / 2, -bd.d / 2, bd.d / 2, bd)
          for _, p in ipairs(bd.pieces) do
            if BLOCKING[p.kind] and not p.gone and p.y < 1.6 and p.y + p.h > 0.3 then
              local hw, hd = p.w / 2 + 0.2, p.d / 2 + 0.2
              mark(occ, p.x - hw, p.x + hw, p.z - hd, p.z + hd, true)
            end
          end
        end
      end
    end
  end
  return occ, house
end

-- A way on foot from a to b round walls, rivers and other people's houses:
-- an A* search over a grid of half-metre steps (below ground, in a cellar,
-- the game's own wall test). Returns waypoints (the last within reach of b), or nil.
local STEP = 0.5
local NEIGHBOURS = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } }
local function find_way(a, b, alt, reach)
  local la, lb = world.to_local(a), world.to_local(b)
  local gx, gy = lb[1] - la[1], -(lb[3] - la[3])
  local span = math.floor((math.max(60, 1.5 * math.sqrt(gx * gx + gy * gy) + 20)) / STEP)
  local below = alt < world.ground_height(a) - 1.5
  local occ, house = wall_map(la, span * STEP + 40)
  local goal_house = house[cell_key(gx, gy)]
  local function clear_at(x, n) return not occ[cell_key(x, n)] end
  local W = 2 * span + 1
  local function key(i, j) return (j + span) * W + (i + span) end
  local pos, wet = {}, {}
  local function at(i, j)
    local k = key(i, j)
    local p = pos[k]
    if not p then
      p = world.move(a, 0, j * STEP, i * STEP, 0)
      p.alt = below and alt or world.ground_height(p)
      local _, _, depth = world.water(p)
      pos[k], wet[k] = p, (depth or 0) > walkerm.WADE
    end
    return p
  end
  local cost, from, open = {}, {}, {}
  local k0 = key(0, 0)
  cost[k0] = 0
  at(0, 0)
  heap_push(open, 0, k0)
  local found
  local budget = 30000
  local near = math.max(reach, 1.2)
  while #open > 0 and budget > 0 do
    budget = budget - 1
    local k = heap_pop(open)[2]
    local i, j = k % W - span, k // W - span
    local x, n = i * STEP, j * STEP
    if (gx - x) ^ 2 + (gy - n) ^ 2 <= near * near then found = k break end
    local p = pos[k]
    for _, d in ipairs(NEIGHBOURS) do
      local ni, nj = i + d[1], j + d[2]
      if math.abs(ni) <= span and math.abs(nj) <= span then
        local nk = key(ni, nj)
        local q = at(ni, nj)
        local ok = not wet[nk]
        if ok and below then
          ok = not places.blocked(P, p, q, alt)
        elseif ok then
          ok = clear_at(ni * STEP, nj * STEP) and clear_at((i + ni) * STEP / 2, (j + nj) * STEP / 2)
        end
        if ok then
          local step = ((d[1] ~= 0 and d[2] ~= 0) and 1.414 or 1) * STEP
          local h = house[cell_key(ni * STEP, nj * STEP)]
          if h and h ~= goal_house then step = step * 4 end -- (round other houses, not through them)
          local c = cost[k] + step
          if not cost[nk] or c < cost[nk] then
            cost[nk], from[nk] = c, k
            heap_push(open, c + math.sqrt((gx - ni * STEP) ^ 2 + (gy - nj * STEP) ^ 2), nk)
          end
        end
      end
    end
  end
  if not found then return nil end
  local cells = {}
  local k = found
  while k do
    table.insert(cells, 1, { (k % W - span) * STEP, (k // W - span) * STEP, k })
    k = from[k]
  end
  -- Straightened: from each point on to the furthest one in plain sight.
  local function sight(c1, c2)
    if below then return false end
    local dx, dy = c2[1] - c1[1], c2[2] - c1[2]
    local steps = math.ceil(math.sqrt(dx * dx + dy * dy) / 0.2)
    for s = 1, steps do
      if not clear_at(c1[1] + dx * s / steps, c1[2] + dy * s / steps) then return false end
    end
    return true
  end
  local out, m0 = { pos[cells[1][3]] }, 1
  while m0 < #cells do
    local m = m0 + 1
    for look = math.min(#cells, m0 + 40), m0 + 2, -1 do
      if sight(cells[m0], cells[look]) then m = look break end
    end
    out[#out + 1] = pos[cells[m][3]]
    m0 = m
  end
  return out
end

-- Walks to a position (on foot); true when within reach. Finds a way round
-- walls; bumping into a closed door, it presses use; stuck, it looks for a
-- way again, or tries a step aside.
local OFFSETS = { 0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.1, -2.1, 2.8 }
local side_bias = 1
local function walk_to(target, reach, why, seconds)
  if not game.walker then return false end
  reach = reach or 2
  local best, since = math.huge, 0
  local way, wi
  local function plan()
    local w = game.walker
    way, wi = find_way(w.pos, target, w.pos.alt, reach), 1
  end
  plan()
  for _ = 1, math.floor((seconds or 90) * 60) do
    local w = game.walker
    if not w then return false end
    local d = dist(w.pos, target)
    if d < reach then return true end
    local aim = target
    if way then
      while wi < #way and dist(w.pos, way[wi]) < 0.8 do wi = wi + 1 end
      aim = way[wi]
    end
    local want = bearing(w.pos, aim)
    local going_in = places.at(P, target) ~= nil
    local inside = places.at(P, w.pos) ~= nil
    if not way then -- the nearest heading round a wall
      for _, off in ipairs(OFFSETS) do
        local h = want + off * side_bias
        local probe = world.move(w.pos, h, math.min(3, d), 0, 0)
        if not places.blocked(P, w.pos, probe, w.pos.alt)
          and (going_in or inside or not places.at(P, probe)) then -- (round houses, not through them)
          want = h
          break
        end
      end
    end
    local diff = wrap(want - w.heading)
    local turn = math.max(-1, math.min(1, diff * 2))
    local fwd = math.abs(diff) < 0.6 and 1 or 0.2
    if d < best - 0.3 then best, since = d, 0 else since = since + 1 end
    if w.bumped and since % 30 == 1 then press("use") end -- (a door on the way)
    if since > 240 then -- four seconds without getting nearer: a step aside, and a new way
      side_bias = -side_bias
      local side = rand() < 0.5 and 1 or -1
      for _ = 1, 50 do input.inject("steer", side, 0.2, 1) tick_once() end
      for _ = 1, 30 do input.inject("steer", 0, 1, 1) tick_once() end
      if not game.walker then return false end
      plan()
      since, best = 0, dist(game.walker.pos, target)
    end
    input.inject("steer", turn, fwd, 1)
    if d > 15 then input.inject("boost", 1, 0, 1) end
    tick_once()
    if ticks > BUDGET then return false end
  end
  local w = game.walker
  -- What it ran into: the nearest piece ahead in the building it is in.
  local bumped = ""
  local bd = w and places.at(P, w.pos)
  if bd and w.bumped then
    local x, y, z = places.to_building(P, bd, world.move(w.pos, w.heading, 0.6, 0, 0))
    local best, bdist
    for _, p in ipairs(bd.pieces or {}) do
      local dd = math.max(math.abs(x - p.x) - p.w / 2, math.abs(z - p.z) - p.d / 2, 0)
      if not p.gone and p.kind ~= "floor" and (not bdist or dd < bdist) then best, bdist = p, dd end
    end
    if best then
      bumped = string.format(" (%s %s at %.1f m, y %.1f..%.1f, feet %.1f)", best.kind, tostring(best.mat), bdist,
        best.y, best.y + best.h, y)
    end
  end
  trouble("could not walk to " .. (why or "a place") .. (w and string.format(
    " (%.0f m short%s%s%s%s)", dist(w.pos, target), w.blocked and ", water too deep" or "",
    w.bumped and (", against a wall" .. bumped) or "", places.at(P, w.pos) and ", inside a building" or "",
    way and "" or ", no way found") or ""))
  return false
end

local function face(target)
  for _ = 1, 90 do
    local w = game.walker
    if not w then return end
    local diff = wrap(bearing(w.pos, target) - w.heading)
    if math.abs(diff) < 0.1 then return end
    input.inject("steer", math.max(-1, math.min(1, diff * 2)), 0, 1)
    tick_once()
  end
end

-- Onto the carpet (walking to it first).
local function board()
  if not game.walker then return true end
  if not walk_to(game.carpet.pos, walkerm.REACH - 0.5, "the carpet", 120) then return false end
  press("mount")
  wait(0.5)
  if game.walker then
    local w, c = game.walker, game.carpet
    log("DO", string.format("(carpet %.1f m away, %.1f m above; cloak %s, tent %s, down %s)",
      world.distance(w.pos, c.pos), c.pos.alt - w.pos.alt, tostring(game.cloak.worn), tostring(game.tent ~= nil),
      tostring(game.health.down ~= nil)))
    trouble("the carpet would not take the heir")
    return false
  end
  log("DO", "steps onto the carpet")
  return true
end

-- Flies to a position at a height above the ground; lands and steps off if land is true.
local function fly_to(target, above, land, why)
  if game.walker and not board() then return false end
  above = above or 40
  log("DO", "flies to " .. (why or "a place") .. string.format(" (%.1f km)", dist(me(), target) / 1000))
  for _ = 1, 60 * 600 do
    local c = game.carpet
    local d = dist(c.pos, target)
    local ground = world.ground_height(c.pos)
    local want_alt = (target.alt and above == 0) and target.alt or (math.max(ground, world.height(target)) + above)
    if land and d < 60 then want_alt = ground + 2 end
    local climb = math.max(-1, math.min(1, (want_alt - c.pos.alt) / 10))
    local diff = wrap(bearing(c.pos, target) - c.heading)
    local turn = math.max(-1, math.min(1, diff * 1.5))
    -- Speed to suit the distance (slow on the approach, stopping near), slow in sharp turns.
    local want_speed = math.max(0, math.min(60, (d - 15) * 0.25))
    local throttle = math.max(-1, math.min(1, (want_speed - c.speed) / 5))
    if math.abs(diff) > 1.2 then throttle = math.min(throttle, 0) end
    input.inject("steer", turn, throttle, 1)
    input.inject("climb", climb, 0, 1)
    if d > 1500 and math.abs(diff) < 0.3 then input.inject("boost", 1, 0, 1) end
    tick_once()
    if not land and d < 30 then return true end
    if land and d < 40 and c.speed < walkerm.CARPET_SLOW and c.pos.alt - ground < walkerm.CARPET_LOW then
      press("mount")
      wait(0.5)
      if game.walker then log("DO", "steps off the carpet") return true end
    end
    if ticks > BUDGET then return false end
  end
  trouble("could not fly to " .. (why or "a place"))
  return false
end

-- Goes to a place, on foot if near, else by carpet (when it flies yet).
local function go(target, why, reach)
  local d = dist(me(), target)
  local can_fly = not game.story or prologue.can_fly(game.story)
  if d > 250 and can_fly then
    if not fly_to(target, 40, true, why) then return false end
  elseif not game.walker then
    -- On the carpet but it does not fly yet, or near: get down first.
    fly_to(world.move(me(), 0, 0, 0, 0), 0, true, "the ground")
  end
  log("DO", "walks to " .. (why or "a place"))
  return walk_to(target, reach or 2.5, why, 180)
end

-- ---- staying alive ---------------------------------------------------------
local last_hp = game.health.hp
local function upkeep()
  local s = game.survival
  if game.walker and s.food < 40 then
    local before = s.food
    press("eat")
    if s.food > before then log("DO", "eats") end
  end
  if game.walker and s.water < 40 then
    local before = s.water
    press("drink")
    if s.water > before then log("DO", "drinks") end
  end
  if game.health.hp < last_hp - 4 then
    -- Hurt: anything alive and close is a foe; the left hand casts at it.
    local foe
    for _, id in ipairs(sim.creatures_near(me(), 20)) do
      local cr = sim.creature(id)
      if cr and cr.alive and (not foe or dist(me(), cr.pos) < dist(me(), foe.pos)) then foe = cr end
    end
    if foe and game.walker and (STYLE == "hunter" or game.health.hp < 60) then
      log("DO", "turns on the " .. tostring(foe.species) .. " and casts")
      face(foe.pos)
      input.inject("cast_left", 1, 0, 50)
      wait(1)
      wait(0.5)
    end
  end
  last_hp = game.health.hp
end

-- ---- watching --------------------------------------------------------------
local last_place, last_beat, last_status, met = nil, nil, -1, {}
local species_seen = {}
local taken_seen = {}
local function watch()
  local line = place_line()
  if line and line ~= last_place and not line:match("%d+%.%d km") then log("PLACE", line) end
  if line then last_place = line end
  if game.story and game.story.beat ~= last_beat then
    last_beat = game.story.beat
    log("BEAT", "prologue: " .. last_beat)
  end
  if game.story then diff_lines("STORY", game.story.lines) end
  if game.saga then
    diff_lines("SAGA", game.saga.lines)
    diff_lines("SAGA", require("story.saga").hud_lines(game.saga, me()))
  end
  for _, n in ipairs(game.health.news or {}) do
    local key = "HURT" .. n.text .. math.floor(world.time().time / 300)
    if not seen_lines[key] then seen_lines[key] = true log("HURT", n.text) end
  end
  for harm, total in pairs(game.health.taken or {}) do
    local before = taken_seen[harm] or 0
    if total - before >= 5 then
      taken_seen[harm] = total
      log("HARM", string.format("%s: %d (health %d)%s", harm, math.floor(total - before + 0.5),
        math.ceil(game.health.hp), last_place and (", " .. last_place) or ""))
    end
  end
  if game.health.down and not seen_lines["down" .. math.floor(world.time().time / 600)] then
    seen_lines["down" .. math.floor(world.time().time / 600)] = true
    log("DOWN", "the heir is defeated")
  end
  for _, id in ipairs(sim.creatures_near(me(), 40)) do
    local cr = sim.creature(id)
    if cr and cr.alive and not species_seen[cr.species] then
      species_seen[cr.species] = true
      log("SEE", tostring(cr.species))
    end
  end
  local h = math.floor(world.time().time / 3600)
  if h ~= last_status and h % 3 == 0 then
    last_status = h
    local s = game.survival
    log("STATUS", string.format("%s, hp %d, food %d, water %d, rest %d, %s%s",
      game.walker and "on foot" or "on the carpet", math.ceil(game.health.hp), math.floor(s.food),
      math.floor(s.water), math.floor(s.rest), last_place or "in the wild",
      game.story and (", story: " .. game.story.beat) or ""))
  end
end
-- Out of the fire: on foot, near flames, it runs for a few seconds towards
-- the nearest side clear of them (as a player would).
local healthm = require("player.health")
local fleeing = false
local function flee_fire()
  local w = game.walker
  if fleeing or not w or game.health.down or not healthm.fire_near(w.pos, 4) then return end
  fleeing = true
  local away
  for r = 15, 45, 15 do
    for k = 0, 7 do
      local p = world.move(w.pos, w.heading + k * math.pi / 4, r, 0, 0)
      if not healthm.fire_near(p, 6) then away = p break end
    end
    if away then break end
  end
  if away then
    log("DO", "runs from the fire")
    for _ = 1, 240 do
      local v = game.walker
      if not v then break end
      local diff = wrap(bearing(v.pos, away) - v.heading)
      input.inject("steer", math.max(-1, math.min(1, diff * 2)), 1, 1)
      input.inject("boost", 1, 0, 1)
      step(1)
      ticks = ticks + 1
      if dist(v.pos, away) < 3 then break end
    end
  end
  fleeing = false
end
pulse = function()
  watch()
  flee_fire()
end
local function wait_watch(seconds, cond)
  local n = math.floor(seconds * 60 / 30)
  for _ = 1, math.max(1, n) do
    if wait(0.5, cond) then return true end
    watch()
    upkeep()
    if ticks > BUDGET then return false end
  end
  return false
end

-- ---- the prologue ----------------------------------------------------------
local function farrokh_person(where)
  for _, p in ipairs(where.plan.people or {}) do if p.name == "Farrokh" then return p end end
end
local function house_route(where)
  local house = where.house
  local door
  for _, d in ipairs(house.doors or {}) do if d.kind == "front" then door = d end end
  local x = door and door.pos or 0
  return places.to_world(P, house, x, 0, house.d / 2 + 2), places.to_world(P, house, x, 0, house.d / 2 - 1.5)
end
local function into_house(where, target, why)
  local outside, inside = house_route(where)
  go(outside, "Farrokh's door")
  walk_to(inside, 1.2, "inside Farrokh's house", 30)
  return walk_to(target, 1.5, why, 40)
end
local function stair(where)
  local g = where.sanctum
  local top = world.move(g.origin, 0, g.stair.z0 - 0.3, g.stair.x, 0)
  top.alt = g.origin.alt + underground.DEPTH
  local bottom = world.move(g.origin, 0, g.stair.z1, g.stair.x, 0)
  bottom.alt = g.origin.alt
  return top, bottom
end

local function play_prologue()
  local S = game.story
  local where = S.places
  local started = world.time().time
  local beat_now, tries = nil, 0
  while S.beat ~= "done" and ticks < BUDGET and world.time().time - started < 3 * 86400 do
    local b = S.beat
    if b ~= beat_now then beat_now, tries = b, 0 end
    tries = tries + 1
    if tries == 30 then trouble("no way on at '" .. b .. "' after 30 tries") end
    watch()
    if b == "village" then
      if not S.chores.water then go(where.qanat_pos, "the qanat outlet", 3.4); press("use")
      elseif not S.chores.goats then go(where.pen_pos, "the goat pen", 4.5); press("use")
      elseif not S.chores.pomegranates then go(where.orchard, "the orchard", 20); press("use")
      elseif not S.talked then
        local f = farrokh_person(where)
        if f then
          log("DO", "looks for Farrokh (" .. tostring(f.doing) .. ")")
          if places.at(P, f.pos) then into_house(where, f.pos, "Farrokh") else go(f.pos, "Farrokh", 2) end
          for _ = 1, 6 do
            if S.talked then break end
            walk_to(f.pos, 1.6, "Farrokh", 10)
            face(f.pos)
            press("use")
          end
        end
      else
        into_house(where, where.loom_pos, "the loom")
        press("use")
      end
      wait_watch(1)
    elseif b == "arrival" or b == "hiding" then
      wait_watch(5, function() return S.beat ~= b end)
    elseif b == "handover" or (b == "ashes" and S.out) then
      local top, bottom = stair(where)
      if b == "ashes" then log("DO", "goes back down into the cellar") else log("DO", "runs to hide in the cellar") end
      into_house(where, top, "the hatch")
      walk_to(bottom, 1, "the foot of the cellar stair", 20)
      wait_watch(2, function() return S.beat ~= b end)
    elseif b == "ashes" then
      local top = stair(where)
      log("DO", "climbs out of the cellar")
      walk_to(top, 1, "the top of the stair", 20)
      local outside = house_route(where)
      walk_to(outside, 2, "out of the house", 30)
      go(where.farrokh_pos, "the ruins of the house", 6)
      press("use")
      wait_watch(1)
    elseif b == "sanctum" then
      local g = where.sanctum
      for _, p in ipairs(g.pieces) do
        if p.treasure and not p.taken then
          local at = world.move(g.origin, 0, p.z + 1.2, p.x, 0)
          at.alt = g.origin.alt
          walk_to(at, 1.2, "a shelf in the sanctum", 20)
          face(world.move(g.origin, 0, p.z, p.x, 0))
          press("use")
        end
      end
      wait_watch(1)
    elseif b == "carpet" then
      log("DO", "lies down on the rug to sleep")
      press("sleep")
      wait_watch(2, function() return S.beat ~= b end)
    elseif b == "flying" then
      local g = where.sanctum
      if game.walker and game.walker.pos.alt < g.origin.alt + 3 then -- still down in the cellar: up and out
        local top = stair(where)
        log("DO", "climbs out of the cellar with the rug")
        walk_to(top, 1, "the top of the stair", 30)
        walk_to(house_route(where), 2, "out of the house", 30)
      end
      local rings = prologue.rings(S)
      local r = rings[S.ring]
      if r then fly_to(r, 0, false, "ring " .. S.ring) end
      wait_watch(0.5)
    elseif b == "survival" then
      if not game.walker then fly_to(world.move(me(), 0, 0, 0, 0), 0, true, "a camp") end
      log("DO", "gathers, lights a fire and boils water")
      for _ = 1, 3 do press("gather") wait_watch(1) end
      press("campfire")
      wait_watch(3)
      for _ = 1, 3 do press("cook") wait_watch(2) end
      game.gathered_now = game.gathered_now
    elseif b == "leaving" then
      local away = world.move(where.square, rand() * 2 * math.pi, 3500, 0, 0)
      fly_to(away, 80, false, "out of the valley")
    end
  end
  if S.beat ~= "done" then trouble("the prologue stalled at '" .. S.beat .. "'") end
end

-- ---- after the prologue ----------------------------------------------------
local function saga_target()
  local A = game.saga
  if not A then return nil end
  local N = A.N
  if A.trail and not N.done.trail then return A.trail, "the trail of ash" end
  for _, m in ipairs(require("story.narrative").data().circle) do
    local site = N.sites[m.id]
    if site and not N.remnants[m.id] then return site.pos, site.name end
  end
  local mage = A.R and A.R.list and A.R.list[1]
  if mage and not A.met[mage.id] then
    local st = history.get(P.db, mage.sanctum)
    if st then return st.pos, "the rival " .. tostring(mage.name) end
  end
  if A.citadel and not A.duel then return A.citadel, "Azhdar's citadel" end
  return nil
end

local function visit_people()
  local near = peopling.near(P.db, P.grid, me(), 15000)
  local pick
  for _, n in ipairs(near) do
    if not met[n.settlement.id] then pick = n break end
  end
  if not pick then return false end
  met[pick.settlement.id] = true
  local st = pick.settlement
  go(st.pos, st.name .. " (" .. tostring(st.type):gsub("_", " ") .. ")", 8)
  wait_watch(3)
  local plan = P.plans[st.id]
  local talked = 0
  for _, p in ipairs(plan and plan.people or {}) do
    if talked >= 2 then break end
    if p.pos and not places.at(P, p.pos) and dist(me(), p.pos) < 60 then
      walk_to(p.pos, 1.6, p.name, 20)
      face(p.pos)
      press("use")
      talked = talked + 1
      wait_watch(1)
    end
  end
  return true
end

-- ---- the run ---------------------------------------------------------------
log("START", string.format("style %s, seed %d", STYLE, engine.info().seed))
wait(15, function() return P.ready end)
for _ = 1, 600 do
  if world.chunk_stage(me()) == 3 then break end
  tick_once()
  sleep(5)
end
log("PLACE", place_line() or "in the wild")
if STYLE == "camper" or STYLE == "explorer" then
  log("DO", "flies about before the story (" .. STYLE .. ")")
  for _ = 1, 2 do
    local a = rand() * 2 * math.pi
    fly_to(world.move(me(), a, 1500 + rand() * 1500, 0, 0), 60, false, "somewhere new")
    wait_watch(2)
  end
  fly_to(world.move(game.home.pos, 0, 0, 0, 0), 40, true, "home")
end
log("DO", "presses F5: the story begins")
press("story")
wait_watch(2)
if STYLE == "check" and game.story then
  -- A self-check of the bot's wall map against the game's own wall test, round Farrokh's house.
  local where = game.story.places
  local o = where.farrokh_pos
  local la = world.to_local(o)
  local occ = wall_map(la, 80)
  local agree, walls, total = 0, 0, 0
  for x = -12, 12, 0.5 do
    for n = -12, 12, 0.5 do
      local p = world.move(o, 0, n, x, 0)
      p.alt = world.ground_height(p)
      local game_says = places.blocked(P, p, p, p.alt)
      local mine = occ[cell_key(x, n)] == true
      total = total + 1
      if game_says then walls = walls + 1 end
      if not game_says or mine then agree = agree + 1 end -- (every wall the game knows is on the map)
    end
  end
  log("CHECK", string.format("wall map: %d of %d points agree, %d in walls", agree, total, walls))
  local way = find_way(where.pen_pos, where.orchard, world.ground_height(where.pen_pos), 2)
  log("CHECK", string.format("pen to orchard: %s", way and (#way .. " waypoints") or "no way"))
  local _, inside = house_route(where)
  way = find_way(where.qanat_pos, inside, world.ground_height(where.qanat_pos), 1.2)
  log("CHECK", string.format("qanat to inside Farrokh's house: %s", way and (#way .. " waypoints") or "no way"))
  local f = farrokh_person(where)
  if f then
    way = find_way(inside, f.pos, world.ground_height(inside), 1.6)
    log("CHECK", string.format("to Farrokh (%s): %s", tostring(f.doing), way and (#way .. " waypoints") or "no way"))
  end
  BUDGET = 0
elseif game.story then play_prologue() else trouble("F5 did not begin the story") end
local idle = 0
while ticks < BUDGET do
  watch()
  upkeep()
  local h = hour()
  if game.walker and (h >= 22 or h < 5) then
    log("DO", "makes camp and sleeps")
    if STYLE == "camper" then press("campfire") end
    press("sleep")
    wait_watch(3)
  else
    local target, why = saga_target()
    if target and (STYLE == "story" or rand() < 0.7) then
      go(target, why, 6)
      press("use")
      wait_watch(4)
    elseif STYLE ~= "story" and visit_people() then
      idle = 0
    else
      idle = idle + 1
      if idle == 1 and target == nil then trouble("nothing more the story asks that the bot can find") end
      local flies = not game.story or prologue.can_fly(game.story)
      if STYLE == "camper" or idle % 2 == 0 or not flies then
        press("gather")
        wait_watch(5)
      else
        fly_to(world.move(me(), rand() * 2 * math.pi, 2000, 0, 0), 60, true, "the wild")
      end
    end
  end
end
-- ---- the end ---------------------------------------------------------------
local N = game.saga and game.saga.N
local done = {}
for k, v in pairs(N and N.done or {}) do if v then done[#done + 1] = k end end
table.sort(done)
log("END", string.format("after %d ticks; prologue %s; saga %s; hp %d; codex %d entries", ticks,
  game.story and game.story.beat or "not begun", #done > 0 and table.concat(done, ", ") or "-",
  math.ceil(game.health.hp), (function()
    local n = 0
    for _ in pairs(game.codex.entries or game.codex.known or {}) do n = n + 1 end
    return n
  end)()))
local items = {}
for id, n in pairs(pack.counts and pack.counts(game.pack) or {}) do items[#items + 1] = id .. " " .. n end
if #items > 0 then log("END", "carrying " .. table.concat(items, ", ")) end
for what, n in pairs(troubles) do log("END", string.format("trouble x%d: %s", n, what)) end
