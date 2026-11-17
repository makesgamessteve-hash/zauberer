-- SPDX-License-Identifier: MIT
-- Deliberately failing scenario (tests/engine checks the report).
scenario { name = "deliberate failure" }
step(42)
expect_eq(engine.tick(), 41, "tick count")
