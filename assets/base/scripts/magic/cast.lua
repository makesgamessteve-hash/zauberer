-- SPDX-License-Identifier: MIT
-- Casting a weave (concept 3.3): the cost at the caster's place, paid from
-- the reservoir; control from the gesture and familiarity; strain from
-- overcharging; a misfire if the roll says so; the effect at the aimed place;
-- and the journal and attunement updated.
local weave = require("magic.weave")
local mage = require("magic.mage")
local spellbook = require("magic.spellbook")
local effects = require("magic.effects")

local cast = {}

-- A caster: the mage, the spellbook, the effects in play, and a counter
-- that makes every cast's random roll different but repeatable.
function cast.new(m, book, fx)
  return { mage = m or mage.new(), book = book or spellbook.new(), effects = fx or effects.new(), count = 0 }
end

-- The place the weave is aimed at: where a ray along the aim meets the ground
-- or a body within reach, else the end of the reach.
function cast.aim(origin, heading, pitch, reach)
  local far = world.move(origin, heading, reach * math.cos(pitch), 0, reach * math.sin(pitch))
  local hit = sim.sweep(origin, far, 0)
  return hit and hit.pos or far
end

-- The seed of the roll for a caster's next cast: one per cast and tick, and
-- opts.seed (a number) to vary it.
function cast.next_seed(c, opts)
  return ((opts and opts.seed) or 0) * 1000003 + (c.count + 1) * 7919 + engine.tick()
end

-- Casts weave w (verb, colors, form, modifiers, power) from origin along
-- heading and pitch. opts: accuracy (the gesture's, default 0.8), charge
-- (seconds the cast was charged), carpet (for Void moves), seed.
-- Returns a result: ok, name, cost, plan, effect, discovered, message.
function cast.weave(c, w, origin, heading, pitch, opts)
  opts = opts or {}
  local ok, err = weave.validate(w)
  if not ok then return { ok = false, message = err } end
  if mage.burnt_out(c.mage) then
    return { ok = false, message = "too strained to weave" }
  end
  local casts = spellbook.casts(c.book, w)
  local extent = weave.extent(w)
  local target = cast.aim(origin, heading, pitch or 0, extent.reach)
  local cell = mana.at(origin)
  local cost = weave.cost(w, {
    distance = w.form == "tap" and 0 or world.distance(origin, target),
    place = cell.loaded and cell.stored or nil,
    efficiency = mage.efficiency(c.mage, w.colors, weave.tier(casts).bonus),
    material = opts.material,
  })
  if not mage.take(c.mage, cost.per) then
    return { ok = false, message = "not enough mana", cost = cost }
  end
  local overcharge = mage.overcharge(c.mage, w.power, opts.charge or 1)
  mage.strain_by(c.mage, overcharge)
  local seed = cast.next_seed(c, opts)
  c.count = c.count + 1
  local control = weave.control(opts.accuracy or 0.8, casts)
  if c.mage.strain > 60 then control = control * 0.8 end -- blurred by strain
  local plan = weave.resolve(w, { control = control, overcharge = overcharge, casts = casts, seed = seed })
  local effect = effects.apply(c.effects, plan, {
    origin = origin, heading = heading, pitch = pitch or 0, target = target, caster = c.mage, carpet = opts.carpet,
  })
  local entry, discovered = spellbook.record_cast(c.book, w)
  mage.cast(c.mage, w.colors)
  local message = entry.name .. (plan.misfire and (" misfired (" .. plan.misfire .. ")") or "")
  return { ok = true, name = entry.name, cost = cost, plan = plan, effect = effect, discovered = discovered,
    target = target, message = message }
end

-- Time passes for the caster's effects and the mage (at the mana of place).
function cast.tick(c, dt, place)
  mage.tick(c.mage, dt, place)
  return effects.tick(c.effects, dt)
end

return cast
