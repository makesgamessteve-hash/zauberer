-- SPDX-License-Identifier: MIT
-- The six disciplines (concept 3.4): spellcasting, ritual magic, alchemy,
-- artificing, summoning and naturalism, each with tiers Novice, Apprentice,
-- Journeyman, Adept and Master. Mastery grows through practice (doing the
-- discipline: each cast, brew, enchantment, ritual, summoning or thing
-- learnt), study (Farrokh's books in the tent's study) and trials (what
-- must be done to pass into each tier, data/trials.lua): experience stops at
-- the next tier's threshold until its trial is passed. Mastery makes the
-- discipline's work better: richer yields, lower costs, stronger bindings.
local news = require("player.news")

local disciplines = {}

disciplines.LIST = { "spellcasting", "ritual", "alchemy", "artificing", "summoning", "naturalism" }
disciplines.TIERS = { "Novice", "Apprentice", "Journeyman", "Adept", "Master" }
disciplines.XP = { 0, 100, 300, 700, 1500 } -- experience at which each tier begins
disciplines.STUDY = 12 -- experience an hour of study

local trials
function disciplines.trials()
  if not trials then
    local text = assert(vfs.read("data/trials.lua"))
    trials = assert(load(text, "=data/trials.lua", "t", {}))()
  end
  return trials
end

local function known(d)
  for _, name in ipairs(disciplines.LIST) do
    if name == d then return true end
  end
  return false
end

function disciplines.new(saved)
  saved = type(saved) == "table" and saved or {}
  local s = { xp = {}, tier = {}, done = {}, news = news.new(), shown = false }
  for _, d in ipairs(disciplines.LIST) do
    local sv = type(saved[d]) == "table" and saved[d] or {}
    s.xp[d] = math.max(tonumber(sv.xp) or 0, 0)
    s.tier[d] = math.max(math.min(math.floor(tonumber(sv.tier) or 1), #disciplines.TIERS), 1)
    s.done[d] = { counts = {}, kinds = {} }
    for event, n in pairs(type(sv.counts) == "table" and sv.counts or {}) do
      s.done[d].counts[event] = tonumber(n) or 0
    end
    for event, list in pairs(type(sv.kinds) == "table" and sv.kinds or {}) do
      s.done[d].kinds[event] = {}
      for _, what in ipairs(type(list) == "table" and list or {}) do s.done[d].kinds[event][what] = true end
    end
  end
  return s
end

function disciplines.snapshot(s)
  local out = {}
  for _, d in ipairs(disciplines.LIST) do
    local kinds = {}
    for event, set in pairs(s.done[d].kinds) do
      local list = {}
      for what in pairs(set) do list[#list + 1] = what end
      table.sort(list)
      kinds[event] = list
    end
    out[d] = { xp = s.xp[d], tier = s.tier[d], counts = s.done[d].counts, kinds = kinds }
  end
  return out
end

-- Whether the trial into the next tier is passed.
function disciplines.trial_passed(s, d)
  local t = disciplines.trials()[d] and disciplines.trials()[d][s.tier[d]]
  if not t then return false end
  local count = s.done[d].counts[t.event] or 0
  local distinct = 0
  for _ in pairs(s.done[d].kinds[t.event] or {}) do distinct = distinct + 1 end
  return count >= (t.count or 0) and distinct >= (t.distinct or 0)
end

local function advance(s, d)
  while s.tier[d] < #disciplines.TIERS and s.xp[d] >= disciplines.XP[s.tier[d] + 1]
    and disciplines.trial_passed(s, d) do
    s.tier[d] = s.tier[d] + 1
    news.say(s.news, string.format("%s%s: %s", d:sub(1, 1):upper(), d:sub(2), disciplines.TIERS[s.tier[d]]), 6)
  end
  local cap = disciplines.XP[s.tier[d] + 1]
  if cap and s.xp[d] > cap then s.xp[d] = cap end -- (held at the threshold until the trial)
end

-- Practice: experience in a discipline for a kind of event (what: which one).
function disciplines.practice(s, d, xp, event, what)
  if not known(d) then return end
  s.xp[d] = s.xp[d] + xp
  if event then
    s.done[d].counts[event] = (s.done[d].counts[event] or 0) + 1
    s.done[d].kinds[event] = s.done[d].kinds[event] or {}
    s.done[d].kinds[event][what or event] = true
  end
  advance(s, d)
end

-- Study: hours with Farrokh's books on a discipline.
function disciplines.study(s, d, hours)
  disciplines.practice(s, d, disciplines.STUDY * hours)
end

-- How mastery betters the work: 1 at Novice, more with each tier (yields,
-- bindings) - or, for costs, less (cost = true).
function disciplines.factor(s, d, cost)
  local t = s and s.tier[d] or 1
  return cost and (1 - 0.08 * (t - 1)) or (1 + 0.1 * (t - 1))
end

function disciplines.hud_lines(s)
  local lines = news.lines(s.news)
  if s.shown then
    for _, d in ipairs(disciplines.LIST) do
      local next_xp = disciplines.XP[s.tier[d] + 1]
      local trial = ""
      if next_xp and s.xp[d] >= next_xp and not disciplines.trial_passed(s, d) then trial = " - trial awaits" end
      lines[#lines + 1] = string.format("%-12s %-10s %d%s%s", d, disciplines.TIERS[s.tier[d]], math.floor(s.xp[d]),
        next_xp and ("/" .. next_xp) or "", trial)
    end
  end
  return lines
end

function disciplines.tick(s, dt) news.tick(s.news, dt) end

return disciplines
