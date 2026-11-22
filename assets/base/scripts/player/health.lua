-- SPDX-License-Identifier: MIT
-- The player's health (concept 5.13): one pool hurt through the same harms as
-- the creatures (heat, impact, cold, poison, smoke, magic). Flames and heat
-- under the player, thick smoke, bites, hard landings and weaves going off
-- in the hand hurt; healing weaves and rest mend. A ward of the right colour
-- turns most of its harm aside. At no health the player is defeated, and
-- after a moment gets up again where the carpet first set out.
local health = {}

health.MAX = 100
health.REGEN = 0.5        -- per second once unharmed for a while
health.REGEN_DELAY = 10   -- seconds
health.BITE = 12          -- a wolf's bite
health.FALL_SAFE = 8      -- m/s: softer landings do no harm
health.FALL_HARM = 6      -- per m/s faster than that
health.BURN = 20          -- per second standing in flames
health.SMOKE_THICK = 0.005 -- kg/m^3 that chokes
health.BACKFIRE = 0.5     -- per mana of a weave going off in the hand
health.WARDED = 0.3       -- share of a harm a ward of its colour lets through
health.DOWN_TIME = 3      -- seconds lying defeated
health.REVIVE = 50        -- health on getting up again
health.REACH = 3          -- metres above the ground within which ground harms reach
health.SHELTER = 1.5      -- metres below the ground beyond which the surface's fire and smoke do not reach
health.SAFE = 30          -- metres from fire and hunters a defeated player gets up

local HARMS = { heat = true, impact = true, cold = true, poison = true, smoke = true, magic = true }
-- Which ward turns which harm aside.
health.WARD_OF = { heat = "fire", impact = "earth", cold = "water", smoke = "storm" }

function health.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { hp = saved.hp or health.MAX, max = health.MAX, since_harm = health.REGEN_DELAY, down = nil,
    taken = {}, news = {}, time = 0, defeats = saved.defeats or 0 }
end

function health.snapshot(h)
  return { hp = h.hp, defeats = h.defeats }
end

