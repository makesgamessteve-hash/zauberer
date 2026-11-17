-- SPDX-License-Identifier: MIT
-- Runs with the fixture mods order_a and order_b installed.
scenario { name = "script requests quit" }
step(10)
engine.quit()
step(1000)
expect(false, "never reached: the engine quits first")
