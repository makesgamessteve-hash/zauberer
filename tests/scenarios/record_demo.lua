-- SPDX-License-Identifier: MIT
-- Scripted flight input for the replay fixture tests/replays/demo.zbrec
-- (regenerate with: zauberer --headless --scenario tests/scenarios/record_demo.lua
--  --record tests/replays/demo.zbrec --seed 7).
-- spec: carpet-flight: Same flight on every platform
scenario { name = "record flight input" }
input.inject("steer", 0.3, 1, 180)
step(180)
input.inject("boost", 1, 0, 120)
input.inject("steer", -0.5, 1, 120)
step(120)
input.inject("climb", 1, 0, 90)
step(90)
input.inject("chime")
step(40)
input.inject("look", 12, -3, 20)
step(60)
input.inject("credits")
step(110)
