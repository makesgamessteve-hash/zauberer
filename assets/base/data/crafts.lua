-- SPDX-License-Identifier: CC-BY-4.0
-- What the workbench makes (concept 3.4: woodworking, sewing and weaving,
-- carving). needs: item = count from the pack ("thread": units of thread
-- from fibres, hides or cord); makes: item = count; repair: mends the carpet
-- fully; hours: the time it takes.
return {
  { id = "cord", name = "Cord", needs = { thread = 3 }, makes = { cord = 1 }, hours = 0.5 },
  { id = "lantern", name = "Horn lantern", needs = { deadwood = 1, resin = 1, hide = 1, cord = 1 }, makes = { lantern = 1 },
    hours = 2 },
  { id = "staff", name = "Staff", needs = { wood = 2, cord = 1 }, makes = { staff = 1 }, hours = 1 },
  { id = "wand", name = "Wand", needs = { wood = 1, resin = 1 }, makes = { wand = 1 }, hours = 1 },
  { id = "carpet_repair", name = "Mend the carpet", needs = { cord = 2 }, repair = true, hours = 3 },
}
