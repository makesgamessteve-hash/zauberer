-- SPDX-License-Identifier: MIT
-- "Rise and fall" (concept 10.6): the optional history simulation. From the
-- cradles, peoples grow towards what each region can feed (logistic
-- growth) and spill into the lands about; where enough people gather a
-- chiefdom forms, and polities grow by war along their borders (battles
-- weighed by numbers, terrain and fortune), sack cities, take tribute; big
-- or old ones split in succession crises and collapse; steppe peoples raid
-- the settled lands in bad years; plagues run along the roads, droughts
-- starve, mountains erupt; prophets arise in crises; rich, quiet realms
-- build great works. It runs in decades (DECADE years a step) and writes
-- into the same history database as the Settled world: the present-day
-- peoples, realms, ruins and the events that made them. Deterministic.
local rng = require("people.rng")
local history = require("people.history")
local job = require("people.job")
local language = require("people.language")

local simulation = {}

simulation.YEARS = 4000
simulation.DECADE = 10
simulation.GROWTH = 0.08      -- per decade, towards capacity
simulation.SPILL = 0.75       -- share of capacity at which people move on
simulation.CHIEFDOM = 400     -- people in a region for a chiefdom to form
simulation.MAX_REALMS = 40

local function capacity(c)
  if not c.land or c.class == "ice" then return 0 end
  return c.fertility * 1000 + (c.river and 300 or 0) + (c.oasis and 300 or 0) + (c.coast and 100 or 0) + 20
end
simulation.capacity = capacity

