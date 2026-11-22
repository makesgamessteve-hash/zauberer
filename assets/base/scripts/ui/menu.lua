-- SPDX-License-Identifier: MIT
-- The game's menu (Escape in play): resume, settings, controls (remapping),
-- packages (mods), credits and quit. Up and down choose a line, left and right
-- change a setting, confirm opens or does it, Escape goes back. While it is
-- open the heir waits; alone the world stands still (in company it cannot stop).
local settings = require("ui.settings")
local i18n = require("ui.i18n")

local menu = {}

menu.REPEAT = 0.25 -- seconds before a held direction moves again

-- The actions the menu reads while it is open (the game's are idle then).
menu.ACTIONS = { quit = true, steer = true, confirm = true }

-- The controls the player can remap, in menu order.
menu.REMAP = { "steer", "climb", "boost", "cast_left", "cast_right", "draw", "verb", "color", "color2", "channel",
  "mount", "jump", "use", "sleep", "stow", "unstow", "cloak", "tent", "codex", "story", "weave", "reset" }

local function yes_no(v) return v and "on" or "off" end

local function value_text(name, v)
  if name == "language" then
    for _, l in ipairs(i18n.languages()) do
      if l.code == v then return l.name end
    end
    return v
  end
  if type(v) == "boolean" then return yes_no(v) end
  if name:match("_volume$") then return math.floor(v + 0.5) .. "%" end
  if name == "frame_cap" then return v == 0 and "none" or (v .. " fps") end
  if name == "pixel_height" then return v .. " lines" end
  if name == "text_size" then return v .. "x" end
  return tostring(v)
end

-- Labels of the settings, as the menu shows them (translated where they are drawn).
menu.LABELS = {
  master_volume = "Volume", music_volume = "Music", effects_volume = "Sounds", mute = "Mute", vsync = "V-sync",
  frame_cap = "Frame cap", pixel_height = "Pixel resolution", brightness = "Brightness", invert_look = "Invert look",
  vibration = "Vibration",
  language = "Language", text_size = "Text size", high_contrast = "High contrast", colour_vision = "Colour vision",
  colour_glyphs = "Colour glyphs", reduced_flashing = "Reduced flashing",
  captions = "Sound captions", toggle_hold = "Toggle instead of hold", gesture_assist = "Gesture assist",
  look_curve = "Look curve", look_speed = "Look speed", fov = "Field of view",
  story_markers = "Story markers", hints = "Hints", survival = "Survival", xr_steer = "VR steering",
  xr_snap_turn = "VR snap turning", xr_vignette = "VR comfort vignette", xr_reduced_speed = "VR reduced speed",
  xr_stabiliser = "VR horizon stabiliser",
}

-- s: the settings table; hooks: { quit(), resume(), packages(), set_package(id, on), and for the
-- title: continue(), new_world(), saves() -> { { name, day } } newest first, load(name),
-- delete(name) -> ok, why, current() -> the save being played, version, can_quit }.
function menu.new(s, hooks)
  return { open = false, page = "main", home = "main", at = 1, s = s, hooks = hooks or {}, hold = 0, last = 0,
    capture = nil, note = nil }
end

-- Opens the end of the story: its closing lines, then the credits, then play goes on.
function menu.open_ending(m)
  m.open, m.page, m.home, m.at, m.capture, m.note = true, "ending", "ending", 1, nil, nil
end

-- Opens the title: the game's first page (continue, a new world, a saved game, settings, credits, quit).
function menu.open_title(m)
  m.open, m.page, m.home, m.at, m.capture, m.note = true, "title", "title", 1, nil, nil
end

local function save_line(entry)
  return entry.day and string.format("%s, day %d", entry.name, entry.day) or entry.name
end

local function items(m)
  local out = {}
  if m.page == "ending" then
    out = { { label = "Continue", page = "ending_credits" } }
  elseif m.page == "ending_credits" then
    out = { { label = "Go on in the world", go = "close" } }
  elseif m.page == "title" then
    local h = m.hooks
    local saves = h.saves and h.saves() or {}
    out[1] = { label = m.started and "Continue" or (h.current and h.current() and "Continue" or "New game"),
      go = "continue" }
    out[#out + 1] = { label = "Create a world", go = "new_world" }
    if #saves > 0 then out[#out + 1] = { label = "Load game", page = "load" } end
    out[#out + 1] = { label = "Help", page = "help" }
    out[#out + 1] = { label = "Settings", page = "settings" }
    out[#out + 1] = { label = "Controls", page = "controls" }
    out[#out + 1] = { label = "Credits", page = "credits" }
    if h.can_quit ~= false then out[#out + 1] = { label = "Quit", go = "quit" } end
  elseif m.page == "help" then
    for _, t in ipairs(require("ui.help").TOPICS) do
      out[#out + 1] = { label = t.title, page = "topic", topic = t.id }
    end
    out[#out + 1] = { label = "Show hints again", go = "reset_hints" }
  elseif m.page == "load" then
    for _, e in ipairs((m.hooks.saves and m.hooks.saves()) or {}) do
      out[#out + 1] = { save = e, raw = save_line(e), page = "save" }
    end
  elseif m.page == "save" then
    out = { { label = "Play", go = "load" }, { label = "Delete", page = "confirm" }, { label = "Back", go = "back" } }
  elseif m.page == "confirm" then
    out = { { label = "No, keep it", go = "back" }, { label = "Yes, delete it", go = "delete" } }
  elseif m.page == "company" then
    local h = m.hooks
    for _, id in ipairs(require("coop.quick").ORDER) do
      out[#out + 1] = { label = require("coop.quick").TEXT[id], go = "quick", quick = id }
    end
    if h.is_host and h.is_host() then
      for _, heir in ipairs(h.heirs and h.heirs() or {}) do
        out[#out + 1] = { raw = string.format(i18n.t("Remove %s"), heir.name), page = "remove", heir = heir }
      end
      out[#out + 1] = { label = h.is_open() and "Close to new heirs" or "Open to new heirs", go = "toggle_open" }
    end
  elseif m.page == "remove" then
    out = { { label = "No, keep them", go = "back" }, { label = "Yes, remove them", go = "kick" } }
  elseif m.page == "main" then
    out = { { label = "Resume", go = "resume" }, { label = "Map", page = "map" },
      { label = "Journal", page = "journal" }, { label = "Help", page = "help" },
      { label = "Settings", page = "settings" },
      { label = "Controls", page = "controls" }, { label = "Packages", page = "packages" },
      { label = "Credits", page = "credits" }, { label = "Save and quit", go = "quit" } }
    if m.hooks.in_company and m.hooks.in_company() then
      table.insert(out, 2, { label = "Company", page = "company" })
    end
  elseif m.page == "settings" then
    for _, name in ipairs(settings.ORDER) do
      out[#out + 1] = { setting = name, label = menu.LABELS[name] }
    end
    out[#out + 1] = { label = "Reset to defaults", page = "reset" }
  elseif m.page == "reset" then
    out = { { label = "No, keep them", go = "back" }, { label = "Yes, reset them", go = "reset_settings" } }
  elseif m.page == "controls" then
    for _, action in ipairs(menu.REMAP) do
      out[#out + 1] = { action = action, label = action }
    end
  elseif m.page == "packages" then
    for _, p in ipairs((m.hooks.packages and m.hooks.packages()) or {}) do
      out[#out + 1] = { package = p, label = p.id }
    end
  end
  return out
end

function menu.toggle(m)
  m.open = not m.open
  m.page, m.home, m.at, m.capture, m.note = "main", "main", 1, nil, nil
  return m.open
end

-- Closes the menu (and the title) and goes on playing.
function menu.close(m)
  m.open, m.page, m.home = false, "main", "main"
  if m.hooks.resume then m.hooks.resume() end
end

-- (the pages under another)
local UP = { save = "load", confirm = "save", topic = "help", reset = "settings", remove = "company" }

local function back(m)
  if m.capture then
    m.capture = nil
  elseif m.page == "main" then
    menu.close(m)
  elseif m.page == "title" or m.page == "ending" then
    return -- (left by choosing)
  elseif m.page == "ending_credits" then
    menu.close(m) -- (the credits are skipped)
  elseif UP[m.page] then
    m.page, m.at = UP[m.page], 1
  else
    m.page, m.at = m.home, 1
  end
end

-- The bindings of an action with a new one in place of those of the same
-- device (keyboard and mouse, gamepad, touch or VR controllers).
local function device(b)
  local kind = b:match("^([%a_]+):") or b
  if kind == "key" or kind == "keys" or kind == "mouse" then return "desktop" end
  return kind
end

function menu.rebind(current, binding)
  local out = {}
  for _, b in ipairs(current) do
    if device(b) ~= device(binding) then out[#out + 1] = b end
  end
  table.insert(out, 1, binding)
  return out
end

local function activate(m, item, step)
  local h = m.hooks
  if item.go == "resume" or item.go == "back" then
    back(m)
  elseif item.go == "quit" then
    if h.quit then h.quit() end
  elseif item.go == "continue" then
    m.started = true
    menu.close(m)
    if h.continue then h.continue() end
  elseif item.go == "new_world" then
    m.started = true
    menu.close(m)
    if h.new_world then h.new_world() end
  elseif item.go == "load" then
    local ok, why = h.load(m.chosen.name)
    if ok then
      m.started = true
      menu.close(m)
    else
      m.note = why
    end
  elseif item.go == "delete" then
    local ok, why = h.delete(m.chosen.name)
    m.note = ok and nil or why
    m.page, m.at = "load", 1
    if not ok then return end
    if #((h.saves and h.saves()) or {}) == 0 then m.page = "title" end
  elseif item.go == "close" then
    menu.close(m)
  elseif item.go == "quick" then
    if h.quick then h.quick(item.quick) end
    menu.close(m)
  elseif item.go == "kick" then
    local ok, why = h.kick(m.chosen_heir.player)
    m.page, m.at, m.note = "company", 1, ok and string.format(i18n.t("%s was removed"), m.chosen_heir.name) or why
  elseif item.go == "toggle_open" then
    h.set_open(not h.is_open())
  elseif item.go == "reset_settings" then
    settings.reset(m.s)
    m.page, m.at, m.note = "settings", 1, "Back to the defaults"
  elseif item.go == "reset_hints" then
    if h.reset_hints then h.reset_hints() end
    m.note = "The hints will show again"
  elseif item.page then
    if item.page == "map" and h.map_open then h.map_open() end
    if item.save then m.chosen = item.save end
    if item.heir then m.chosen_heir = item.heir end
    if item.topic then m.topic = item.topic end
    m.page, m.at = item.page, 1
  elseif item.setting then
    settings.step(m.s, item.setting, step or 1)
  elseif item.action and not step then
    m.capture = item.action
    input.capture() -- (forget what was pressed before)
  elseif item.package and not step and m.hooks.set_package then
    local ok, why = m.hooks.set_package(item.package.id, item.package.turned_off)
    m.note = ok and "Takes effect at the next start" or why
  end
end

-- One tick while the menu is open. st(name): the action states.
-- The book's tabs, in order (ui/book.lua draws them).
menu.TABS = { "main", "journal", "map", "help", "settings" }

-- Opens a tab's page.
function menu.goto_tab(m, page)
  m.page, m.at, m.capture = page, 1, nil
end

-- Turns to the next (dir 1) or the previous (-1) tab.
function menu.turn_tab(m, dir)
  local at = 1
  local of = { save = "main", load = "main", confirm = "main", controls = "settings", packages = "main",
    credits = "main", company = "main", remove = "main", topic = "help", reset = "settings" }
  local page = of[m.page] or m.page
  for i, p in ipairs(menu.TABS) do if p == page then at = i end end
  menu.goto_tab(m, menu.TABS[(at - 1 + dir) % #menu.TABS + 1])
end

-- The item the focus is on.
function menu.focused(m)
  return items(m)[m.at]
end

-- Points at row i (the focus follows); chosen, it is done.
function menu.point(m, i, chosen)
  local list = items(m)
  if not list[i] then return false end
  m.at = i
  if chosen then activate(m, list[i]) end
  return true
end

function menu.tick(m, st, dt)
  if not m.open then return end
  if m.capture then
    local binding = input.capture()
    if binding and binding ~= "key:escape" then
      input.set_user_binding(m.capture, menu.rebind(input.bindings(m.capture), binding))
      m.capture = nil
    elseif st("quit").pressed then
      m.capture = nil
    end
    return
  end
  if st("quit").pressed then return back(m) end
  if m.page ~= "title" and m.page ~= "ending" and m.page ~= "ending_credits" then
    if st("tab_next").pressed then return menu.turn_tab(m, 1) end
    if st("tab_prev").pressed then return menu.turn_tab(m, -1) end
  end
  if m.page == "map" then -- the stick moves the map, confirm zooms out
    local d = st("steer")
    local mx = (d.x or 0) > 0.5 and 1 or (d.x or 0) < -0.5 and -1 or 0
    local my = (d.y or 0) > 0.5 and 1 or (d.y or 0) < -0.5 and -1 or 0
    if mx == 0 and my == 0 then
      m.hold = 0
    else
      m.hold = m.hold - dt
      if m.hold <= 0 then
        m.hold = menu.REPEAT
        if m.hooks.map_pan then m.hooks.map_pan(mx, my) end
      end
    end
    if st("confirm").pressed and m.hooks.map_zoom then m.hooks.map_zoom() end
    return
  end
  local list = items(m)
  local dir = st("steer")
  local dy = (dir.y or 0) > 0.5 and -1 or (dir.y or 0) < -0.5 and 1 or 0
  local dx = (dir.x or 0) > 0.5 and 1 or (dir.x or 0) < -0.5 and -1 or 0
  if dy == 0 and dx == 0 then
    m.hold = 0
  else
    m.hold = m.hold - dt
    if m.hold <= 0 then
      m.hold = menu.REPEAT
      if dy ~= 0 and #list > 0 then m.at = (m.at - 1 + dy) % #list + 1 end
      if dx ~= 0 and list[m.at] and list[m.at].setting then activate(m, list[m.at], dx) end
    end
  end
  if st("confirm").pressed and list[m.at] then activate(m, list[m.at]) end
end

-- The menu's lines (English: they are translated when drawn).
function menu.lines(m, credits)
  local out = {}
  local title = { main = "Menu", settings = "Settings", controls = "Controls", packages = "Packages",
    credits = "Credits", title = "Zauberer", load = "Load game", save = m.chosen and { raw = m.chosen.name },
    confirm = m.chosen and { raw = string.format(i18n.t("Delete %s?"), m.chosen.name) }, help = "Help",
    reset = "Reset every setting to its default?", company = "Company",
    remove = m.chosen_heir and { raw = string.format(i18n.t("Remove %s from the session?"), m.chosen_heir.name) } }
  if m.page == "topic" then
    for _, t in ipairs(require("ui.help").TOPICS) do
      if t.id == m.topic then title.topic = t.title end
    end
  end
  out[1] = title[m.page]
  if m.page == "ending" or m.page == "ending_credits" then
    local body = m.page == "ending" and ((m.hooks.ending_lines and m.hooks.ending_lines()) or {}) or (credits or {})
    out[1] = m.page == "ending" and "The end of the story" or "Credits"
    for _, line in ipairs(body) do out[#out + 1] = line end
    out[#out + 1] = { raw = "> " .. i18n.t(items(m)[1].label) }
    return out
  end
  if m.page == "journal" then
    out[1] = "Journal"
    for _, line in ipairs((m.hooks.journal_lines and m.hooks.journal_lines()) or {}) do out[#out + 1] = line end
    out[#out + 1] = input.mode() == "touch" and "Menu: back" or "Escape: back"
    return out
  end
  if m.page == "topic" then
    for _, line in ipairs((m.hooks.topic_lines and m.hooks.topic_lines(m.topic)) or {}) do out[#out + 1] = line end
    out[#out + 1] = input.mode() == "touch" and "Menu: back" or "Escape: back"
    return out
  end
  if (m.page == "title" or m.page == "main") and m.hooks.version then
    out[#out + 1] = { raw = "v" .. m.hooks.version }
  end
  if m.page == "credits" then
    for _, line in ipairs(credits or {}) do out[#out + 1] = line end
    out[#out + 1] = "Escape: back"
    return out
  end
  for i, item in ipairs(items(m)) do
    local mark = i == m.at and "> " or "  "
    if item.setting then
      local value = value_text(item.setting, m.s[item.setting])
      if item.setting ~= "language" then value = i18n.t(value) end -- (a language is named in its own words)
      out[#out + 1] = { raw = mark .. i18n.t(item.label) .. ": " .. value }
    elseif item.action then
      local b = m.capture == item.action and i18n.t("press a key or button") or
        table.concat(input.bindings(item.action) or {}, ", ")
      out[#out + 1] = { raw = mark .. item.action .. ": " .. b }
    elseif item.save or item.heir then
      out[#out + 1] = { raw = mark .. item.raw }
    elseif item.package then
      local p = item.package
      out[#out + 1] = { raw = mark .. p.id .. " " .. (p.version or "") .. ": " ..
        i18n.t(p.turned_off and "off" or "on") ..
        (p.error and p.error ~= "turned off" and (" (" .. i18n.t(p.error) .. ")") or "") }
    else
      out[#out + 1] = { raw = mark .. i18n.t(item.label) }
    end
  end
  if m.note then out[#out + 1] = m.note end
  if input.mode() == "touch" then
    out[#out + 1] = m.page == "settings" and "Stick up and down: choose, left and right: change, OK: set, Menu: back"
      or "Stick up and down: choose, OK: open, Menu: back"
  else
      out[#out + 1] = m.page == "settings" and "Up and down: choose, left and right: change, Enter: set, Escape: back"
      or "Up and down: choose, Enter: open, Escape: back"
  end
  return out
end

return menu
