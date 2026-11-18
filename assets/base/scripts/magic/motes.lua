-- SPDX-License-Identifier: MIT
-- Mana motes (concept 3.2): glowing specks of one colour's mana released by
-- magical beings when they die, by destroyed enchanted objects and by mana
-- blooms. They drift with the wind and slowly dissolve back into the field.
-- A mage flying through them collects them (they are drawn in from a little
-- way off), and a mage holding still can channel mana from the field itself.
local weave = require("magic.weave")
local mage = require("magic.mage")

local motes = {}

motes.LIFETIME = 120     -- seconds for a mote to dissolve
motes.COLLECT = 4        -- metres: collected
motes.ATTRACT = 15       -- metres: drawn towards a collector
motes.MAX = 512
motes.BLOOM_EVERY = 20   -- seconds between looks for a bloom
motes.BLOOM_RICH = 60    -- mana a cell's richest colour must hold to bloom
motes.CHANNEL_SPEED = 5  -- m/s: faster than this, no channeling
local DRIFT = 0.3        -- share of the wind a mote follows
local PULL = 6           -- m/s towards a collector

function motes.new(seed)
  return { list = {}, rng = weave.rng(seed or 1), since_bloom = 0, released = 0, collected = 0, dissolved = 0,
    time = 0, recent = {}, recent_until = 0 }
end

-- Releases amount mana of a colour as count motes (1 .. 64) around pos;
-- returns how many were made.
function motes.release(s, pos, color, amount, count)
  count = math.max(1, math.min(count or 5, 64))
  local made = 0
  for _ = 1, count do
    if #s.list >= motes.MAX or amount <= 0 then break end
    local a, r = s.rng() * 2 * math.pi, 0.5 + 2 * s.rng()
    local p = world.move(pos, a, r, 0, 1 + 2 * s.rng())
    s.list[#s.list + 1] = { pos = p, color = color, amount = amount / count, age = 0, phase = s.rng() * 6.28 }
    made = made + 1
  end
  s.released = s.released + amount * made / count
  return made
end

-- Collects into a mage what fits; returns how much.
local function collect(m, mote)
  local got = mage.add(m, mote.color, mote.amount)
  mote.amount = mote.amount - got
  return got
end

-- Time passes: motes drift, are drawn to and collected by the collectors
-- ({ pos, mage }), and dissolve into the field. Returns what was collected.
function motes.tick(s, dt, collectors)
  collectors = collectors or {}
  s.time = s.time + dt
  local got, kept = {}, {}
  for _, m in ipairs(s.list) do
    m.age = m.age + dt
    -- Drift with the wind, bobbing a little.
    local w = sim.wind_at(m.pos)
    local east, north = w.east * DRIFT, w.north * DRIFT
    local bob = math.sin(m.age * 1.7 + m.phase) * 0.3
    -- Drawn in by the nearest collector within reach.
    for _, c in ipairs(collectors) do
      local flat, up = world.distance(m.pos, c.pos), c.pos.alt - m.pos.alt
      local d = math.sqrt(flat * flat + up * up)
      if d <= motes.COLLECT then
        local amount = collect(c.mage, m)
        if amount > 0 then
          got[#got + 1] = { color = m.color, amount = amount }
          s.collected = s.collected + amount
          s.recent[m.color] = (s.recent[m.color] or 0) + amount
          s.recent_until = s.time + 3
        end
      elseif d <= motes.ATTRACT then
        local a, b = world.to_local(m.pos), world.to_local(c.pos)
        local de, dn = b[1] - a[1], -(b[3] - a[3])
        local len = math.max(math.sqrt(de * de + dn * dn), 0.1)
        east, north = east + de / len * PULL, north + dn / len * PULL
        bob = bob + up / math.max(d, 0.1) * PULL
      end
    end
    local speed = math.sqrt(east * east + north * north)
    if speed > 0 then m.pos = world.move(m.pos, math.atan(east, north), speed * dt, 0, bob * dt) end
    -- Dissolving: a share of its mana goes back to the field each tick.
    local lost = math.min(m.amount, m.amount * dt / math.max(motes.LIFETIME - m.age, 1))
    if m.age >= motes.LIFETIME then lost = m.amount end
    if lost > 0 then
      mana.release(m.pos, m.color, lost)
      m.amount = m.amount - lost
      s.dissolved = s.dissolved + lost
    end
    if m.amount > 0.01 then kept[#kept + 1] = m end
  end
  s.list = kept
  return got
end

-- Mana blooms: where a colour runs very rich around pos, some of it now and
-- then gathers into motes (taken from the field, so nothing is made).
function motes.bloom(s, pos, dt)
  s.since_bloom = s.since_bloom + dt
  if s.since_bloom < motes.BLOOM_EVERY then return 0 end
  s.since_bloom = 0
  local cell = mana.at(pos)
  if not cell.loaded then return 0 end
  local best, most = nil, motes.BLOOM_RICH
  for _, c in ipairs(mage.COLORS) do
    if cell.stored[c] > most and cell.stored[c] >= 0.9 * cell.cap[c] then best, most = c, cell.stored[c] end
  end
  if not best then return 0 end
  local where = world.move(pos, s.rng() * 2 * math.pi, 10 + 15 * s.rng(), 0)
  local taken = mana.draw(where, best, 10, 16)
  return taken > 0 and motes.release(s, where, best, taken, 5) or 0
end

-- Channeling: a mage holding still (slower than CHANNEL_SPEED) draws from the
-- field at the channeling rate, each colour by how much of it there is.
-- Returns the mana gained.
function motes.channel(m, pos, dt, speed)
  if (speed or 0) > motes.CHANNEL_SPEED then return 0 end
  local cell = mana.at(pos)
  if not cell.loaded then return 0 end
  local total = 0
  for _, c in ipairs(mage.COLORS) do total = total + cell.stored[c] end
  if total <= 0 then return 0 end
  local gained = 0
  for _, c in ipairs(mage.COLORS) do
    local want = m.channel * dt * cell.stored[c] / total
    if want > 0 then
      local room = math.max(m.capacity - mage.total(m), 0)
      local taken = mana.draw(pos, c, math.min(want, room), 16)
      gained = gained + mage.add(m, c, taken)
    end
  end
  return gained
end

-- The HUD line of what was collected lately.
function motes.hud_lines(s)
  if s.time >= s.recent_until then
    s.recent = {}
    return {}
  end
  local parts = {}
  for _, c in ipairs(mage.COLORS) do
    if s.recent[c] then parts[#parts + 1] = c .. " " .. math.floor(s.recent[c] + 0.5) end
  end
  return { "Motes collected: " .. table.concat(parts, ", ") }
end

return motes
