-- SPDX-License-Identifier: MIT
-- The UI's art kit in use: frames (nine-slice, from ui/frames.png), icons
-- (ui/icons.png), the palette, text with a shadow, key chips, the mana colours
-- of the chosen colour-vision mode and the high-contrast frames. Everything is
-- drawn on the UI grid (ui/layout.lua). The data comes from
-- tools/gen/gen_ui.py (ui/kit_data.lua).
local data = require("ui.kit_data")

local kit = { data = data }

local state = { frames = nil, icons = nil, font = nil, contrast = false, vision = "normal" }
kit.ADVANCE, kit.LINE = 6, 10 -- the pixel font's advance and a line's height (grid pixels at scale 1)

-- The font to draw with (the pixel font); textures load on first use.
function kit.set_font(font) state.font = font end

local function textures()
  if not state.frames then
    state.frames = render.texture(data.frames_texture)
    state.icons = render.texture(data.icons_texture)
  end
  return state.frames, state.icons
end

-- The settings that change the kit: high contrast frames, the colour-vision mode's mana colours.
function kit.configure(opts)
  state.contrast = opts.contrast and true or false
  state.vision = data.mana[opts.vision or "normal"] and opts.vision or "normal"
end

function kit.contrast() return state.contrast end

-- A palette colour by name, with alpha a (default 1).
function kit.colour(name, a)
  local c = data.palette[name] or data.palette.white
  return { c[1], c[2], c[3], a or c[4] }
end

-- A mana colour in the chosen colour-vision mode, and its glyph.
function kit.mana(name, a)
  local c = data.mana[state.vision][name] or data.palette.grey
  return { c[1], c[2], c[3], a or 1 }, data.glyphs[name] or "?"
end

-- A frame of a kind (kilim, thread, plate, chip, page, band, focus) over a box, tinted and faded.
function kit.frame(kind, x, y, w, h, tint, alpha)
  local f = data.frames[kind]
  assert(f, "no frame " .. tostring(kind))
  local frames = textures()
  local src = { f.src[1], f.src[2] + (state.contrast and data.contrast_offset or 0), f.src[3], f.src[4] }
  local c = tint or { 1, 1, 1, 1 }
  local colour = { c[1], c[2], c[3], (c[4] or 1) * (alpha or 1) }
  render.ui_nine(frames, x, y, w, h, { src = src, border = f.border, color = colour })
end

-- An icon at (x, y), scale times its 16 pixels, tinted and faded; false when there is no such icon.
function kit.icon(name, x, y, scale, tint, alpha)
  local at = data.icons[name]
  if not at then return false end
  local _, icons = textures()
  local s = data.icon_size
  local c = tint or { 1, 1, 1, 1 }
  render.ui_image(icons, x, y, { src = { at[1], at[2], s, s }, scale = scale or 1,
    color = { c[1], c[2], c[3], (c[4] or 1) * (alpha or 1) } })
  return true
end

function kit.has_icon(name) return data.icons[name] ~= nil end

-- The width of text in grid pixels.
function kit.width(s, scale)
  if not state.font then return #s * kit.ADVANCE * (scale or 1) end
  return #render.text_glyphs(state.font, s) * kit.ADVANCE * (scale or 1)
end

-- Text with a one-pixel ink shadow (an outline in high contrast); returns its width.
function kit.text(s, x, y, colour, scale, alpha)
  local c = colour or data.palette.cream
  local a = (c[4] or 1) * (alpha or 1)
  local ink = data.palette.ink
  scale = scale or 1
  if state.font then
    if state.contrast then
      for _, d in ipairs({ { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }) do
        render.draw_text(state.font, s, x + d[1] * scale, y + d[2] * scale, scale, { ink[1], ink[2], ink[3], a })
      end
      render.draw_text(state.font, s, x, y, scale, { c[1], c[2], c[3], a })
    else
      render.draw_text(state.font, s, x, y, scale, { c[1], c[2], c[3], a }, { ink[1], ink[2], ink[3], a })
    end
  end
  return kit.width(s, scale)
end

-- Text centred on cx.
function kit.text_c(s, cx, y, colour, scale, alpha)
  return kit.text(s, cx - kit.width(s, scale) // 2, y, colour, scale, alpha)
end

-- Words wrapped to width (grid pixels) as lines.
function kit.wrap(s, width, scale)
  local lines, line = {}, ""
  for word in s:gmatch("%S+") do
    local try = line == "" and word or (line .. " " .. word)
    if line ~= "" and kit.width(try, scale) > width then
      lines[#lines + 1] = line
      line = word
    else
      line = try
    end
  end
  if line ~= "" then lines[#lines + 1] = line end
  return lines
end

-- The icon that shows a binding ("mouse:left", "gamepad:a", "touch:button:confirm"), if any.
local BINDING_ICONS = { ["mouse:left"] = "mouse_left", ["mouse:right"] = "mouse_right", ["gamepad:a"] = "pad_a",
  ["gamepad:b"] = "pad_b", ["gamepad:x"] = "pad_x", ["gamepad:y"] = "pad_y", ["gamepad:left_shoulder"] = "pad_lb",
  ["gamepad:right_shoulder"] = "pad_rb", ["gamepad:left_trigger"] = "pad_lt", ["gamepad:right_trigger"] = "pad_rt" }

function kit.binding_icon(binding)
  if binding and binding:match("^touch:") then return "touch" end
  return BINDING_ICONS[binding]
end

-- A key or button chip at (x, y): the binding's icon, or its label in a chip frame. Returns its width.
function kit.chip(label, x, y, binding, alpha)
  local icon = kit.binding_icon(binding)
  if icon then
    kit.icon(icon, x, y - 2, 1, nil, alpha)
    return 16
  end
  local w = kit.width(label) + 6
  kit.frame("chip", x, y - 1, w, 11, nil, alpha)
  kit.text(label, x + 3, y + 1, data.palette.saffron, 1, alpha)
  return w
end

return kit
