-- SPDX-License-Identifier: MIT
-- luacheck configuration for package scripts, Lua tests and scenarios.
std = "lua54"
max_line_length = 120
read_globals = { "engine", "render", "audio", "input", "vfs", "save", "world", "net", "xr" }
globals = { "demo" }
exclude_files = { "third_party/**", "build/**", "build-*/**" }

files["assets/*/package.lua"] = { std = "lua54" }
files["tests/lua/**"] = { read_globals = { "test" } }
files["tests/scenarios/**"] = {
  read_globals = { "scenario", "step", "sleep", "expect", "expect_eq", "expect_near", "expect_error" },
  globals = { "failing_hook_ticks", "xr_events" },
}
files["tests/fixtures/scenarios/**"] = {
  read_globals = { "scenario", "step", "sleep", "expect", "expect_eq", "expect_near", "expect_error" },
}
files["tests/fixtures/mods/broken/package.lua"] = { only = {} } -- deliberately not a literal table
files["tests/fixtures/mods/failing_hook/**"] = { globals = { "failing_hook_ticks" } }
files["tests/fixtures/mods/xr_events/**"] = { globals = { "xr_events" } }
-- Data tables (credits, bindings, notices) keep one entry per line.
files["assets/*/credits.lua"] = { max_line_length = false }
files["assets/*/data/**"] = { max_line_length = false }
-- Saves kept exactly as the game wrote them.
files["tests/fixtures/saves/**"] = { max_line_length = false }
