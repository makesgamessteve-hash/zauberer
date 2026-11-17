-- SPDX-License-Identifier: MIT
-- Test fixture: half of a dependency cycle.
return { id = "cycle_a", version = "1.0.0", api = "1.0", license = "MIT", requires = { cycle_b = ">=1.0" } }
