-- SPDX-License-Identifier: MIT
-- The map of the known world: the land the heir has seen (in cells of a
-- kilometre, within two of where they went), the places they came near, the
-- heir, the carpet and the current goal, drawn as coloured letters around a
-- centre that pans, at zooms of 1 to 20 km a letter. Positions are measured
-- from where the carpet first set out (by distance and bearing), so the map
-- reads like a compass map out to hundreds of kilometres. Kept with the save.
local i18n = require("ui.i18n")

local worldmap = {}

worldmap.CELL = 1000      -- metres a cell of seen land
worldmap.SIGHT = 2        -- cells round the heir that count as seen
worldmap.ZOOMS = { 1, 2, 5, 10, 20 } -- kilometres a letter
worldmap.COLS, worldmap.ROWS = 41, 21

function worldmap.new(saved)
  saved = type(saved) == "table" and saved or {}
  local m = { seen = {}, places = {}, zoom = 2, cx = 0, cy = 0, heights = {} }
  for _, k in ipairs(type(saved.seen) == "table" and saved.seen or {}) do
    if type(k) == "string" then m.seen[k] = true end
  end
  for _, p in ipairs(type(saved.places) == "table" and saved.places or {}) do
    if type(p) == "table" and type(p.name) == "string" and type(p.x) == "number" and type(p.y) == "number" then
      m.places[#m.places + 1] = p
    end
  end
  return m
end

function worldmap.snapshot(m)
  local seen = {}
  for k in pairs(m.seen) do seen[#seen + 1] = k end
  table.sort(seen)
  return { seen = seen, places = m.places }
end

-- Metres east and north of home (by distance and bearing).
function worldmap.offset(home, pos)
  local a, b = world.to_local(home), world.to_local(pos)
  local bearing = math.atan(b[1] - a[1], -(b[3] - a[3]))
  local d = world.distance(home, pos)
  return d * math.sin(bearing), d * math.cos(bearing)
end

-- The position x metres east and y north of home.
function worldmap.at(home, x, y)
  local d = math.sqrt(x * x + y * y)
  if d < 1e-6 then return world.move(home, 0, 0, 0, 0) end
  return world.move(home, math.atan(x, y), d, 0, 0)
end

local function key(i, j) return i .. "," .. j end

-- The heir at pos: the land near is seen.
function worldmap.mark(m, home, pos)
  local x, y = worldmap.offset(home, pos)
  local ci, cj = math.floor(x / worldmap.CELL + 0.5), math.floor(y / worldmap.CELL + 0.5)
  local r = worldmap.SIGHT
  for i = ci - r, ci + r do
    for j = cj - r, cj + r do
      if (i - ci) ^ 2 + (j - cj) ^ 2 <= r * r then m.seen[key(i, j)] = true end
    end
  end
  return x, y
end

-- A place come near (once by name).
function worldmap.know(m, home, name, pos)
  for _, p in ipairs(m.places) do
    if p.name == name then return false end
  end
  local x, y = worldmap.offset(home, pos)
  m.places[#m.places + 1] = { name = name, x = math.floor(x), y = math.floor(y) }
  return true
end

function worldmap.pan(m, dx, dy)
  local step = worldmap.ZOOMS[m.zoom] * 1000 * 5
  m.cx, m.cy = m.cx + dx * step, m.cy + dy * step
end

function worldmap.zoom_out(m)
  m.zoom = m.zoom % #worldmap.ZOOMS + 1
end

-- Puts the map's centre on (x, y) metres from home.
function worldmap.centre(m, x, y) m.cx, m.cy = x, y end

local SEA, LOW, HIGH, PEAK = { 0.25, 0.45, 0.85, 1 }, { 0.45, 0.7, 0.35, 1 }, { 0.65, 0.55, 0.35, 1 },
  { 0.92, 0.92, 0.95, 1 }

-- What a seen cell looks like: a letter and its colour, from the land's height.
local function land(m, home, x, y)
  local k = math.floor(x / 500) .. "," .. math.floor(y / 500)
  local h = m.heights[k]
  if not h then
    h = world.height(worldmap.at(home, x, y))
    m.heights[k] = h
  end
  if h < 0 then return "~", SEA end
  if h < 300 then return ".", LOW end
  if h < 1500 then return "^", HIGH end
  return "A", PEAK
end

-- The map's letters: rows of { char, colour } (unseen land blank) and the
-- marks over them ({ char, col, row, label? }). me, carpet, goal: positions.
function worldmap.view(m, home, me, carpet, goal)
  local km = worldmap.ZOOMS[m.zoom] * 1000
  local half_c, half_r = (worldmap.COLS - 1) / 2, (worldmap.ROWS - 1) / 2
  local rows = {}
  for r = 0, worldmap.ROWS - 1 do
    local row = {}
    for c = 0, worldmap.COLS - 1 do
      local x, y = m.cx + (c - half_c) * km, m.cy + (half_r - r) * km
      local ci, cj = math.floor(x / worldmap.CELL + 0.5), math.floor(y / worldmap.CELL + 0.5)
      if m.seen[key(ci, cj)] then
        local ch, col = land(m, home, x, y)
        row[#row + 1] = { ch, col }
      else
        row[#row + 1] = { " ", SEA }
      end
    end
    rows[#rows + 1] = row
  end
  local marks = {}
  local function mark(ch, x, y, label)
    local c = math.floor((x - m.cx) / km + half_c + 0.5)
    local r = math.floor(half_r - (y - m.cy) / km + 0.5)
    if c >= 0 and c < worldmap.COLS and r >= 0 and r < worldmap.ROWS then
      marks[#marks + 1] = { ch = ch, col = c, row = r, label = label }
    end
  end
  for _, p in ipairs(m.places) do mark("o", p.x, p.y, p.name) end
  if goal then
    local gx, gy = worldmap.offset(home, goal)
    mark("X", gx, gy)
  end
  if carpet then
    local x, y = worldmap.offset(home, carpet)
    mark("C", x, y)
  end
  if me then
    local x, y = worldmap.offset(home, me)
    mark("@", x, y)
  end
  return rows, marks
end

local INKS = { sea = { 78, 124, 160 }, sea2 = { 70, 112, 150 }, shore = { 214, 196, 140 }, low = { 128, 150, 84 },
  low2 = { 112, 136, 76 }, high = { 168, 134, 88 }, peak = { 236, 230, 220 } }

-- The colour of seen land at x, y metres from home (nil when unseen), in the parchment's inks.
local function ink(m, home, x, y, px, py)
  local ci, cj = math.floor(x / worldmap.CELL + 0.5), math.floor(y / worldmap.CELL + 0.5)
  if not m.seen[key(ci, cj)] then return nil end
  local k = math.floor(x / 500) .. "," .. math.floor(y / 500)
  local h = m.heights[k]
  if not h then
    h = world.height(worldmap.at(home, x, y))
    m.heights[k] = h
  end
  if h < -2 then return (px + py) % 2 == 0 and INKS.sea or INKS.sea2 end
  if h < 4 then return INKS.shore end
  if h < 300 then return (px * 7 + py * 3) % 11 == 0 and INKS.low2 or INKS.low end
  if h < 1500 then return INKS.high end
  return INKS.peak
end

-- Paints the seen land into a texture of w by h pixels (metres per pixel from the zoom), around the
-- map's centre; repainted only when the view or the seen land changed. Returns the texture.
function worldmap.paint(m, home, w, h)
  local per = worldmap.ZOOMS[m.zoom] * 1000 / 6
  local seen = 0
  for _ in pairs(m.seen) do seen = seen + 1 end
  local sig = string.format("%d,%d,%d,%d,%d,%d", math.floor(m.cx), math.floor(m.cy), m.zoom, seen, w, h)
  if m.painted and m.painted.sig == sig then return m.painted.texture end
  local px = {}
  local clear = string.char(0, 0, 0, 0)
  for j = 0, h - 1 do
    for i = 0, w - 1 do
      local c = ink(m, home, m.cx + (i - w / 2) * per, m.cy + (h / 2 - j) * per, i, j)
      px[#px + 1] = c and string.char(c[1], c[2], c[3], 235) or clear
    end
  end
  if m.painted then render.free_texture(m.painted.texture) end
  m.painted = { sig = sig, texture = render.texture_from_pixels(w, h, table.concat(px)), w = w, h = h }
  return m.painted.texture
end

-- Draws the painted map in the box (x, y, w, h) with the places, the heir, the carpet and the goal,
-- a north mark and the scale.
function worldmap.draw_painted(m, home, me, carpet, goal, x, y, w, h)
  local kit = require("ui.kit")
  w, h = math.min(1024, math.floor(w)), math.min(1024, math.floor(h))
  render.ui_image(worldmap.paint(m, home, w, h), x, y, { w = w, h = h })
  local per = worldmap.ZOOMS[m.zoom] * 1000 / 6
  local function at(px, py) return x + w / 2 + (px - m.cx) / per, y + h / 2 - (py - m.cy) / per end
  local function inside(sx, sy) return sx >= x and sx < x + w - 8 and sy >= y and sy < y + h - 8 end
  local ink_dark = kit.colour("ink")
  for _, p in ipairs(m.places) do
    local sx, sy = at(p.x, p.y)
    if inside(sx, sy) then
      kit.icon("place", math.floor(sx) - 8, math.floor(sy) - 8)
      render.draw_text(kit.font_handle(), p.name, sx + 7, sy - 4, 1, ink_dark)
    end
  end
  local marks = { { goal, "goal" }, { carpet, "carpet" }, { me, "heir" } }
  for _, mk in ipairs(marks) do
    if mk[1] then
      local ox, oy = worldmap.offset(home, mk[1])
      local sx, sy = at(ox, oy)
      if inside(sx, sy) then kit.icon(mk[2], math.floor(sx) - 8, math.floor(sy) - 8) end
    end
  end
  render.draw_text(kit.font_handle(), "N", x + w - 18, y + 6, 1, ink_dark)
  render.ui_rect(x + w - 16, y + 16, 1, 10, ink_dark)
  local km = worldmap.ZOOMS[m.zoom]
  render.ui_rect(x + 10, y + h - 12, math.floor(km * 1000 / per), 2, ink_dark)
  render.draw_text(kit.font_handle(), km .. " km", x + 14 + math.floor(km * 1000 / per), y + h - 16, 1, ink_dark)
end

-- Draws the map at (x, y) in screen pixels with a key line under it.
function worldmap.draw(m, font, home, me, carpet, goal, x, y)
  local rows, marks = worldmap.view(m, home, me, carpet, goal)
  local adv, line = 6, 9
  render.draw_text(font, i18n.t("Map"), x, y, 1, { 1, 1, 1, 1 })
  local top = y + line + 2
  for r, row in ipairs(rows) do
    for c, cell in ipairs(row) do
      if cell[1] ~= " " then render.draw_text(font, cell[1], x + (c - 1) * adv, top + (r - 1) * line, 1, cell[2]) end
    end
  end
  for _, mk in ipairs(marks) do
    local px, py = x + mk.col * adv, top + mk.row * line
    render.draw_text(font, mk.ch, px, py, 1, { 1, 0.9, 0.3, 1 })
    if mk.label then render.draw_text(font, mk.label, px + adv, py, 1, { 1, 1, 1, 1 }) end
  end
  local bottom = top + worldmap.ROWS * line + 2
  render.draw_text(font, string.format(i18n.t("%d km a letter; @ you, C the carpet, X the goal, o a place"),
    worldmap.ZOOMS[m.zoom]), x, bottom, 1, { 0.8, 0.8, 0.8, 1 })
  render.draw_text(font, i18n.t(input.mode() == "touch" and "Stick: move, OK: zoom, Menu: back"
    or "Arrows: move, Enter: zoom, Escape: back"), x, bottom + line, 1, { 0.8, 0.8, 0.8, 1 })
  return rows, marks
end

return worldmap
