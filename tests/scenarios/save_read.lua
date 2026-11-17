-- SPDX-License-Identifier: MIT
scenario { name = "save read" }
local heir = save.get("heir")
expect(heir ~= nil, "save data from the previous run")
expect_eq(heir.name, "Lei")
expect_eq(heir.level, 3)
expect_eq(heir.items[2], "book")
