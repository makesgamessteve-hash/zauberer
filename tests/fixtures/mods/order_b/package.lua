-- SPDX-License-Identifier: CC-BY-4.0
-- Test fixture: logs its shutdown (loads after order_a).
return { id = "order_b", version = "1.0.0", api = "1.0", license = "MIT", requires = { order_a = ">=1.0" } }
