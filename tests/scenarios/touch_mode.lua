-- SPDX-License-Identifier: MIT
-- Runs with ZB_TOUCH_PRIMARY=1 (a touch-first device).
-- spec: touch-and-gamepad-controls: Touch layout
scenario { name = "touch-first device" }
expect_eq(engine.info().input_mode, "touch")
expect_eq(input.mode(), "touch")
expect_eq(demo.game.input_mode_event, "touch", "scripts receive an input-mode event")
expect(demo.controls.touch_ready(), "the flight's on-screen controls are set up")
expect(not demo.controls.setup_touch(), "they are added only once")
step(1)
