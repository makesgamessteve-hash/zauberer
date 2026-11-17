-- SPDX-License-Identifier: CC-BY-4.0
-- Test fixture: the other half of a dependency cycle.
return { id = "cycle_b", version = "1.0.0", api = "1.0", license = "MIT", requires = { cycle_a = ">=1.0" } }
