-- SPDX-License-Identifier: CC-BY-4.0
-- Upgrades woven into the carpet's slots (concept 2.1; woven by artificing).
--   effect   weatherproof  rain does not soak it
--            fire_ward     share of a burn that gets through
--            speed         factor on its top speed and acceleration
--            silent        factor on how visible it is in flight
return {
  { id = "weatherproof", name = "Weatherproof weave", effect = { weatherproof = true } },
  { id = "fire_ward", name = "Fire ward lining", effect = { fire_ward = 0.3 } },
  { id = "swift_weave", name = "Swift weave", effect = { speed = 1.15 } },
  { id = "silent_glide", name = "Silent glide", effect = { silent = 0.5 } },
}
