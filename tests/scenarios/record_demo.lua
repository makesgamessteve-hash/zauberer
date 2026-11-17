-- SPDX-License-Identifier: MIT
-- Scripted input for the replay fixture tests/replays/demo.zbrec
-- (regenerate with: zauberer --headless --scenario tests/scenarios/record_demo.lua
--  --record tests/replays/demo.zbrec --seed 7).
scenario { name = "record demo input" }
input.inject("turn", 0.5, -0.25, 120)
step(120)
input.inject("spin", 1, 0, 90)
step(150)
input.inject("chime")
step(40)
input.inject("zoom", 1, 0, 100)
step(100)
input.inject("look", 12, -3, 20)
step(90)
input.inject("credits")
step(100)
