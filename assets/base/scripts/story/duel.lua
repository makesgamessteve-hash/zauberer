-- SPDX-License-Identifier: MIT
-- Duels with mages (concept 7.5, 1.6): a foe with health, a ward that is
-- weak to some colours and strong against others, and attacks at a pace.
-- The heir's casts strike through the ward by their power and colour; the
-- arena's state (a calmed ley storm, the foe's mana field) shifts the
-- balance. A foe at no health is defeated; the heir's harm goes to their
-- body by the damage model.
local duel = {}

duel.BASE = 10 -- harm of a power-1 cast on an unwarded foe

-- foe: { name, hp, weak = { colour }, strong = { colour }, attack (harm),
-- every (seconds between attacks), colour (of its attacks) }.
function duel.new(foe)
  return { foe = foe, hp = foe.hp, max = foe.hp, clock = 0, log = {}, over = false }
end

local function among(list, c)
  for _, x in ipairs(list or {}) do if x == c then return true end end
  return false
end

-- A cast of the heir's: { colours, power }. env: { calm (the storm calmed:
-- the ward weaker) }. Returns the harm done.
function duel.cast(D, w, env)
  if D.over then return 0 end
  local f = D.foe
  local k = 1
  for _, c in ipairs(w.colours or {}) do
    if among(f.weak, c) then k = k * 1.6 elseif among(f.strong, c) then k = k * 0.4 end
  end
  if env and env.calm then k = k * 1.5 end
  local harm = duel.BASE * (w.power or 1) * k
  D.hp = math.max(0, D.hp - harm)
  D.log[#D.log + 1] = harm
  if D.hp <= 0 then D.over, D.won = true, true end
  return harm
end

-- Time passes in the duel: the foe attacks at its pace. hurt(harm, colour)
-- harms the heir. Returns the attacks made.
function duel.tick(D, dt, hurt)
  if D.over then return 0 end
  D.clock = D.clock + dt
  local n = 0
  while D.clock >= D.foe.every do
    D.clock = D.clock - D.foe.every
    hurt(D.foe.attack, D.foe.colour)
    n = n + 1
  end
  return n
end

-- The heir gave up or fell: the duel is lost.
function duel.yield(D)
  D.over, D.won = true, false
end

return duel
