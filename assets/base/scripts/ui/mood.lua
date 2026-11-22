-- SPDX-License-Identifier: MIT
-- The UI takes the mood of the world (docs/design/ui): frames tinted by the
-- light of the hour and the weather (gently: at most MAX_TINT, never the
-- colours that carry meaning); danger near warms the screen's edges with a
-- pomegranate band that grows as it closes in, beats slowly under 30 %
-- health, and marks the edge a hit came from. Reduced motion keeps the
-- meaning without the movement.
local kit = require("ui.kit")
local layout = require("ui.layout")

local mood = {}

mood.MAX_TINT = 0.2
mood.HIT_TIME = 0.6 -- seconds a hit's mark shows

function mood.new()
  return { tint = { 1, 1, 1 }, danger = 0, hits = {}, beat = 0, still = false }
end

local DUSK, NIGHT, STORM = { 1, 0.86, 0.72 }, { 0.78, 0.84, 1 }, { 0.84, 0.84, 0.88 }

local function mix(a, b, k)
  return { a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k, a[3] + (b[3] - a[3]) * k }
end

-- The tint of the frames for the light (daylight 0..1, sun_low 0..1: near the horizon) and the
-- weather (storm 0..1). Each channel stays within MAX_TINT of white.
function mood.tint_for(daylight, sun_low, storm)
  local t = { 1, 1, 1 }
  if daylight < 0.25 then
    t = mix(t, NIGHT, math.min(1, (0.25 - daylight) / 0.15))
  elseif sun_low > 0 then
    t = mix(t, DUSK, sun_low)
  end
  if storm > 0 then t = mix(t, STORM, math.min(1, storm)) end
  for i = 1, 3 do t[i] = math.max(1 - mood.MAX_TINT, math.min(1, t[i])) end
  return t
end

-- One frame: ctx = { daylight, sun_low, storm, danger (0..1, how near), hp_frac, now, reduced_motion }.
function mood.update(md, ctx)
  md.tint = mood.tint_for(ctx.daylight or 1, ctx.sun_low or 0, ctx.storm or 0)
  md.danger = math.max(0, math.min(1, ctx.danger or 0))
  md.low = (ctx.hp_frac or 1) < 0.3
  md.now = ctx.now or 0
  md.still = ctx.reduced_motion and true or false
  for i = #md.hits, 1, -1 do
    if md.now - md.hits[i].at > mood.HIT_TIME then table.remove(md.hits, i) end
  end
  kit.set_tint(md.tint)
end

-- A hit from a side ("left", "right", "front", "back").
function mood.hit(md, side, now)
  md.hits[#md.hits + 1] = { side = side or "front", at = now }
end

-- The side of the screen a thing at bearing b (degrees) is on for a rider looking at heading h.
function mood.side(h, b)
  local off = ((b - h + 540) % 360) - 180
  if math.abs(off) < 45 then return "front" end
  if math.abs(off) > 135 then return "back" end
  return off < 0 and "left" or "right"
end

-- How strong the edges glow now (0..1): danger, the heartbeat under low health.
function mood.edge(md)
  local e = md.danger * 0.6
  if md.low then
    local beat = md.still and 0.5 or (0.5 + 0.5 * math.sin(md.now * 2 * math.pi * 0.9))
    e = math.max(e, 0.25 + 0.35 * beat)
  end
  return e
end

-- Draws the edges (danger, the heartbeat, hits).
function mood.draw(md)
  local w, h = layout.grid()
  local e = mood.edge(md)
  local red = kit.colour("pomegranate")
  local function band(x, y, bw, bh, a)
    if a > 0.01 then render.ui_rect(x, y, bw, bh, { red[1], red[2], red[3], a }) end
  end
  if e > 0 then
    for k = 0, 5 do -- (a soft edge: bands thinning inwards)
      local a = e * 0.22 * (1 - k / 6)
      band(k * 3, 0, 3, h, a)
      band(w - (k + 1) * 3, 0, 3, h, a)
      band(0, k * 3, w, 3, a * 0.8)
      band(0, h - (k + 1) * 3, w, 3, a * 0.8)
    end
  end
  for _, hit in ipairs(md.hits) do
    local a = 0.55 * (1 - (md.now - hit.at) / mood.HIT_TIME)
    if hit.side == "left" then band(0, h // 4, 10, h // 2, a)
    elseif hit.side == "right" then band(w - 10, h // 4, 10, h // 2, a)
    elseif hit.side == "back" then band(w // 4, h - 10, w // 2, 10, a)
    else band(w // 4, 0, w // 2, 10, a) end
  end
end

return mood
