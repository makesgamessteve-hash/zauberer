-- SPDX-License-Identifier: MIT
-- The world explorer in the game: F4 opens it with the world held, Escape
-- closes it, the Menu page opens it too; Q/E turn the overlay; the keys walk
-- the lore (the realms, a realm, its capital and back) while the globe swings
-- to what is open; a settlement clicked on the globe opens; F4 closes.
-- spec: world-explorer: Opening and closing
-- spec: world-explorer: Turning, zooming and marks
-- spec: world-explorer: Following the lore
scenario { name = "world explorer" }
local game = demo.game
local explorer = require("ui.explorer")
local menu = require("ui.menu")
step(5)
local function press(action)
  input.inject(action, 1, 1)
  step(2)
end
local x = game.explorer
press("explorer")
expect(x.open and game.world_held, "F4 opens it, the world held")
expect(x.db ~= nil and #x.db.settlement > 0, "the whole history is there")
press("quit")
expect(not x.open and not game.menu.open, "Escape closes it (and opens no menu)")
-- From the Menu page.
press("quit")
expect(game.menu.open, "the menu")
for i = 1, 20 do
  game.menu.at = i
  local it = menu.focused(game.menu)
  if it and it.go == "explorer" then break end
end
press("confirm")
expect(x.open and not game.menu.open, "the Menu page opens it")
-- The overlay.
press("tab_next")
expect_eq(explorer.LAYERS[x.layer], "realms")
-- The lore with the keys: down to the realms, a realm, its capital, back.
local function rows() return explorer.rows_of(x, explorer.top(x)) end
for _ = 1, 30 do
  local r = rows()[explorer.top(x).at]
  if r.text:find("^Realms") then break end
  input.inject("steer", 0, -1, 1)
  step(2)
  step(25) -- (the key repeats only when held)
end
press("confirm")
expect_eq(explorer.top(x).of, "polity", "the realms")
press("confirm")
local realm = explorer.top(x).id
expect(realm and realm:find("^polity:"), "a realm's page")
local has_capital
for i, r in ipairs(rows()) do
  if r.text:find("^capital: ") then has_capital = i end
end
expect(has_capital, "its capital is a link")
explorer.top(x).at = has_capital
press("confirm")
local capital = explorer.top(x).id
expect(capital and capital:find("^settlement:"), "the capital's page")
step(60)
local w, h = require("ui.layout").grid()
local v = explorer.view(x, w, h)
local px, py = explorer.project(v, x.mark)
expect(px and math.abs(px - w * (1 - explorer.PANEL) / 2) < 3 and math.abs(py - h / 2) < 3,
  "the globe turned to it: " .. tostring(px) .. ", " .. tostring(py))
press("quit")
expect_eq(explorer.top(x).id, realm, "back to the realm")
-- A settlement clicked on the globe opens.
local g = render.ui_grid()
local target
for _, mk in ipairs(explorer.marks(x)) do
  if mk.kind == "settlement" and mk.id ~= capital then
    local sx, sy = explorer.project(v, mk.v)
    if sx and sx > 10 and sx < w * (1 - explorer.PANEL) - 10 and sy > 30 and sy < h - 40 then
      local hit = explorer.hit(x, v, sx, sy)
      if hit and hit.id == mk.id then
        target = { id = mk.id, x = g.x + sx / g.width * g.w, y = g.y + sy / g.height * g.h } -- (as the pointer reads)
        break
      end
    end
  end
end
expect(target ~= nil, "a settlement in view")
if target then
  input.inject_pointer(target.x, target.y, true)
  step(2)
  input.inject_pointer(target.x, target.y, false)
  step(2)
  expect_eq(explorer.top(x).id, target.id, "the clicked settlement's page")
end
press("explorer")
expect(not x.open, "F4 closes it")
step(2)
expect(not game.world_held, "the world goes on")
