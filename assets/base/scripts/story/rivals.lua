-- SPDX-License-Identifier: MIT
-- Rival mages and domains (concept 7.6): mages generated from the world's
-- mage towers, each with a personality (greedy, scholarly, territorial,
-- kind, mad), favoured colours and disciplines, a sanctum and a domain of
-- regions round it, servants, a familiar and a reputation; Azhdar's three
-- lieutenants among them. They live in the global simulation: territorial
-- and greedy ones claim more land, neighbours raid each other, the kind and
-- the scholarly trade with towns. Met, they are hostile, trade or teach by
-- their nature; the heirs may make a truce, become apprentices or allies -
-- and a greedy ally may betray them when Azhdar grows strong. Contests are
-- duels. Heirs who win a sanctum hold its domain: mana every day and
-- servants.
local rng = require("people.rng")
local history = require("people.history")
local language = require("people.language")
local peopling = require("people.peopling")
local duel = require("story.duel")

local rivals = {}

rivals.PERSONALITIES = { "greedy", "scholarly", "territorial", "kind", "mad" }
rivals.COLOURS = { "fire", "storm", "light", "life", "water", "earth", "void", "decay" }
rivals.DISCIPLINES = { "alchemy", "artificing", "ritual", "summoning", "spellcasting" }

-- The mages of the world.
function rivals.generate(db, story)
  local out = {}
  local s = rng.new(db.seed, "rivals")
  for _, st in ipairs(db.settlement) do
    if st.type == "mage_tower" and not st.ruined then
      local lang = peopling.speech(db, st.culture)
      local p = s:pick(rivals.PERSONALITIES)
      out[#out + 1] = { id = "mage:" .. st.id, name = language.name(lang, "person", "mage" .. st.id), personality = p,
        colours = { s:pick(rivals.COLOURS), s:pick(rivals.COLOURS) }, discipline = s:pick(rivals.DISCIPLINES),
        sanctum = st.id, domain = { [st.cell] = true }, servants = s:int(1, 6), familiar = s:pick({ "owl", "fox",
          "serpent", "hawk" }), reputation = 0, power = 40 + s:int(0, 60), relation = "stranger" }
    end
  end
  for _, site in ipairs(db.site) do -- free mages hold the elder ruins at the nexuses
    if site.kind == "elder ruin" then
      local cu = db.cell_culture[site.cell] or db.culture[1].id
      local lang = peopling.speech(db, cu)
      out[#out + 1] = { id = "mage:" .. site.id, name = language.name(lang, "person", "mage" .. site.id),
        personality = s:pick(rivals.PERSONALITIES), colours = { s:pick(rivals.COLOURS), s:pick(rivals.COLOURS) },
        discipline = s:pick(rivals.DISCIPLINES), sanctum = site.id, domain = { [site.cell] = true },
        servants = s:int(1, 6), familiar = s:pick({ "owl", "fox", "serpent", "hawk" }), reputation = 0,
        power = 50 + s:int(0, 70), relation = "stranger" }
    end
  end
  for i, l in ipairs(story and story.lieutenants or {}) do -- Azhdar's lieutenants
    local home = db.settlement[(i * 37) % #db.settlement + 1]
    out[#out + 1] = { id = "lieutenant:" .. l.id, name = l.name, personality = l.personality, colours = l.colours,
      discipline = "spellcasting", sanctum = home.id, domain = { [home.cell] = true }, servants = 8,
      familiar = "ash hound",
      reputation = -50, power = 120 + i * 20, relation = "enemy", serves_azhdar = true }
  end
  return { list = out, days = 0, claims = {} }
end

local function owner_of(R, cell)
  for _, m in ipairs(R.list) do
    if m.domain[cell] then return m end
  end
end

-- Days pass: domains grow, neighbours raid, some trade with towns.
function rivals.tick(R, db, grid, days)
  local events = {}
  for _ = 1, days do
    R.days = R.days + 1
    for _, m in ipairs(R.list) do
      if not m.defeated then
        local s = rng.new(db.seed, m.id, R.days)
        if (m.personality == "territorial" or m.personality == "greedy" or m.serves_azhdar) and R.days % 10 == 0 then
          local cells = {}
          for cell in pairs(m.domain) do cells[#cells + 1] = cell end
          table.sort(cells) -- grow from the first cell of the domain, the same way every run
          for _, nid in ipairs(cells[1] and grid.cells[cells[1]].nb or {}) do
            if grid.cells[nid].land and not owner_of(R, nid) and s:chance(0.5) then
              m.domain[nid] = true
              events[#events + 1] = { kind = "claim", mage = m.id }
              break
            end
          end
        end
        if R.days % 15 == 0 then
          for _, o in ipairs(R.list) do
            if o ~= m and not o.defeated and m.personality ~= "kind" then
              for cell in pairs(m.domain) do
                for _, nid in ipairs(grid.cells[cell].nb) do
                  if o.domain[nid] and s:chance(0.3) then -- a raid
                    local win = m.power * s:float() > o.power * s:float()
                    local loser = win and o or m
                    loser.servants = math.max(0, loser.servants - 1)
                    events[#events + 1] = { kind = "raid", mage = m.id, on = o.id, won = win }
                  end
                end
              end
            end
          end
        end
        if (m.personality == "kind" or m.personality == "scholarly") and R.days % 7 == 0 then
          local st = history.get(db, m.sanctum)
          st.prosperity = math.min(1, (st.prosperity or 0.5) + 0.01)
          events[#events + 1] = { kind = "trade", mage = m.id }
        end
      end
    end
  end
  return events
end

-- Meeting a mage: how they take the heirs (by nature, and the heirs' power
-- against theirs). Returns "hostile", "trade" or "teach".
function rivals.meet(m, heirs_power)
  if m.serves_azhdar or m.personality == "mad" then return "hostile" end
  if m.personality == "territorial" then return heirs_power > m.power and "trade" or "hostile" end
  if m.personality == "greedy" then return "trade" end
  return "teach"
end

-- Proposes a relation: "truce", "apprentice" or "alliance". Returns
-- whether it is taken.
function rivals.propose(m, kind, heirs_power)
  local mood = rivals.meet(m, heirs_power)
  local ok
  if kind == "truce" then ok = mood ~= "hostile" or heirs_power > m.power * 1.5
  elseif kind == "apprentice" then ok = mood == "teach"
  elseif kind == "alliance" then ok = not m.serves_azhdar and (mood ~= "hostile") and m.reputation >= 0
  else ok = false end
  if ok then m.relation = kind end
  return ok
end

-- An ally may betray the heirs when Azhdar grows strong (the greedy, once
-- he holds three Weaves or more).
function rivals.betrays(m, azhdar_weaves)
  if m.relation == "alliance" and m.personality == "greedy" and azhdar_weaves >= 3 then
    m.relation = "betrayer"
    return true
  end
  return false
end

-- A contest with a mage: a duel against their power and colours.
function rivals.contest(m)
  return duel.new({ name = m.name, hp = m.power, weak = { m.colours[1] == "fire" and "water" or "fire" },
    strong = { m.colours[1] }, attack = 4, every = 2, colour = m.colours[1] })
end

-- A won contest: the mage is defeated, and the heirs may hold the sanctum's
-- domain.
function rivals.defeat(R, m, claim)
  m.defeated = true
  if claim then
    R.heirs_domain = R.heirs_domain or { cells = {}, servants = 0 }
    for cell in pairs(m.domain) do R.heirs_domain.cells[cell] = true end
    R.heirs_domain.servants = R.heirs_domain.servants + m.servants
    R.heirs_domain.sanctum = m.sanctum
  end
end

-- What the heirs' domain gives a day: mana and the servants' work.
function rivals.domain_income(R)
  local d = R.heirs_domain
  if not d then return 0, 0 end
  local n = 0
  for _ in pairs(d.cells) do n = n + 1 end
  return n * 5, d.servants
end

return rivals
