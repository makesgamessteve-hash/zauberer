-- SPDX-License-Identifier: MIT
-- Settlement types (concept 6.3): sizes in buildings, people per building,
-- whether walled, and the buildings their plan must hold (by building type
-- id of data/buildings.lua) beside homes.
return {
  { id = "hamlet", name = "farmstead", buildings = { 1, 8 }, people = 6, roles = { "well" } },
  { id = "village", name = "farming village", buildings = { 15, 60 }, people = 5,
    roles = { "well", "shrine", "granary", "smithy", "teahouse" } },
  { id = "fishing_village", name = "fishing village", buildings = { 10, 40 }, people = 5,
    roles = { "well", "shrine", "dock" } },
  { id = "mining_town", name = "mining town", buildings = { 10, 80 }, people = 5,
    roles = { "well", "smithy", "storehouse", "teahouse" } },
  { id = "market_town", name = "market town", buildings = { 60, 250 }, people = 6,
    roles = { "well", "temple", "bazaar", "caravanserai", "bathhouse", "smithy", "granary", "teahouse" } },
  { id = "oasis_town", name = "oasis town", buildings = { 20, 150 }, people = 6,
    roles = { "qanat_outlet", "cistern", "temple", "bazaar", "caravanserai", "bathhouse", "teahouse" } },
  { id = "caravanserai", name = "caravanserai", buildings = { 1, 1 }, people = 30, roles = { "caravanserai" } },
  { id = "city", name = "walled city", buildings = { 250, 1500 }, people = 7, walled = true,
    roles = { "citadel", "temple", "bazaar", "caravanserai", "bathhouse", "cistern", "teahouse", "library" } },
  { id = "citadel", name = "citadel", buildings = { 1, 1 }, people = 120, roles = { "citadel" } },
  { id = "temple", name = "temple complex", buildings = { 10, 40 }, people = 4, roles = { "temple", "well" } },
  { id = "mage_tower", name = "mage tower", buildings = { 1, 4 }, people = 3, roles = { "tower" } },
  { id = "nomad_camp", name = "nomad camp", buildings = { 5, 30 }, people = 6, roles = {}, tents = true },
  { id = "stilt_village", name = "stilt village", buildings = { 10, 50 }, people = 5, roles = { "shrine", "dock" },
    stilts = true },
  { id = "cliff_dwellings", name = "cliff dwellings", buildings = { 10, 60 }, people = 5, roles = { "cistern" } },
}
