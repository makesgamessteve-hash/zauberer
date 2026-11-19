-- SPDX-License-Identifier: MIT
-- Several active areas (concept 11.6, implementation plan 1.11): heirs near
-- each other share one active simulation area; heirs apart get one each,
-- up to four. The host's budget is shared: the more areas, the smaller
-- each one's radius (and smaller still when the simulation runs over its
-- budget), never below a floor.
local areas = {}

areas.JOIN = 2000      -- metres within which heirs share an area
areas.RADIUS = 300     -- metres: one area's radius on the host's whole budget
areas.FLOOR = 80       -- metres: the smallest an area becomes
areas.MAX = 4

-- Groups the heirs' positions into areas: { centre, members }.
function areas.cluster(positions, distance)
  local list = {}
  local keys = {}
  for k in pairs(positions) do keys[#keys + 1] = k end
  table.sort(keys)
  for _, k in ipairs(keys) do
    local p = positions[k]
    local found
    for _, a in ipairs(list) do
      if not found and distance(a.centre, p) < areas.JOIN then found = a end
    end
    if found then
      found.members[#found.members + 1] = k
    else
      list[#list + 1] = { centre = p, members = { k } }
    end
  end
  while #list > areas.MAX do -- (more than four apart: the last share the nearest)
    local last = table.remove(list)
    for _, m in ipairs(last.members) do table.insert(list[#list].members, m) end
  end
  return list
end

-- The radius each of n areas gets; over: how far the simulation runs over
-- its budget (total ms / budget ms, 1 when within it).
function areas.radius(n, over)
  local r = areas.RADIUS / math.sqrt(math.max(1, n))
  if over and over > 1 then r = r / over end
  return math.max(areas.FLOOR, math.min(areas.RADIUS, r))
end

-- Sets up the areas: a focus for each and the shared radius. api: {
-- set_focus(index, pos), clear_focus(index), set_radius(metres), distance }.
function areas.apply(positions, over, api)
  local list = areas.cluster(positions, api.distance)
  for i = 1, areas.MAX do
    if list[i] then api.set_focus(i, list[i].centre) else api.clear_focus(i) end
  end
  local r = areas.radius(#list, over)
  api.set_radius(r)
  return list, r
end

return areas
