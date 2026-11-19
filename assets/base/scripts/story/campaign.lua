-- SPDX-License-Identifier: MIT
-- Azhdar's campaign (concept 1.5, 1.7): a destruction timeline in the
-- global simulation, paced by the heirs' progress. Every so many days
-- (sooner the further the story has come) Azhdar strikes: a settlement
-- burns and its land is blighted, its people take to the roads as
-- refugees; where a Circle member kept a Weave the heirs have not secured,
-- he takes it. Each strike weakens the ley network a little. Being slow
-- never fails the story; the world only grows darker.
local history = require("people.history")

local campaign = {}

campaign.INTERVAL = 4 * 86400  -- seconds between strikes at the start of Act II
campaign.BLIGHT = 0.3          -- blight a strike leaves on its region (0 .. 1)
campaign.LEY_LOSS = 0.02       -- what each strike takes from the ley network
campaign.REACH = 1500000       -- metres from his origin within which he strikes

function campaign.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { targets = saved.targets, next = saved.next, struck = saved.struck or {}, refugees = saved.refugees or {},
    blight = saved.blight or {}, taken = saved.taken or {} }
end

function campaign.snapshot(C)
  return { targets = C.targets, next = C.next, struck = C.struck, refugees = C.refugees, blight = C.blight,
    taken = C.taken }
end

-- Plans the campaign: settlements by distance from Azhdar's origin (the
-- nearest first, Anarvan and story places spared), with the Circle's sites
-- among them (every third strike goes for a Weave).
function campaign.plan(C, db, origin, circle_sites, now)
  if C.targets then return C.targets end
  local list = {}
  for _, st in ipairs(db.settlement) do
    if not st.story and not st.ruined and st.type ~= "city" then
      local d = world.distance(origin, st.pos)
      if d < campaign.REACH then list[#list + 1] = { id = st.id, d = d } end
    end
  end
  table.sort(list, function(a, b) return a.d < b.d or (a.d == b.d and a.id < b.id) end)
  local keys = {}
  for k in pairs(circle_sites or {}) do keys[#keys + 1] = k end
  table.sort(keys)
  local targets, k = {}, 1
  for i, t in ipairs(list) do
    if i % 3 == 0 and keys[k] then
      targets[#targets + 1] = { circle = keys[k] }
      k = k + 1
    end
    targets[#targets + 1] = { id = t.id }
    if #targets >= 40 then break end
  end
  while keys[k] do targets[#targets + 1] = { circle = keys[k] }; k = k + 1 end
  C.targets, C.next = targets, now + campaign.INTERVAL
  return targets
end

-- Strikes when due. progress: beats done (the further, the sooner).
-- N: the narrative (its Weaves); ctx: { set_ley(strength), ley (now) }.
-- Returns what happened this time (or nil).
function campaign.tick(C, db, N, now, progress, ctx)
  if not C.targets or now < C.next then return nil end
  C.next = now + campaign.INTERVAL / (1 + 0.5 * (progress or 0))
  local t = table.remove(C.targets, 1)
  if not t then return nil end
  ctx = ctx or {}
  local out
  if t.circle then
    local weave
    for _, m in ipairs(require("story.narrative").data().circle) do
      if m.id == t.circle then weave = m.weave end
    end
    if N.weaves[weave] ~= "heirs" then
      N.weaves[weave] = "azhdar"
      C.taken[#C.taken + 1] = weave
      local name = require("story.narrative").weave_name(weave)
      out = { kind = "weave", weave = weave, text = "Azhdar has taken " .. name }
    else
      out = { kind = "foiled", weave = weave, text = "Azhdar came for a Weave the heirs had secured" }
    end
  else
    local st = history.get(db, t.id)
    st.destroyed, st.ruined, st.fall_cause, st.population = true, true, "the Ashen", 0
    C.struck[#C.struck + 1] = st.id
    C.blight[tostring(st.cell)] = math.min(1, (C.blight[tostring(st.cell)] or 0) + campaign.BLIGHT)
    history.event(db, { year = db.year, kind = "burning", participants = { st.id }, place = st.id, importance = 0.8,
      title = "Azhdar burns " .. st.name })
    local refuge -- the nearest place still standing
    for _, o in ipairs(db.settlement) do
      if not o.destroyed and not o.ruined and o ~= st and (not refuge
        or world.distance(o.pos, st.pos) < world.distance(refuge.pos, st.pos)) then refuge = o end
    end
    if refuge then C.refugees[#C.refugees + 1] = { from = st.id, to = refuge.id, at = now } end
    out = { kind = "burnt", settlement = st.id, text = "Smoke over " .. st.name .. ": the Ashen were there" }
  end
  if ctx.set_ley then ctx.set_ley(math.max(0.2, (ctx.ley or 1) - campaign.LEY_LOSS)) end
  return out
end

-- How blighted a region is (0 .. 1).
function campaign.blight_at(C, cell_id)
  return C.blight[tostring(cell_id)] or 0
end

-- What people say of the campaign: the latest burnings.
function campaign.rumours(C, db)
  local out = {}
  for i = #C.struck, math.max(1, #C.struck - 2), -1 do
    out[#out + 1] = "They say " .. history.get(db, C.struck[i]).name .. " burned, and the ground is grey there."
  end
  return out
end

return campaign
