-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360).
-- spec: platform-targets: Resize at runtime
scenario { name = "resize at runtime" }
demo.game.set_mode("showcase")
step(5)
local frames = render.stats().frames
engine.resize_window(800, 600)
step(5)
expect(demo.game.drawable ~= nil, "scripts received a resize event")
expect_eq(demo.game.drawable[1] .. "x" .. demo.game.drawable[2], "800x600")
local st = render.stats()
expect_eq(st.drawable_width .. "x" .. st.drawable_height, "800x600")
expect(st.frames > frames, "rendering continues without a restart")
engine.set_fullscreen(true)
step(5)
engine.set_fullscreen(false)
step(5)
expect(render.stats().frames > frames + 10)
