-- SPDX-License-Identifier: MIT
-- Survival (concept 7.2): food, water, rest and warmth, and the conditions
-- that come from the land: sickness from dirty water, spoilt food, raw meat
-- and mosquitoes; poison; being soaked. Needs run in game time and drain
-- faster with exertion, heat, cold and sickness; when one runs out it hurts.
-- A difficulty preset scales it all: wanderer (slow, no disease), survivor
-- (the default) and hermit (harsh).
local news = require("player.news")

local survival = {}

survival.DAY = 86400
survival.PRESETS = {
  wanderer = { drain = 0.5, disease = 0, exposure = 0.5 },
  survivor = { drain = 1.0, disease = 1, exposure = 1.0 },
  hermit = { drain = 1.5, disease = 1.5, exposure = 1.5 },
}
survival.FOOD_KCAL = 2000     -- kcal that make a full stomach (a day at rest)
survival.WATER_L = 2.5        -- litres that make a full drink (a day at rest)
survival.AWAKE_HOURS = 16     -- hours awake from rested to exhausted
survival.SLEEP_HOURS = 8      -- hours of sleep from exhausted to rested
survival.CORE = 37            -- deg C of a body at ease
survival.CHILL = 35.5         -- below this it is too cold
survival.FEVERISH = 39        -- above this it is too hot
survival.FIRE_WARMTH = 18     -- deg C a fire close by adds to the air
survival.CLOTHES = 10         -- deg C the mage's clothes are worth
survival.CLOAK = 20           -- deg C the carpet worn as a cloak is worth, dry
survival.COMFORT = 25         -- deg C felt a body seeks: clothes are shed and a fire sat back from above it
survival.SICK_DAYS = 4        -- days a sickness lasts untreated

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

function survival.new(saved, preset)
  saved = type(saved) == "table" and saved or {}
  local s = { food = saved.food or 80, water = saved.water or 80, rest = saved.rest or 90,
    core = saved.core or survival.CORE, wet = saved.wet or 0, sick = saved.sick or 0, poison = saved.poison or 0,
    peaked = saved.peaked, treated = saved.treated,
    seed = saved.seed or 12345, preset = saved.preset or preset or "survivor", news = news.new() }
  return s
end

function survival.snapshot(s)
  return { food = s.food, water = s.water, rest = s.rest, core = s.core, wet = s.wet, sick = s.sick,
    poison = s.poison, peaked = s.peaked, treated = s.treated, seed = s.seed, preset = s.preset }
end

local function rules(s) return survival.PRESETS[s.preset] or survival.PRESETS.survivor end

-- A deterministic roll 0 .. 1 (saved with the state).
local function roll(s)
  s.seed = (s.seed * 1103515245 + 12345) % 2147483648
  return s.seed / 2147483648
end

local function say(s, text)
  news.say(s.news, text)
end

-- Real time passes: old news goes.
function survival.tick_news(s, dt)
  news.tick(s.news, dt)
end

-- Catches a sickness with a chance (scaled by the preset).
local function catch(s, chance, what)
  if rules(s).disease > 0 and roll(s) < chance * rules(s).disease then
    if s.sick < 0.2 then say(s, "You feel sick (" .. what .. ")") end
    s.sick = math.max(s.sick, 0.2)
    return true
  end
  return false
end

-- The air a body feels, deg C: the weather's, colder high up, in wind and
-- when wet, warmer in shelter, and warmer in clothes and by a fire as far as
-- the body wants (up to COMFORT: in the heat clothes are shed and a fire is
-- sat back from). ctx.cloak (0 .. 1: how much of its worth the carpet worn
-- as a cloak gives) adds up to CLOAK more and halves the wind's chill.
function survival.feels(ctx)
  local air = (ctx.air or 15) - 6.5 * math.max(ctx.height or 0, 0) / 1000
  local wind = math.max((ctx.wind or 0) - 1, 0) * 0.7 * ((ctx.cloak or 0) > 0 and 0.5 or 1)
  local felt = air - wind - (ctx.wet or 0) * 8 + (ctx.shelter and 6 or 0)
  felt = felt + math.min(survival.CLOTHES, math.max(survival.COMFORT - felt, 0))
  felt = felt + math.min(survival.CLOAK * (ctx.cloak or 0), math.max(survival.COMFORT - felt, 0))
  if ctx.fire then felt = felt + math.min(survival.FIRE_WARMTH, math.max(survival.COMFORT - felt, 0)) end
  return felt
