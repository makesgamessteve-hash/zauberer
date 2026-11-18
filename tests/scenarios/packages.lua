-- SPDX-License-Identifier: MIT
-- Runs with the fixture mods override, broken, cycle_a, cycle_b, future and failing_hook installed.
-- spec: asset-packages: Invalid manifest
-- spec: asset-packages: Dependency cycle
-- spec: asset-packages: Package targets a future API
-- spec: lua-scripting: Failing tick hook
-- spec: input-actions: Mod changes a default binding
-- spec: content-licensing: Mod assets appear in credits
scenario { name = "fixture packages" }
demo.game.set_mode("showcase")
local by_id = {}
for _, p in ipairs(engine.packages()) do
  by_id[p.id] = p
end
expect(by_id.base.enabled and by_id.base.load_order == 1, "base loads first")
expect(by_id.override.enabled and by_id.override.load_order > 1, "the override mod loads after base")
expect(by_id.failing_hook.enabled)
expect(not by_id.future.enabled and by_id.future.error:find("scripting API 9.0", 1, true), "future API")
expect(not by_id.cycle_a.enabled and by_id.cycle_a.error:find("cycle", 1, true), "cycle detected")
expect(not by_id.cycle_b.enabled)
local broken
for _, p in ipairs(engine.packages()) do
  if not p.enabled and p.error:find("package.lua", 1, true) and p.id ~= "future" then
    broken = p
  end
end
expect(broken ~= nil, "the broken manifest is reported")
local chime = input.bindings("chime")
expect_eq(chime[1], "key:v", "the mod's binding replaces the base binding")
local credited = {}
for _, c in ipairs(engine.credits()) do
  if c.package == "override" then
    credited[c.file] = c
  end
end
expect(credited["textures/rune.png"] and credited["textures/rune.png"].author == "Fixture Author")
expect(credited["sounds/rune.wav"] and credited["sounds/rune.wav"].license == "CC-BY-4.0")
expect(credited["sounds/rune.wav"].source == "tests/fixtures")
step(10)
expect_eq(failing_hook_ticks, 5, "the failing hook is disabled after its error")
expect_near(demo.game.state.time, 10 / 60, 1e-9, "the base package keeps running")
