-- SPDX-License-Identifier: MIT
-- The carpet as a tent (concept 2.3): on flat, dry ground the carpet folds
-- itself into a small canvas tent. Inside is the pocket home (player/home):
-- the mage crouches through the door to enter. Pitching takes a moment and
-- packing up is instant. The tent is a thing in the world: rain falls on it
-- and flames can burn it (the carpet's condition). No pitching on steep
-- ground, in water or on flames.
local tent = {}

tent.STEEP = 0.35       -- the steepest ground it is pitched on (rise over run)
tent.REACH = 3          -- metres from the door within which the mage can enter
tent.PITCH_SECONDS = 2  -- a moment to pitch

-- Whether a tent can be pitched at pos: true, or nil and why.
function tent.can_pitch(pos)
  local h = world.ground_height(pos)
  for k = 0, 7 do
    local q = world.move(pos, k * math.pi / 4, 1.5, 0)
    if math.abs(world.ground_height(q) - h) > tent.STEEP * 1.5 then return nil, "too steep to pitch the tent" end
  end
  local water = sim.water_at(pos)
  if world.water(pos) or (water.loaded and water.depth > 0.05) then return nil, "no pitching in water" end
  local fire = sim.fire_at(pos)
  if fire.loaded and (fire.burning or fire.temperature > 100) then return nil, "the ground is burning" end
  return true
end

function tent.new(saved)
  saved = type(saved) == "table" and saved or {}
  if type(saved.pos) ~= "table" then return nil end
  return { pos = saved.pos, heading = tonumber(saved.heading) or 0, inside = saved.inside and true or false }
end

function tent.snapshot(t)
  return t and { pos = t.pos, heading = t.heading, inside = t.inside or nil } or nil
end

-- Pitches the tent 2 m ahead of the mage at pos facing heading; returns the
-- tent, or nil and why.
function tent.pitch(pos, heading)
  local spot = world.move(pos, heading, 2, 0)
  spot.alt = world.ground_height(spot)
  local ok, why = tent.can_pitch(spot)
  if not ok then return nil, why end
  return { pos = spot, heading = heading + math.pi, inside = false }
end

-- The place in front of its door, where the mage comes out.
function tent.door(t)
  local p = world.move(t.pos, t.heading, 1.8, 0)
  p.alt = world.ground_height(p)
  return p
end

function tent.near(t, pos)
  return t ~= nil and world.distance(tent.door(t), pos) <= tent.REACH
end

return tent