-- Runs the simulation on a peopled grid whose cultures have their cradles
-- (db.culture[i].cradle). Fills db.cell_culture and db.cell_realm, adds the
-- surviving polities and the events. opts: { years, conflict (0..2) }.
function simulation.run(db, grid, opts)
  opts = opts or {}
  local years = opts.years or simulation.YEARS
  local conflict = opts.conflict or 1
  local s = rng.new(db.seed, "rise and fall")
  local pop, culture, owner = {}, {}, {}
  local start = db.year - years
  local realms = {} -- live polities: { id, cells, capital, culture, founded, strength, age }
  local fallen = 0
  local ruin = {}
  db.cell_ruin = ruin
  for _, cu in ipairs(db.culture) do
    pop[cu.cradle] = 200
    culture[cu.cradle] = cu.id
    cu.settled = start
    history.event(db, { year = start, kind = "settling", participants = { cu.id }, place = cu.cradle,
      importance = 0.9, title = "the " .. cu.name .. " settle their homeland" })
  end
  local land_cells = {}
  for _, c in ipairs(grid.cells) do
    if capacity(c) > 0 then land_cells[#land_cells + 1] = c end
  end
  local function new_realm(cell, year)
    local cu = history.get(db, culture[cell.id])
    local lang = language.make(history.get(db, cu.language), require("people.peopling").data().family[cu.family],
      nil)
    local r = { key = #realms + 1, cells = 1, capital = cell.id, culture = cu.id, founded = year, age = 0,
      name = language.name(lang, "realm", "rise" .. cell.id .. year) }
    realms[r.key] = r
    owner[cell.id] = r.key
    history.event(db, { year = year, kind = "rise", participants = { cu.id }, place = cell.id, importance = 0.5,
      title = "the chiefdom of " .. r.name .. " is founded", realm_name = r.name })
    return r
  end
  db.wars = db.wars or {}
  for step = 0, years / simulation.DECADE - 1 do
    local year = start + step * simulation.DECADE
    job.pace(4)
    -- Demography: logistic growth and spilling over.
    local spill = {}
    for _, c in ipairs(land_cells) do
      local p = pop[c.id]
      if p and p > 0 then
        local K = capacity(c)
        p = p + simulation.GROWTH * p * (1 - p / K)
        pop[c.id] = p
        if p > K * simulation.SPILL then
          for _, nid in ipairs(c.nb) do
            local o = grid.cells[nid]
            if capacity(o) > 0 and (pop[nid] or 0) < capacity(o) * 0.3 then
              spill[#spill + 1] = { nid, p * 0.05, culture[c.id] }
            end
          end
        end
      end
    end
    for _, sp in ipairs(spill) do
      if not pop[sp[1]] or pop[sp[1]] < 1 then culture[sp[1]] = sp[3] end
      pop[sp[1]] = (pop[sp[1]] or 0) + sp[2]
    end
    -- Chiefdoms where people gather.
    if #realms - fallen < simulation.MAX_REALMS then
      for _, c in ipairs(land_cells) do
        if (pop[c.id] or 0) > simulation.CHIEFDOM and not owner[c.id] and s:chance(0.02) then
          new_realm(c, year)
        end
      end
    end
    -- Realms grow: peaceful absorption of unruled neighbours, war with ruled ones.
    for _, r in ipairs(realms) do
      if not r.dead then
        r.age = r.age + simulation.DECADE
        local border = {}
        for _, c in ipairs(land_cells) do
          if owner[c.id] == r.key then
            for _, nid in ipairs(c.nb) do
              if owner[nid] ~= r.key and (pop[nid] or 0) > 20 then border[#border + 1] = { c.id, nid } end
            end
          end
        end
        if #border > 0 then
          local b = border[s:int(1, #border)]
          local target = owner[b[2]]
          if not target then
            if s:chance(0.5) then owner[b[2]] = r.key; r.cells = r.cells + 1 end
          elseif s:chance(0.04 * conflict) then -- war
            local enemy = realms[target]
            local mine = r.cells * (1 + s:float()) + (grid.cells[b[1]].relief > 300 and 1 or 0)
            local theirs = enemy.cells * (1 + s:float()) + (grid.cells[b[2]].relief > 300 and 2 or 0)
            local win = mine >= theirs
            local winner, loser = win and r or enemy, win and enemy or r
            local field = win and b[2] or b[1]
            local war = history.event(db, { year = year, kind = "war", participants = {}, place = field,
              importance = 0.75, cause_text = s:pick({ "land", "succession", "faith", "revenge", "raids" }),
              winner_name = winner.name, loser_name = loser.name, winner_key = winner.key, loser_key = loser.key,
              title = "the war of " .. r.name .. " and " .. enemy.name })
            war.battle_year, war.fallen = year + s:int(0, 3), s:int(5, 80) * 100
            db.wars[#db.wars + 1] = war.id
            if win then
              owner[b[2]] = r.key
              r.cells, enemy.cells = r.cells + 1, enemy.cells - 1
              if b[2] == enemy.capital or s:chance(0.2) then -- the city is sacked
                ruin[b[2]] = "war"
                pop[b[2]] = (pop[b[2]] or 0) * 0.3
                history.event(db, { year = year, kind = "sack", participants = {}, place = b[2], importance = 0.6,
                  cause = war.id, title = "the sack of a city of " .. enemy.name })
              end
            end
          end
        end
        -- Collapse: overextension, succession crises.
        local strain = r.cells / 40 + r.age / 1500
        if s:chance(0.01 * strain) then
          r.dead = true
          fallen = fallen + 1
          history.event(db, { year = year, kind = "collapse", participants = {}, place = r.capital, importance = 0.7,
            title = r.name .. " falls apart", realm_name = r.name })
          for _, c in ipairs(land_cells) do
            if owner[c.id] == r.key then owner[c.id] = nil end
          end
        elseif r.cells > 8 and r.age > 200 and s:chance(0.01) then
          history.event(db, { year = year, kind = "great work", participants = {}, place = r.capital,
            importance = 0.5, title = r.name .. " raises " .. s:pick({ "a great library", "an observatory",
              "a stone bridge", "a canal", "a palace of four gardens" }) })
        end
      end
    end
    -- Nomads, plague, drought, fire mountains, prophets.
    if s:chance(0.03 * conflict) then
      local c = land_cells[s:int(1, #land_cells)]
      if c.class == "steppe" and pop[c.id] then
        history.event(db, { year = year, kind = "nomad raid", participants = {}, place = c.id, importance = 0.5,
          title = "riders from the steppe fall upon the settled lands" })
        for _, nid in ipairs(c.nb) do
          if pop[nid] then pop[nid] = pop[nid] * 0.7; ruin[nid] = ruin[nid] or "war" end
        end
      end
    end
    if s:chance(0.02) then
      local c = land_cells[s:int(1, #land_cells)]
      if pop[c.id] then
        history.event(db, { year = year, kind = "plague", participants = {}, place = c.id, importance = 0.55,
          title = "a plague comes with the caravans" })
        pop[c.id] = pop[c.id] * 0.5
        for _, nid in ipairs(c.nb) do if pop[nid] then pop[nid] = pop[nid] * 0.7 end end
        if s:chance(0.4) then
          history.event(db, { year = year + 1, kind = "revelation", participants = {}, place = c.id, importance = 0.6,
            title = "a prophet rises in the time of plague" })
        end
      end
    end
    if s:chance(0.03) then
      local c = land_cells[s:int(1, #land_cells)]
      if c.class == "desert" or c.class == "steppe" then
        history.event(db, { year = year, kind = "famine", participants = {}, place = c.id, importance = 0.3,
          title = "the wells run dry" })
        if pop[c.id] then pop[c.id] = pop[c.id] * 0.6 end
        if (pop[c.id] or 0) < 5 then ruin[c.id] = "abandonment" end
      elseif c.relief > 900 then
        history.event(db, { year = year, kind = "eruption", participants = {}, place = c.id, importance = 0.55,
          title = "the mountain wakes in fire" })
        ruin[c.id] = "magic"
      end
    end
  end
  -- The present: who lives where, who rules.
  db.cell_culture, db.cell_settled, db.cell_realm = {}, {}, {}
  for _, c in ipairs(land_cells) do
    if (pop[c.id] or 0) > 5 then
      db.cell_culture[c.id] = culture[c.id]
      db.cell_settled[c.id] = math.max(0, (capacity(c) - pop[c.id]) / 50)
    end
  end
  db.sim = { realms = realms, owner = owner, pop = pop, fallen = fallen }
  return db
end

return simulation
