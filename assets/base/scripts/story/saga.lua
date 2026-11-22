-- SPDX-License-Identifier: MIT
-- The story after the prologue in the game (Acts II and III and after):
-- the narrative director anchors the Circle's remnants, Azhdar's campaign
-- begins with the burning that leaves the trail of ash, rival mages live
-- their days, rumours point the way. Divination (the ritual) reveals the
-- drifting citadel; ready for the storm, the heirs face Azhdar in a duel
-- fought with their own casts; then they choose an ending (F5 shows the
-- next, use takes it), and the world goes on after it.
local narrative = require("story.narrative")
local campaign = require("story.campaign")
local rivals = require("story.rivals")
local finale = require("story.finale")
local aftermath = require("story.aftermath")
local duel = require("story.duel")
local history = require("people.history")

local saga = {}

saga.CITADEL = 300   -- metres from the citadel at which the duel begins
saga.RIVAL = 100     -- metres from a rival's sanctum at which they are met

function saga.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { N = narrative.new(saved.narrative), C = campaign.new(saved.campaign), ending = saved.ending,
    years = saved.years or 0, lines = {}, met = saved.met or {}, choice = 1, ending_seen = saved.ending_seen }
end

function saga.snapshot(S)
  return { narrative = narrative.snapshot(S.N), campaign = campaign.snapshot(S.C), ending = S.ending,
    years = S.years, met = S.met, ending_seen = S.ending_seen }
end

local function say(S, text)
  S.lines[#S.lines + 1] = text
  if #S.lines > 6 then table.remove(S.lines, 1) end
end

-- Begins (or resumes) the story in a peopled world. P: places state.
function saga.begin(S, P, now)
  local db, grid = P.db, P.grid
  local home = history.get(db, db.story.anarvan).pos
  narrative.anchor(S.N, db, grid, home)
  local azhdar = history.get(db, db.story.azhdar)
  local origin = azhdar.home and history.get(db, azhdar.home).pos or home
  local fresh = S.C.targets == nil
  campaign.plan(S.C, db, origin, S.N.sites, now)
  S.R = rivals.generate(db, narrative.data())
  S.nexus = mana.ley(home).nexus -- (the citadel drifts round the nexus nearest the heirs' home)
  if fresh then -- the first burning: the trail of ash begins
    S.C.next = now
    local ev = campaign.tick(S.C, db, S.N, now, 0, {})
    if ev then say(S, ev.text) end
  end
  if #S.C.struck > 0 then S.trail = history.get(db, S.C.struck[1]).pos end
  return S
end

-- A tick of the story. ctx: { P, pos (an heir), now, heirs_power, ritual
-- (tier), void, ash_ward, ally, calm, cast (this tick's cast: colours,
-- power), hurt(harm, colour), learn(discipline), choose (F5), take (use) }.
function saga.tick(S, ctx)
  local P = ctx.P
  local db = P.db
  local done = narrative.count(S.N.done)
  local ev = campaign.tick(S.C, db, S.N, ctx.now, done, { set_ley = mana.set_ley_strength,
    ley = mana.ley(ctx.pos).strength })
  if ev then say(S, ev.text) end
  local day = math.floor(ctx.now / 86400)
  if S.day and day > S.day then rivals.tick(S.R, db, P.grid, day - S.day) end
  S.day = day
  -- Rivals met near their sanctums.
  local met_now = false
  for _, m in ipairs(S.R.list) do
    local st = history.get(db, m.sanctum)
    if not S.met[m.id] and st and world.distance(ctx.pos, st.pos) < saga.RIVAL then
      S.met[m.id] = rivals.meet(m, ctx.heirs_power or 50)
      met_now = true
      say(S, m.name .. " (" .. m.personality .. ") is " .. S.met[m.id])
    end
  end
  -- The citadel and the duel.
  if S.N.divined and not S.citadel then S.citadel = finale.citadel_at(S.nexus, ctx.now) end
  local ready = finale.ready({ ritual = ctx.ritual, ash_ward = ctx.ash_ward, ally = ctx.ally })
  if S.N.done.storm and not S.duel and not S.N.defeated then
    S.citadel = finale.citadel_at(S.nexus, ctx.now)
    if world.distance(ctx.pos, S.citadel) < saga.CITADEL then
      S.duel = finale.duel()
      say(S, "Azhdar, the Ash Weaver, turns to face you")
    end
  end
  if S.duel and not S.duel.over then
    if ctx.cast then duel.cast(S.duel, ctx.cast, { calm = ctx.calm }) end
    duel.tick(S.duel, ctx.dt or 1 / 60, ctx.hurt or function() end)
    if S.duel.won then
      S.N.defeated = true
      say(S, "Azhdar falls. Choose: F5 shows the endings, use takes one")
    end
  end
  -- The ending, chosen.
  local ending
  if S.N.defeated and not S.ending then
    local open = finale.endings({ ritual = ctx.ritual, void = ctx.void })
    if ctx.choose then
      S.choice = S.choice % #open + 1
      say(S, narrative.data().endings[open[S.choice]].name)
    end
    if ctx.take then
      ending = open[math.min(S.choice, #open)]
      S.ending = ending
      S.chronicles = aftermath.apply(ending, { db = db, economy = ctx.economy, set_ley = mana.set_ley_strength,
        ley = mana.ley(ctx.pos).strength })
      for _, c in pairs(S.chronicles) do S.last_chronicle = c end
      say(S, narrative.data().endings[ending].text)
    end
  end
  local completed, said = narrative.tick(S.N, { pos = ctx.pos, trail = S.trail, rival_met = met_now,
    divined = S.N.divined, ready = ready == true, defeated = S.N.defeated, ending = ending or S.ending,
    learn = ctx.learn })
  for _, l in ipairs(said) do say(S, l) end
  -- After the ending the years go by.
  if S.ending then
    local year = math.floor(ctx.now / (360 * 86400))
    if S.year and year > S.year then
      aftermath.year(S.ending, { set_ley = mana.set_ley_strength, ley = mana.ley(ctx.pos).strength })
      S.years = S.years + 1
    end
    S.year = year
  end
  return completed
end

-- Divination done (the ritual): the citadel's place, or why not.
function saga.divine(S, now, ritual_tier)
  local p, why = finale.divine(S.N, S.nexus, now, ritual_tier)
  if p then
    S.citadel = p
    say(S, "In the smoke of the circle you see the citadel, drifting on the storm")
  else
    say(S, why)
  end
  return p
end

-- What the heirs should do now, and rumours of it.
function saga.hud_lines(S, pos)
  local out = {}
  local open = narrative.open(S.N)
  if open[1] then out[#out + 1] = "Story: " .. open[1].title end
  local r = narrative.rumour(S.N, pos)
  if r then out[#out + 1] = r end
  for _, l in ipairs(S.lines) do out[#out + 1] = l end
  return out
end

return saga