local function say(h, text)
  for _, n in ipairs(h.news) do
    if n.text == text then
      n.until_time = h.time + 3
      return
    end
  end
  h.news[#h.news + 1] = { text = text, until_time = h.time + 3 }
end

-- Tells the player something (for a few seconds, with the other news).
function health.told(h, text) say(h, text) end

-- Hurts the player (amount > 0) through a harm, less a ward of its colour
-- (wards(kind) -> bool); returns the health taken. No harm while defeated.
function health.hurt(h, harm, amount, wards)
  assert(HARMS[harm], "no such harm: " .. tostring(harm))
  if h.down or amount <= 0 then return 0 end
  local ward = health.WARD_OF[harm]
  if ward and wards and wards(ward) then amount = amount * health.WARDED end
  local taken = math.min(amount, h.hp)
  h.hp = h.hp - taken
  h.since_harm = 0
  h.taken[harm] = (h.taken[harm] or 0) + taken
  if h.hp <= 0 then
    h.hp, h.down, h.defeats = 0, 0, h.defeats + 1
    say(h, "You are defeated (" .. harm .. ")")
  end
  return taken
end

-- Heals up to full health; returns how much.
function health.heal(h, amount)
  if h.down or amount <= 0 then return 0 end
  local got = math.min(amount, h.max - h.hp)
  h.hp = h.hp + got
  return got
end

-- Whether fire burns or the ground is scorching within radius metres of pos
-- (looked at in rings round it).
function health.fire_near(pos, radius)
  local function hot(p)
    local f = sim.fire_at(p)
    return f.loaded and (f.burning or f.temperature > 60)
  end
  if hot(pos) then return true end
  for r = 5, radius, 5 do
    for k = 0, 7 do
      if hot(world.move(pos, k * math.pi / 4, r, 0, 0)) then return true end
    end
  end
  return false
end

-- Where a defeated player gets up: at pos (where the carpet first set out)
-- unless fire or a hunter (hunter_near(p, radius) -> bool) is within
-- health.SAFE metres; else the nearest place round it that is clear of both
-- (rings of 40 .. 400 m), or pos when there is none. Returns the place and
-- whether it was moved.
function health.safe_spot(pos, hunter_near)
  local function safe(p)
    return not health.fire_near(p, health.SAFE) and not (hunter_near and hunter_near(p, health.SAFE))
  end
  if safe(pos) then return pos, false end
  for r = 40, 400, 40 do
    for k = 0, 7 do
      local p = world.move(pos, k * math.pi / 4, r, 0, 0)
      local _, _, depth = world.water(p)
      if (depth or 0) <= 0.5 and safe(p) then
        p.alt = world.ground_height(p)
        return p, true
      end
    end
  end
  return pos, false
end

-- One tick. ctx: pos (the player), on_foot, landing (m/s of a landing this
-- tick, if any), events (creature events), heard (effect events: heal and
-- wither with at and amount), cast (this tick's cast result), wards
-- (kind -> bool). Returns "defeated" when the player falls, "revived" when
-- getting up again, else nil.
function health.tick(h, dt, ctx)
  h.time = h.time + dt
  local kept = {}
  for _, n in ipairs(h.news) do
    if n.until_time > h.time then kept[#kept + 1] = n end
  end
  h.news = kept
  if h.down then
    h.down = h.down + dt
    if h.down >= health.DOWN_TIME then
      h.down, h.hp, h.since_harm = nil, health.REVIVE, 0
      say(h, "You get up again")
      return "revived"
    end
    return nil
  end
  local was = h.hp
  local wards = ctx.wards
  -- The ground under the player: flames, heat, smoke (not below ground, in
  -- a cellar or a cave, where the surface's fire cannot reach).
  local pos = ctx.pos
  local above = pos and pos.alt - world.ground_height(pos)
  if pos and above > -health.SHELTER then
    local fire = sim.fire_at(pos)
    local near_ground = ctx.on_foot or above < health.REACH
    if fire.loaded and near_ground then
      if fire.burning then
        health.hurt(h, "heat", health.BURN * dt, wards)
        say(h, "You are burning: get out of the fire!")
      elseif fire.temperature > 60 then
        health.hurt(h, "heat", (fire.temperature - 60) / 10 * dt, wards)
        say(h, "The ground is scorching: get away from the fire!")
      end
    end
    local air = sim.gas_at(pos)
    if air.smoke > health.SMOKE_THICK then
      health.hurt(h, "smoke", 2 * dt * air.smoke / health.SMOKE_THICK, wards)
    end
  end
  if ctx.landing and ctx.landing > health.FALL_SAFE then
    health.hurt(h, "impact", (ctx.landing - health.FALL_SAFE) * health.FALL_HARM, wards)
  end
  for _, ev in ipairs(ctx.events or {}) do
    if ev.kind == "attacked" and ctx.on_foot then
      if health.hurt(h, "impact", health.BITE, wards) > 0 then say(h, "Bitten!") end
    end
  end
  for _, ev in ipairs(ctx.heard or {}) do
    local d = pos and world.distance(ev.at, pos) or math.huge
    if d <= (ev.radius or 4) then
      if ev.kind == "heal" then
        health.heal(h, ev.amount)
      elseif ev.kind == "wither" then
        health.hurt(h, "magic", ev.amount, wards)
      end
    end
  end
  local r = ctx.cast
  if r and r.ok and r.plan and r.plan.misfire == "backfire" then
    health.hurt(h, "magic", r.plan.power * health.BACKFIRE, wards)
    say(h, "The weave went off in your hand")
  end
  if h.down then return "defeated" end
  if h.hp >= was then
    h.since_harm = h.since_harm + dt
    if h.since_harm >= health.REGEN_DELAY then health.heal(h, health.REGEN * dt) end
  end
  return nil
end

function health.hud_lines(h)
  local lines = { "Health " .. math.ceil(h.hp) .. "/" .. h.max }
  for _, n in ipairs(h.news) do lines[#lines + 1] = n.text end
  return lines
end

return health
