-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360) with --debug-views 2.
-- spec: rendering-core: Forced stereo
scenario { name = "forced stereo" }
demo.game.set_mode("showcase")
demo.game.hud = false
step(60)
expect_eq(render.stats().views, 2)
expect_eq(render.stats().model_draws, 2, "the carpet is drawn once per view")
local ok, info = render.match_golden("stereo")
expect(ok, info)
