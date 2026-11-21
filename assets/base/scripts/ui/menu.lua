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
  return tostring(v)
end

-- Labels of the settings, as the menu shows them (translated where they are drawn).
menu.LABELS = {
  language = "Language", text_size = "Text size", high_contrast = "High contrast", colour_vision = "Colour vision",
  colour_glyphs = "Colour glyphs", reduced_flashing = "Reduced flashing",
  captions = "Sound captions", toggle_hold = "Toggle instead of hold", gesture_assist = "Gesture assist",
  look_curve = "Look curve", look_speed = "Look speed", fov = "Field of view",
  story_markers = "Story markers", survival = "Survival", xr_steer = "VR steering",
  xr_snap_turn = "VR snap turning", xr_vignette = "VR comfort vignette", xr_reduced_speed = "VR reduced speed",
  xr_stabiliser = "VR horizon stabiliser",
}

-- s: the settings table; hooks: { quit(), resume(), packages(), set_package(id, on) }.
function menu.new(s, hooks)
  return { open = false, page = "main", at = 1, s = s, hooks = hooks or {}, hold = 0, last = 0, capture = nil,
    note = nil }
end

local function items(m)
  local out = {}
  if m.page == "main" then
    out = { { label = "Resume", go = "resume" }, { label = "Settings", page = "settings" },
      { label = "Controls", page = "controls" }, { label = "Packages", page = "packages" },
      { label = "Credits", page = "credits" }, { label = "Save and quit", go = "quit" } }
  elseif m.page == "settings" then
    for _, name in ipairs(settings.ORDER) do
      out[#out + 1] = { setting = name, label = menu.LABELS[name] }
    end
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
  m.page, m.at, m.capture, m.note = "main", 1, nil, nil
  return m.open
end

local function back(m)
  if m.capture then
    m.capture = nil
  elseif m.page == "main" then
    m.open = false
    if m.hooks.resume then m.hooks.resume() end
  else
    m.page, m.at = "main", 1
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
  if item.go == "resume" then
    back(m)
  elseif item.go == "quit" then
    if m.hooks.quit then m.hooks.quit() end
  elseif item.page then
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
    credits = "Credits" }
  out[1] = title[m.page]
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
  out[#out + 1] = m.page == "main" and "Up and down: choose, Enter: open, Escape: back"
    or "Up and down: choose, left and right: change, Enter: set, Escape: back"
  return out
end

return menu
