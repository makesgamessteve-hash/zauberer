-- SPDX-License-Identifier: MIT
-- First half of "Data persists across runs" (save_read.lua is the second).
scenario { name = "save write" }
save.set("heir", { name = "Lei", level = 3, items = { "bag", "book" } })
expect(save.flush(), "save data written")
