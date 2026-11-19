-- SPDX-License-Identifier: MIT
-- The narrative director (concept 1.7): the story after the prologue as a
-- graph of beats (data/story.lua), anchored into the generated world.
-- Beats declare the sites they need; the director finds candidates in the
-- history database, picks one by the seed (the same seed and choices give
-- the same story) and reserves it. Rumours point to what comes next by
-- direction and distance, never by a marker. Heirs who join later meet the
-- others at a reunion in a settlement on the way. The backstory is already
-- in the history (people.chronicle weaves it in at world creation).
local rng = require("people.rng")
local npcs = require("people.npcs")

local narrative = {}

narrative.VISIT = 60          -- metres from a site that count as being there
narrative.PREFER = 60000      -- metres: a remnant is found about this far from home

local data
function narrative.data()
  if not data then
    local text = assert(vfs.read("data/story.lua"))
    data = assert(load(text, "=data/story.lua", "t", {}))()
  end
  return data
end

function narrative.new(saved)
  saved = type(saved) == "table" and saved or {}
  local N = { done = saved.done or {}, sites = saved.sites or {}, weaves = saved.weaves or {},
    remnants = saved.remnants or {}, revealed = saved.revealed or {}, rivals_met = saved.rivals_met or 0,
    divined = saved.divined, ready = saved.ready, defeated = saved.defeated, ending = saved.ending,
    journal = saved.journal or {}, reunions = saved.reunions or {} }
  for _, w in ipairs(narrative.data().weaves) do
    N.weaves[w.id] = N.weaves[w.id] or w.holder or "hidden"
  end
  N.weaves.wandering = "heirs"
  return N
end

function narrative.snapshot(N)
  return { done = N.done, sites = N.sites, weaves = N.weaves, remnants = N.remnants, revealed = N.revealed,
    rivals_met = N.rivals_met, divined = N.divined, ready = N.ready, defeated = N.defeated, ending = N.ending,
    journal = N.journal, reunions = N.reunions }
end

local function cell_class(grid, st)
  local c = grid.cells[st.cell]
  return c and c.class, c and c.coast
end

