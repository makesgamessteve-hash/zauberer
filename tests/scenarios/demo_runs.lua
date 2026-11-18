-- SPDX-License-Identifier: MIT
-- spec: test-automation: Passing scenario
-- spec: lua-scripting: Entry scripts run in order
-- spec: lua-scripting: Tick hook runs per tick
scenario { name = "demo runs for ten seconds" }
demo.game.set_mode("showcase")
expect(demo ~= nil, "the base package's entry script ran")
step(600)
expect_eq(engine.tick(), 600)
expect_near(demo.game.state.time, 10.0, 1e-9, "the tick hook ran 600 times")
expect_near(engine.time(), 10.0, 1e-9)
expect(audio.state(demo.game.music) == "playing", "music loops")
local info = engine.info()
expect(info.version == engine.version)
