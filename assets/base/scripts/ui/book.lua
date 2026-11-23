-- SPDX-License-Identifier: MIT
-- The menu drawn as the father's book (docs/design/ui): two parchment pages in
-- a kilim-bound cover over the dimmed world, tabs above (Q and E, the shoulder
-- buttons, or pointing at a tab), the page's lines on the left with the
-- focused one on a lapis bar, what it is about on the right, and the bindings
-- under the book. The map is painted across both pages. The title stands over
-- the waiting world with its choices framed and the save Continue would load
-- beside them. Rows and tabs can be pointed at and clicked or tapped.
-- (The menu's pages and what they do are ui/menu.lua's; this only draws them.)
local kit = require("ui.kit")
local layout = require("ui.layout")
local i18n = require("ui.i18n")

local book = {}

book.TABS = { { page = "main", label = "Menu" }, { page = "journal", label = "Journal" },
  { page = "map", label = "Map" }, { page = "help", label = "Help" }, { page = "settings", label = "Settings" } }

-- What each setting does (shown on the right page when it is chosen).
book.ABOUT = {
  language = "The language of the game's words.", music = "How loud the music plays.",
  effects = "How loud the world and the weaves sound.", master = "How loud everything is.",
  mute = "Silence, all at once.", vsync = "Wait for the screen between frames (smoother, steadier).",
  frame_cap = "The most frames a second the game draws.",
  pixel_height = "How many pixels tall the world is drawn: fewer is chunkier and faster.",
  brightness = "How bright the picture is.", invert_look = "Up is down when looking around.",
  vibration = "A gamepad rumbles when the heir is hurt.", text_size = "How large the words and frames are.",
  high_contrast = "Solid frames and outlined words.", colour_vision = "Mana colours kept apart for your eyes.",
  hints = "Small hints about the controls while playing.",
}

local function C(name, a) return kit.colour(name, a) end

local function plain(line)
  if type(line) == "table" then return line.raw or i18n.t(line.text or "") end
  return i18n.t(line)
end

-- The book's frame on the grid: cover, the two pages, the tab row above.
function book.frame_box()
  local s = layout.safe()
  local w = math.min(s.w - 16, 600)
  local h = math.min(s.h - 44, 300)
  local x = s.x + (s.w - w) // 2
  local y = s.y + 24 + (s.h - 44 - h) // 2
  return { x = x, y = y, w = w, h = h }
end

-- Which tab a page belongs to (sub-pages belong to their tab).
local PAGE_TAB = { main = 1, save = 1, load = 1, confirm = 1, controls = 5, packages = 1, credits = 1, company = 1,
  remove = 1, journal = 2, map = 3, help = 4, topic = 4, settings = 5, reset = 5 }

function book.tab_of(page) return PAGE_TAB[page] or 1 end

-- Draws the tabs; returns their boxes (for pointing).
local function draw_tabs(box, current)
  local boxes = {}
  local x = box.x + 4
  for i, t in ipairs(book.TABS) do
    local label = i18n.t(t.label)
    local w = kit.width(label) + 14
    local sel = i == current
    local y = box.y - 18 + (sel and 0 or 3)
    kit.frame(sel and "page" or "plate", x, y, w, 18 - (sel and 0 or 3))
    kit.text(label, x + 7, y + 5, sel and C("ink") or C("parchment"))
    boxes[i] = { x = x, y = box.y - 18, w = w, h = 18, page = t.page }
    x = x + w + 3
  end
  return boxes
end

