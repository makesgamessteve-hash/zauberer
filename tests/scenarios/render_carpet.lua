-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360): the reference scene used by desktop and web goldens.
-- spec: rendering-core: Render the carpet model
-- spec: platform-targets: Normal start
scenario { name = "reference scene" }
demo.game.hud = false
step(60)
expect_eq(render.state(demo.assets.carpet), "ready")
expect_eq(render.stats().backend, "gl")
local ok, info = render.match_golden("carpet")
expect(ok, info)
