-- SPDX-License-Identifier: MIT
-- Runs with ZB_TOUCH_PRIMARY=1 (a touch-first device).
scenario { name = "touch-first device" }
expect_eq(engine.info().input_mode, "touch")
expect_eq(input.mode(), "touch")
expect_eq(demo.game.input_mode_event, "touch", "scripts receive an input-mode event")
expect(demo.touch.ready, "the on-screen controls are set up")
step(1)
