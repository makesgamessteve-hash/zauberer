-- SPDX-License-Identifier: MIT
-- Unit tests of survival (assets/base/scripts/player/survival.lua).
local survival = require("player.survival")

local HOUR = 3600
local MILD = { air = 18, wind = 0, rain = 0, effort = 0 }

local function harm_of(harms, kind)
  local sum = 0
  for _, h in ipairs(harms) do
    if h[1] == kind then sum = sum + h[2] end
  end
  return sum
end

-- spec: survival: Needs
test.case("needs drain in game time, faster with effort", function()
  local rest, work = survival.new(), survival.new()
  survival.tick(rest, 6 * HOUR, MILD)
  survival.tick(work, 6 * HOUR, { air = 18, effort = 2 })
  test.expect_near(rest.food, 80 - 25, 1e-6)
  test.expect_near(rest.water, 80 - 25, 1e-6)
  test.expect(work.food < rest.food and work.water < rest.water and work.rest < rest.rest, "effort costs more")
  local empty = survival.new({ food = 0, water = 0 })
  local harms = survival.tick(empty, HOUR, MILD)
  test.expect(harm_of(harms, "poison") > 5, "starving and parched hurt")
  test.expect(#survival.tick(survival.new(), HOUR, MILD) == 0, "nothing hurts when all is well")
  -- Presets: the wanderer's needs drain at half the pace, the hermit's faster.
  local easy, hard = survival.new(nil, "wanderer"), survival.new(nil, "hermit")
  survival.tick(easy, 6 * HOUR, MILD)
  survival.tick(hard, 6 * HOUR, MILD)
  test.expect(easy.food > rest.food and hard.food < rest.food)
  local saved = survival.new(survival.snapshot(hard))
  test.expect(saved.preset == "hermit" and saved.food == hard.food)
end)

-- spec: survival: Body temperature
test.case("cold, wind and wet chill, fire and clothes warm", function()
  local cold = survival.new()
  local harms = {}
  for _ = 1, 8 do harms = survival.tick(cold, HOUR, { air = -15, wind = 8, effort = 0 }) end
  test.expect(cold.core < survival.CHILL and harm_of(harms, "cold") > 0, "freezing: " .. cold.core)
  local by_fire = survival.new()
  for _ = 1, 8 do survival.tick(by_fire, HOUR, { air = -15, wind = 8, fire = true }) end
  test.expect(by_fire.core > cold.core + 2, "a fire warms")
  local hot = survival.new()
  for _ = 1, 8 do harms = survival.tick(hot, HOUR, { air = 46, effort = 2 }) end
  test.expect(hot.core > survival.FEVERISH and harm_of(harms, "heat") > 0, "overheated: " .. hot.core)
  test.expect(survival.feels({ air = 10, wind = 10 }) < survival.feels({ air = 10 }), "wind chill")
  test.expect(survival.feels({ air = 0, fire = true }) > survival.feels({ air = 0 }), "a fire warms the cold")
  test.expect(survival.feels({ air = 26 }) == 26 and survival.feels({ air = 26, fire = true }) == 26,
    "in the heat the clothes come off and the fire is sat back from")
  test.expect(survival.feels({ air = 15 }) == survival.COMFORT, "clothes as far as wanted")
  test.expect(survival.feels({ air = 10, height = 2000 }) < survival.feels({ air = 10 }), "colder high up")
  -- Rain soaks, a fire dries.
  local wet = survival.new()
  survival.tick(wet, HOUR, { air = 12, rain = 3 })
  test.expect(wet.wet > 0.9, "soaked")
  survival.tick(wet, 0.5 * HOUR, { air = 12, fire = true })
  test.expect(wet.wet < 0.1, "dried by the fire")
  local waded = survival.new()
  survival.tick(waded, 60, { air = 12, in_water = 0.6 })
  test.expect_eq(waded.wet, 1)
  test.expect(survival.hud_lines(wet)[1]:find("Food", 1, true))
end)

-- spec: survival: Conditions
test.case("sickness runs its course and remedies cure it", function()
  local s = survival.new()
  local swamp = { name = "Swamp water", water = 0.5, dirty = 0.8 }
  for _ = 1, 20 do
    if s.sick > 0 then break end
    survival.consume(s, swamp, 0)
  end
  test.expect(s.sick > 0, "dirty water made it sick")
  survival.tick(s, 12 * HOUR, MILD)
  test.expect_near(s.sick, 0.6, 1e-9)
  local worse = survival.new({ sick = 0.6 })
  local well = survival.new()
  survival.tick(worse, 6 * HOUR, MILD)
  survival.tick(well, 6 * HOUR, MILD)
  test.expect(worse.water < well.water and worse.rest < well.rest, "sickness drains")
  -- Untreated it passes in days; a remedy cures it.
  local untreated = survival.new({ sick = 0.6 })
  for _ = 1, 3 * 24 do survival.tick(untreated, HOUR, MILD) end
  test.expect(untreated.sick > 0 and untreated.sick < 0.6, "slowly better")
  local tea = { name = "Herbal tea", water = 0.4, cures = { sick = 0.5 } }
  survival.consume(s, tea, 0)
  survival.consume(s, tea, 0)
  test.expect_eq(s.sick, 0, "cured")
  -- Spoilt and raw food, poison, salt.
  local p = survival.new()
  survival.consume(p, { name = "Fly agaric", kcal = 6, poison = 0.5 }, 0)
  test.expect_eq(p.poison, 0.5)
  test.expect(harm_of(survival.tick(p, HOUR, MILD), "poison") > 2, "poisoned")
  local salty = survival.new()
  survival.consume(salty, { name = "Sea water", water = 0.5, salty = true }, 0)
  test.expect(salty.water < 80, "sea water makes it thirstier")
  local meal = survival.new({ food = 10 })
  survival.consume(meal, { name = "Cooked meat", kcal = 450 }, 0)
  test.expect_near(meal.food, 10 + 450 / survival.FOOD_KCAL * 100, 1e-9)
  local rotten = 0
  for seed = 1, 20 do
    local r = survival.new({ seed = seed })
    survival.consume(r, { name = "Meat", kcal = 400 }, 0.95)
    rotten = rotten + (r.sick > 0 and 1 or 0)
  end
  test.expect(rotten >= 15, "rotten food sickens: " .. rotten)
  local wanderer = survival.new(nil, "wanderer")
  for _ = 1, 20 do survival.consume(wanderer, swamp, 0) end
  test.expect_eq(wanderer.sick, 0, "no disease for the wanderer")
end)

-- spec: survival: Sleep
test.case("sleep restores rest and takes its hours", function()
  local tired = survival.new({ rest = 5, food = 60, water = 60 })
  local hours = survival.sleep(tired, 12, MILD)
  test.expect(hours >= 7 and hours <= 9, "about eight hours: " .. hours)
  test.expect(tired.rest >= 99)
  test.expect(tired.food < 60 and tired.food > 30, "hungrier, not starving: " .. tired.food)
  local rested = survival.new({ rest = 100 })
  test.expect_eq(survival.sleep(rested, 12, MILD), 0, "no need to sleep")
end)