-- The candidates for a site requirement: { id, pos, name }.
function narrative.candidates(db, grid, spec)
  local out = {}
  local function ok_class(cls)
    if not spec.class then return true end
    for _, k in ipairs(spec.class) do if k == cls then return true end end
    return false
  end
  if spec.kind == "settlement" or spec.kind == "tower" then
    for _, st in ipairs(db.settlement) do
      local cls, coast = cell_class(grid, st)
      local fits = not st.ruined and not st.story and ok_class(cls) and (not spec.coast or coast)
      if spec.kind == "tower" then
        fits = fits and (st.type == "mage_tower" or st.type == "temple" or st.type == "mining_town")
      end
      if fits then out[#out + 1] = { id = st.id, pos = st.pos, name = st.name } end
    end
    if #out == 0 and spec.class then return narrative.candidates(db, grid, { kind = spec.kind, coast = spec.coast }) end
  elseif spec.kind == "grave" or spec.kind == "elder ruin" then
    local want = spec.kind == "grave" and "tomb" or "elder ruin"
    for _, site in ipairs(db.site) do
      if site.kind == want then out[#out + 1] = { id = site.id, pos = site.pos, name = site.name } end
    end
  elseif spec.kind == "burnt" then
    for _, st in ipairs(db.settlement) do
      if st.destroyed and not st.story then out[#out + 1] = { id = st.id, pos = st.pos, name = st.name } end
    end
  end
  return out
end

-- Picks a site for a key from candidates: the one nearest the preferred
-- distance from home, ties broken by the seed; not one reserved already.
function narrative.pick(N, db, key, cands, home, prefer)
  local taken = {}
  for _, s in pairs(N.sites) do taken[s.id] = true end
  local best, bs
  for _, c in ipairs(cands) do
    if not taken[c.id] then
      local score = math.abs(world.distance(home, c.pos) - (prefer or narrative.PREFER))
        + rng.float(db.seed, key, c.id) * 1000
      if not bs or score < bs then best, bs = c, score end
    end
  end
  if best then N.sites[key] = { id = best.id, pos = best.pos, name = best.name } end
  return best
end

-- Anchors the Circle's remnants into the world (once).
function narrative.anchor(N, db, grid, home)
  for _, m in ipairs(narrative.data().circle) do
    if not N.sites[m.id] then narrative.pick(N, db, m.id, narrative.candidates(db, grid, m.site), home) end
  end
  return N.sites
end

-- The beats open now (their requirements done, they not).
function narrative.open(N)
  local out = {}
  for _, b in ipairs(narrative.data().beats) do
    local ready = not N.done[b.id]
    for _, r in ipairs(b.requires) do ready = ready and N.done[r] end
    if ready then out[#out + 1] = b end
  end
  return out
end

-- How many Weaves the heirs hold.
function narrative.held(N)
  local n = 0
  for _, holder in pairs(N.weaves) do if holder == "heirs" then n = n + 1 end end
  return n
end

-- Meeting a Circle member's remnant at their site: what they teach, the
-- next revelation, and their Weave (secured, or already taken by Azhdar).
-- ctx: { learn(discipline) }. Returns lines said.
function narrative.meet_remnant(N, id, ctx)
  local m
  for _, x in ipairs(narrative.data().circle) do if x.id == id then m = x end end
  if not m or N.remnants[id] then return {} end
  N.remnants[id] = true
  local lines = {}
  if m.remnant == "teacher" or m.remnant == "student" then
    if ctx.learn then ctx.learn(m.discipline) end
    lines[#lines + 1] = m.role .. " teaches you " .. m.discipline
  else
    lines[#lines + 1] = "At the " .. (m.remnant == "grave" and "grave" or "tower") .. " of " .. m.role
      .. " you learn what " .. m.discipline .. " they left"
  end
  local revs = narrative.data().revelations
  for i, r in ipairs(revs) do
    if not N.revealed[i] then
      N.revealed[i] = true
      lines[#lines + 1] = r
      N.journal[#N.journal + 1] = r
      break
    end
  end
  if N.weaves[m.weave] == "azhdar" then
    lines[#lines + 1] = "The Weave that was kept here is gone: Azhdar came first."
  elseif N.weaves[m.weave] ~= "heirs" then
    N.weaves[m.weave] = "heirs"
    lines[#lines + 1] = "You secure " .. narrative.weave_name(m.weave)
  end
  return lines
end

function narrative.weave_name(id)
  for _, w in ipairs(narrative.data().weaves) do if w.id == id then return w.name end end
  return id
end

function narrative.count(t)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  return n
end

-- Moves the story on from what the heirs did. ctx: { pos (an heir), trail
-- (the burnt place's position), rival_met, divined, ready, defeated,
-- ending, learn }. Returns the beats completed now and lines said.
function narrative.tick(N, ctx)
  local completed, said = {}, {}
  for key, site in pairs(N.sites) do -- reaching a remnant
    if ctx.pos and not N.remnants[key] and world.distance(ctx.pos, site.pos) < narrative.VISIT then
      for _, l in ipairs(narrative.meet_remnant(N, key, ctx)) do said[#said + 1] = l end
    end
  end
  if ctx.rival_met then N.rivals_met = N.rivals_met + 1 end
  for _, b in ipairs(narrative.open(N)) do
    local t, ok = b.trigger, false
    if t == "visit_ruin" then ok = ctx.trail and ctx.pos and world.distance(ctx.pos, ctx.trail) < 300
    elseif t == "remnants" then ok = narrative.count(N.remnants) >= b.need
    elseif t == "rival_met" then ok = N.rivals_met > 0
    elseif t == "revelations" then ok = narrative.count(N.revealed) >= b.need
    elseif t == "divined" then ok = ctx.divined and (narrative.held(N) >= b.weaves or ctx.deep_divination)
    elseif t == "ready" then ok = ctx.ready
    elseif t == "azhdar_defeated" then ok = ctx.defeated
    elseif t == "ending_chosen" then ok = ctx.ending ~= nil end
    if ok then
      N.done[b.id] = true
      completed[#completed + 1] = b.id
      said[#said + 1] = b.title
      if t == "divined" then N.divined = true end
      if t == "ending_chosen" then N.ending = ctx.ending end
    end
  end
  return completed, said
end

-- A rumour pointing to what comes next, from where someone stands.
function narrative.rumour(N, from)
  for _, m in ipairs(narrative.data().circle) do
    local site = N.sites[m.id]
    if site and not N.remnants[m.id] then
      return "They say " .. m.role .. " lives near " .. site.name .. ", " .. npcs.directions(from, site.pos) .. "."
    end
  end
  return nil
end

-- An heir who joins after the prologue meets the others at a reunion: the
-- nearest settlement still standing ahead of the heirs.
function narrative.reunion(N, db, heir, pos)
  local best, bd
  for _, st in ipairs(db.settlement) do
    if not st.destroyed and not st.ruined and not st.story then
      local d = world.distance(pos, st.pos)
      if not bd or d < bd then best, bd = st, d end
    end
  end
  if best then N.reunions[tostring(heir)] = { at = best.id, name = best.name } end
  return best
end

return narrative