end

-- The core temperature a body settles at in air that feels like this.
local function settles_at(feels, effort)
  local t = survival.CORE + effort * 0.6
  if feels < 20 then t = t - (20 - feels) / 8 end
  if feels > 32 then t = t + (feels - 32) / 4 end
  return t
end

-- Time passes: seconds of game time. ctx: air (deg C), wind (m/s), rain
-- (mm/h), height (m above the ground), effort (0 still, 1 walking, 2
-- running), fire (a fire close by), in_water (m deep where it stands),
-- shelter, bites (mosquito bites a second). Returns the harms to deal:
-- { { harm, amount } }.
function survival.tick(s, seconds, ctx)
  local r = rules(s)
  local days = seconds / survival.DAY
  local effort = ctx.effort or 0
  -- Wet: rain and water soak, warmth and air dry.
  if (ctx.in_water or 0) > 0.3 then
    s.wet = 1
  elseif (ctx.rain or 0) > 0.2 and not ctx.shelter and (ctx.cloak or 0) <= 0 then -- (a cloak keeps off rain)
    s.wet = math.min(s.wet + seconds / 1800, 1)
  else
    s.wet = math.max(s.wet - seconds / (ctx.fire and 900 or 7200), 0)
  end
  local feels = survival.feels({ air = ctx.air, wind = ctx.wind, wet = s.wet, height = ctx.height, fire = ctx.fire,
    shelter = ctx.shelter, cloak = ctx.cloak })
  local target = settles_at(feels, effort)
  s.core = s.core + (target - s.core) * clamp(seconds / 7200, 0, 1)
  local hot, cold = s.core > survival.FEVERISH, s.core < survival.CHILL
  -- Needs drain: a day's worth at rest, faster with effort, cold, heat, sickness.
  local burn = (1 + 0.4 * effort + (cold and 0.5 or 0)) * r.drain
  local dry = (1 + 0.5 * effort + (hot and 1 or 0) + s.sick * 0.6) * r.drain
  local tire = (1 + 0.5 * effort + s.sick * 0.5 + (cold and 0.5 or 0)) * r.drain
  s.food = clamp(s.food - 100 * days * burn, 0, 100)
  s.water = clamp(s.water - 100 * days * dry, 0, 100)
  s.rest = clamp(s.rest - 100 * seconds / (survival.AWAKE_HOURS * 3600) * tire, 0, 100)
  -- Mosquitoes carry fever.
  if (ctx.bites or 0) > 0 then catch(s, ctx.bites * seconds * 0.002, "fever") end
  -- A sickness runs its course: worse for the first hours (unless treated),
  -- then better over days.
  if s.sick > 0 then
    if not s.peaked and not s.treated then
      s.sick = math.min(s.sick + days * 2, 0.6)
      s.peaked = s.sick >= 0.6 or nil
    else
      s.sick = math.max(s.sick - days * 0.6 / survival.SICK_DAYS, 0)
    end
    if s.sick == 0 then
      s.treated, s.peaked = nil, nil
      say(s, "You feel better")
    end
  end
  s.poison = math.max(s.poison - days * 0.5, 0)
  -- What hurts.
  local hours = seconds / 3600
  local harms = {}
  local function hurt(harm, per_hour)
    if per_hour > 0 then harms[#harms + 1] = { harm, per_hour * hours * r.exposure } end
  end
  if s.food <= 0 then hurt("poison", 1.5) end -- starving
  if s.water <= 0 then hurt("poison", 4) end -- parched
  if s.core < 34 then hurt("cold", 8) elseif cold then hurt("cold", 2) end
  if hot then hurt("heat", 3) end
  hurt("poison", s.sick * 1.5 + s.poison * 6)
  return harms
end

-- What the body feels at pos: the air, wind and rain, its height above the
-- ground, effort (from walker w, on foot, or the carpet), a fire within a
-- few metres, the water it stands in, and bites (a second; the smoke of a
-- fire close by keeps the mosquitoes off).
function survival.sense(pos, w, bites)
  local weather = sim.weather()
  local wind = sim.wind_at(pos)
  local ground = world.ground_height(pos)
  local fire = false
  for _, d in ipairs({ { 0, 0 }, { 0, 3 }, { 1.6, 3 }, { 3.1, 3 }, { 4.7, 3 } }) do
    local cell = sim.fire_at(world.move(pos, d[1], d[2], 0))
    fire = fire or (cell.loaded and (cell.burning or cell.temperature > 60))
  end
  local water = w and sim.water_at(pos) or nil
  local speed = w and math.abs(w.speed) or 0
  return { air = weather.temperature, wind = wind.speed or 0, rain = weather.rain, height = w and 0 or pos.alt - ground,
    effort = speed > 3 and 2 or (speed > 0.5 and 1 or 0), fire = fire,
    in_water = water and water.loaded and not water.frozen and water.depth or 0, bites = fire and 0 or bites or 0 }
end

-- Eats or drinks an item (its definition from data/items.lua) with its
-- microbe load; returns what was said.
function survival.consume(s, item, load)
  local nausea = s.sick > 0.3 and 0.5 or 1
  s.food = clamp(s.food + (item.kcal or 0) / survival.FOOD_KCAL * 100 * nausea, 0, 100)
  s.water = clamp(s.water + (item.water or 0) / survival.WATER_L * 100 * (item.salty and -1 or 1), 0, 100)
  if item.salty then say(s, "The salt water makes you thirstier") end
  load = load or 0
  if load > 0.9 then catch(s, 0.9, "rotten food")
  elseif load > 0.5 then catch(s, load, "spoilt food") end
  if (item.dirty or 0) > 0 then catch(s, item.dirty * 0.6, item.name or "dirty water") end
  if item.raw then catch(s, item.raw, "raw food") end
  if (item.poison or 0) > 0 then
    s.poison = math.min(s.poison + item.poison, 1)
    say(s, "You feel poisoned")
  end
  local cures = item.cures or {}
  if cures.sick and s.sick > 0 then
    s.sick = math.max(s.sick - cures.sick, 0)
    s.treated = true
    say(s, s.sick > 0 and "The remedy helps" or "You feel better")
  end
  if cures.poison and s.poison > 0 then s.poison = math.max(s.poison - cures.poison, 0) end
  if (item.rest or 0) > 0 then s.rest = clamp(s.rest + item.rest, 0, 100) end
end

-- Sleeps up to hours (or until rested) in ctx; returns the hours slept and
-- the harms of the night. Time is the caller's to move on.
function survival.sleep(s, hours, ctx)
  local slept, harms = 0, {}
  while slept < hours and s.rest < 99 do
    local quiet = { air = ctx.air, wind = ctx.wind, rain = ctx.rain, fire = ctx.fire, shelter = ctx.shelter,
      cloak = ctx.cloak, effort = 0, height = 0 }
    for _, h in ipairs(survival.tick(s, 3600, quiet)) do harms[#harms + 1] = h end
    -- Asleep: the body asks less, and rest comes back.
    s.food = clamp(s.food + 100 / 24 * 0.3, 0, 100)
    s.water = clamp(s.water + 100 / 24 * 0.3, 0, 100)
    s.rest = clamp(s.rest + 100 / survival.SLEEP_HOURS + 100 / survival.AWAKE_HOURS, 0, 100)
    slept = slept + 1
  end
  return slept, harms
end

local function word(v)
  return v > 60 and "well" or v > 30 and "low" or v > 10 and "very low" or "out"
end

function survival.hud_lines(s)
  local lines = { string.format("Food %s, water %s, rest %s, %.1f C", word(s.food), word(s.water), word(s.rest),
    s.core) }
  local conditions = {}
  if s.sick > 0 then conditions[#conditions + 1] = "sick" end
  if s.poison > 0 then conditions[#conditions + 1] = "poisoned" end
  if s.wet > 0.3 then conditions[#conditions + 1] = "soaked" end
  if s.core < survival.CHILL then conditions[#conditions + 1] = "freezing" end
  if s.core > survival.FEVERISH then conditions[#conditions + 1] = "overheated" end
  if #conditions > 0 then lines[#lines + 1] = "You are " .. table.concat(conditions, ", ") end
  for _, text in ipairs(news.lines(s.news)) do lines[#lines + 1] = text end
  return lines
end

return survival