-- The rows of the left page: the focused one on the focus bar; returns their boxes.
local function draw_rows(lines, x, y, w, bottom)
  local rows = {}
  local line_h = 12
  for i = 2, #lines do
    local l = plain(lines[i])
    local focused = l:sub(1, 2) == "> "
    local is_row = focused or l:sub(1, 2) == "  "
    local shown = is_row and l:sub(3) or l
    for _, part in ipairs(kit.wrap(shown, w - 14)) do
      if y + line_h > bottom then return rows end
      if focused then kit.frame("focus", x - 4, y - 3, w, line_h + 1) end
      kit.text(part, x + 2, y, focused and C("cream") or C("ink"), 1, nil)
      if is_row then rows[#rows + 1] = { x = x - 4, y = y - 3, w = w, h = line_h, index = #rows + 1 } end
      y = y + line_h
      is_row, focused = false, false
    end
  end
  return rows
end

-- The bindings for the page, as chips under the book.
local function draw_hints(box, hints)
  local x, y = box.x, box.y + box.h + 6
  for _, h in ipairs(hints) do
    x = x + kit.chip(h[1], x, y, h[2]) + 4
    x = x + kit.text(i18n.t(h[3]), x, y, C("cream")) + 12
  end
end

local function control(action)
  local help = require("ui.help")
  local device = input.device()
  for _, b in ipairs(input.bindings(action) or {}) do
    local name = help.binding_name(b, device)
    if name then return name, b end
  end
  return "?", nil
end

-- The right page: what the chosen row is about.
local function about(m, x, y, w, ctx)
  local item = ctx.item
  local lines = {}
  if item and item.setting and book.ABOUT[item.setting] then
    lines[#lines + 1] = i18n.t(book.ABOUT[item.setting])
  elseif m.page == "main" or m.page == "title" then
    if ctx.goal then lines[#lines + 1] = i18n.t("Now") .. ": " .. i18n.t(ctx.goal) end
    if ctx.version then lines[#lines + 1] = "Zauberer " .. ctx.version end
  end
  local yy = y
  for _, l in ipairs(lines) do
    for _, part in ipairs(kit.wrap(l, w)) do
      kit.text(part, x, yy, C("ink"))
      yy = yy + 11
    end
    yy = yy + 4
  end
end

-- The map across both pages (painted, see ui/worldmap.lua).
local function draw_map(box, ctx)
  if ctx.draw_map then ctx.draw_map(box.x + 10, box.y + 10, box.w - 20, box.h - 20) end
end

-- Draws the open menu m as the book (or the title). lines: menu.lines(m);
-- ctx: { item (the focused item), goal, version, save (the summary of the save Continue loads),
-- draw_map(x, y, w, h) }. Returns the boxes that can be pointed at: { tabs, rows }.
function book.draw(m, lines, ctx)
  ctx = ctx or {}
  local gw, gh = layout.grid()
  if m.page == "title" then return book.draw_title(m, lines, ctx) end
  render.ui_rect(0, 0, gw, gh, { 0.03, 0.02, 0.07, 0.45 }) -- (the world dimmed behind the book)
  local box = book.frame_box()
  kit.frame("kilim", box.x - 6, box.y - 6, box.w + 12, box.h + 12, C("lapis"))
  local half = box.w // 2
  local tabs = draw_tabs(box, book.tab_of(m.page))
  local rows = {}
  if m.page == "map" then
    kit.frame("page", box.x, box.y, box.w, box.h)
    draw_map(box, ctx)
  else
    kit.frame("page", box.x, box.y, half - 1, box.h)
    kit.frame("page", box.x + half + 1, box.y, half - 1, box.h)
    local title = plain(lines[1] or "")
    kit.text(title, box.x + 14, box.y + 12, C("pomegranate_dark"))
    rows = draw_rows(lines, box.x + 14, box.y + 30, half - 24, box.y + box.h - 10)
    about(m, box.x + half + 14, box.y + 30, half - 28, ctx)
  end
  local hints = {}
  local q = control("tab_prev")
  local e = control("tab_next")
  hints[#hints + 1] = { q .. "/" .. e, nil, "Turn the page" }
  local ok, okb = control("confirm")
  hints[#hints + 1] = { ok, okb, m.page == "map" and "Zoom" or "Choose" }
  local back, backb = control("quit")
  hints[#hints + 1] = { back, backb, "Back" }
  draw_hints(box, hints)
  return { tabs = tabs, rows = rows }
end

-- The title over the waiting world: the name, a woven band, the choices framed, the save's summary.
function book.draw_title(_, lines, ctx)
  local gw = layout.grid()
  local s = layout.safe()
  kit.text_c("ZAUBERER", gw // 2, s.y + 30, C("saffron"), 4)
  for x = gw // 2 - 110, gw // 2 + 104, 6 do
    render.ui_rect(x, s.y + 68, 3, 2, C("pomegranate"))
    render.ui_rect(x + 4, s.y + 68, 1, 2, C("saffron"))
  end
  kit.text_c(i18n.t("The heir of the carpet"), gw // 2, s.y + 76, C("cream"))
  local items = {}
  for i = 2, #lines do
    local l = plain(lines[i])
    if l:sub(1, 2) == "> " or l:sub(1, 2) == "  " then items[#items + 1] = l end
  end
  local pw, ph = 200, 20 + #items * 16
  local px, py = gw // 2 - pw // 2, s.y + 100
  kit.frame("kilim", px, py, pw, ph)
  local rows = {}
  for i, l in ipairs(items) do
    local y = py + 10 + (i - 1) * 16
    local focused = l:sub(1, 2) == "> "
    if focused then kit.frame("focus", px + 8, y - 3, pw - 16, 14) end
    kit.text_c(l:sub(3), gw // 2, y, focused and C("cream") or C("parchment_dark"))
    rows[#rows + 1] = { x = px + 8, y = y - 3, w = pw - 16, h = 14, index = i }
  end
  if ctx.save then
    local sx, sw = px + pw + 12, math.min(170, s.x + s.w - (px + pw + 12))
    local sl = {}
    for _, l in ipairs(ctx.save) do sl[#sl + 1] = l end
    if sw > 60 then
      kit.frame("thread", sx, py, sw, 12 + #sl * 11)
      for i, l in ipairs(sl) do kit.text(l, sx + 7, py + 6 + (i - 1) * 11, i == 1 and C("saffron") or C("cream")) end
    end
  end
  if ctx.version then
    local v = "v" .. ctx.version
    local vw = kit.width(v) + 10
    kit.frame("thread", s.x + s.w - vw, s.y + s.h - 14, vw, 14)
    kit.text(v, s.x + s.w - vw + 5, s.y + s.h - 11, C("grey"))
  end
  return { tabs = {}, rows = rows }
end

-- The pointer on the book: a tab or row pointed at and clicked. Returns ("tab", page) or ("row", index).
local hovered -- (the row the pointer last came onto: a hover focuses once, then the keys may move on)

function book.pointed(boxes)
  if not boxes then return nil end
  for _, t in ipairs(boxes.tabs or {}) do
    if layout.clicked(t) then return "tab", t.page end
  end
  for _, r in ipairs(boxes.rows or {}) do
    if layout.clicked(r) then
      hovered = r.index
      return "row", r.index
    end
    if layout.hover(r) then
      if hovered == r.index then return nil end
      hovered = r.index
      return "hover", r.index
    end
  end
  hovered = nil
  return nil
end

book.plain = plain

return book
