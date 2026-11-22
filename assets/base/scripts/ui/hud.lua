-- SPDX-License-Identifier: MIT
-- The HUD: each kind of information in its own framed zone (docs/design/ui).
--   top middle   the compass band (where the rider looks, the goal, places, the others)
--   top left     the objective card (the goal and its steps)
--   upper right  toasts (news, pickups, captions, hints; at most three, fading)
--   right edge   the carpet's gauge (height above the ground, speed), in flight
--   bottom       the two hands with the mana ring between them, the prompt chip above
--   bottom left  health and the needs that want something
-- hud.gather builds the state once a frame from the game; hud.draw draws it.
-- After CALM seconds without an event the HUD draws back to the compass, the
-- hands and health (never while danger is near).
local kit = require("ui.kit")
local layout = require("ui.layout")
local i18n = require("ui.i18n")

local hud = {}

hud.CALM = 8           -- seconds without an event before the HUD draws back
hud.TOAST_LIFE = 5     -- seconds a toast stays (it fades in its last second)
hud.MAX_TOASTS = 3
hud.DANGER_RADIUS = 40 -- metres: a hunter this near is danger
-- News of the world going on around (seen, observed, an animal fleeing) shows, but does not wake a calm HUD.
hud.AMBIENT = { "^Codex:", "^Looking at", "flees$" }

local function ambient(text)
  for _, pat in ipairs(hud.AMBIENT) do
    if text:find(pat) then return true end
  end
  return false
end

function hud.new()
  return { seen = {}, toasts = {}, last_event = -1e9, now = 0, calm = false, sig = "" }
end

local function bearing(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  return math.atan(b[1] - a[1], -(b[3] - a[3]))
end

local function deg(r) return (math.deg(r) % 360 + 360) % 360 end

-- New lines from the game's news become toasts (once each, by text); equal ones in a row merge.
local function add_toast(h, text, icon, now, quiet)
  local last = h.toasts[#h.toasts]
  if last and last.base == text and now - last.born < hud.TOAST_LIFE then
    last.count, last.born = last.count + 1, now
    return
  end
  quiet = quiet or ambient(text)
  if quiet and h.calm then return end -- (the world going on does not wake a calm HUD)
  h.toasts[#h.toasts + 1] = { base = text, icon = icon, born = now, count = 1 }
  while #h.toasts > hud.MAX_TOASTS do table.remove(h.toasts, 1) end
  if not quiet then h.last_event = now end
end

local function feed(h, lines, icon, now, quiet)
  for _, line in ipairs(lines or {}) do
    local text = type(line) == "table" and (line.raw or line.text) or line
    if type(text) == "string" and text ~= "" then
      if not h.seen[text] then add_toast(h, text, icon, now, quiet) end
      h.seen[text] = now
    end
  end
end

-- A line of news said (news.listeners): a toast, merged with an equal one before it.
function hud.push(h, text, now)
  if type(text) ~= "string" or text == "" then return end
  h.seen[text] = now
  add_toast(h, text, nil, now)
end

-- A line shown elsewhere (speech in its box): not a toast while it is about.
function hud.seen(h, text, now)
  if type(text) == "string" then h.seen[text] = now end
end

-- The HUD's state from the game this frame. src: what the game knows (see main.lua's hud_source).
function hud.gather(h, src, now)
  h.now = now
  local s = { flying = src.flying, on_foot = src.on_foot }
  -- The compass: where the rider looks; the goal; places near; the other heirs.
  s.compass = { heading = deg(src.look_heading or 0), marks = {}, night = src.night }
  if src.goal_pos then
    s.compass.marks[#s.compass.marks + 1] = { kind = "goal", bearing = deg(bearing(src.me, src.goal_pos)),
      label = src.goal_name, dist = world.distance(src.me, src.goal_pos) }
  end
  for _, p in ipairs(src.places or {}) do
    s.compass.marks[#s.compass.marks + 1] = { kind = "place", bearing = p.bearing, label = p.name }
  end
  for _, a in ipairs(src.allies or {}) do
    s.compass.marks[#s.compass.marks + 1] = { kind = "ally", bearing = deg(bearing(src.me, a.pos)), label = a.name }
  end
  -- The objective card.
  s.objective = src.objective
  -- Health and the needs that want something.
  s.vitals = { hp = src.hp, max = src.hp_max, needs = {} }
  for _, n in ipairs(src.needs or {}) do
    if n.word ~= "well" then s.vitals.needs[#s.vitals.needs + 1] = n end
  end
  -- The hands and the mana ring.
  s.hands = src.hands
  s.mana = src.mana
  -- The gauge, in flight.
  if src.flying then s.gauge = { height = src.height, speed = src.speed } end
  -- The prompt chip.
  s.prompt = src.prompt
  -- Toasts from the news.
  for _, group in ipairs(src.news or {}) do feed(h, group.lines, group.icon, now, group.ambient) end
  for i = #h.toasts, 1, -1 do
    if now - h.toasts[i].born > hud.TOAST_LIFE then table.remove(h.toasts, i) end
  end
  for text, t in pairs(h.seen) do -- (a line that comes back after a while is news again)
    if now - t > 60 then h.seen[text] = nil end
  end
  s.toasts = {}
  for i = #h.toasts, 1, -1 do
    local t = h.toasts[i]
    s.toasts[#s.toasts + 1] = { text = t.count > 1 and string.format("%s x%d", t.base, t.count) or t.base,
      icon = t.icon, alpha = math.min(1, hud.TOAST_LIFE - (now - t.born)) }
  end
  s.mods = src.mods or {}
  -- Events and danger decide the calm HUD.
  local sig = table.concat({ s.prompt and s.prompt.verb or "", #s.vitals.needs, s.objective and s.objective.title or "",
    src.device or "", src.hp and math.floor(src.hp) or 0 }, "|")
  if sig ~= h.sig then
    h.sig = sig
    h.last_event = now
  end
  s.danger = src.danger or (src.hp and src.hp_max and src.hp < 0.3 * src.hp_max) or false
  h.calm = not s.danger and now - h.last_event > hud.CALM
  s.calm = h.calm
  h.state = s
  return s
end

-- What tests and the headset read: the last state.
function hud.state(h) return h.state end

-- ---- Zones ---------------------------------------------------------------------------------

local function C(name, a) return kit.colour(name, a) end

local function draw_compass(st, w, alpha)
  local bw, x0, y = 220, math.floor(w / 2) - 110, layout.safe().y
  kit.frame("thread", x0, y, bw, 17, nil, alpha)
  local span, cx = 140.0, x0 + bw / 2
  local function at(b)
    local off = ((b - st.heading + 540) % 360) - 180
    return off, cx + off / span * (bw - 16)
  end
  for d = 0, 345, 15 do
    local off, x = at(d)
    if math.abs(off) <= span / 2 - 2 then
      if d % 90 == 0 then
        kit.text_c(({ "N", "E", "S", "W" })[d // 90 + 1], x, y + 5, d == 0 and C("saffron") or C("cream"), 1, alpha)
      elseif d % 45 == 0 then
        render.ui_rect(x, y + 6, 1, 5, C("parchment_dark", alpha))
      else
        render.ui_rect(x, y + 9, 1, 1, C("grey", alpha))
      end
    end
  end
  render.ui_rect(cx, y + 2, 1, 2, C("cream", alpha))
  kit.icon(st.night and "rest" or "heat", x0 + bw + 2, y, 1, nil, alpha)
  local labelled = 0
  for _, m in ipairs(st.marks) do
    local off = math.max(-span / 2 + 4, math.min(span / 2 - 4, (at(m.bearing))))
    local x = cx + off / span * (bw - 16)
    local icon = m.kind == "goal" and "goal" or (m.kind == "ally" and "ally" or "place")
    kit.icon(icon, math.floor(x) - 8, y + 13, 1, nil, alpha)
    if m.label and (m.kind ~= "place" or labelled == 0) then
      local label = i18n.t(m.label)
      if m.dist then label = string.format("%s %s", label, m.dist >= 1000 and string.format("%.1f km", m.dist / 1000)
        or string.format("%d m", math.floor(m.dist))) end
      local lw = kit.width(label) + 8
      local ly = y + 29 + labelled * 13
      kit.frame("thread", math.floor(x - lw / 2), ly - 3, lw, 13, nil, alpha)
      kit.text_c(label, x, ly, m.kind == "goal" and C("saffron") or C("turquoise"), 1, alpha)
      labelled = labelled + 1
    end
  end
end

local function draw_objective(o, calm, alpha)
  local s = layout.safe()
  local steps = calm and {} or (o.steps or {})
  local width = kit.width(i18n.t(o.title)) + 20
  for _, step in ipairs(steps) do width = math.max(width, kit.width(i18n.t(step.text)) + 30) end
  width = math.min(width, 200)
  local h = 22 + #steps * 10
  kit.frame("kilim", s.x, s.y, width, h, nil, alpha)
  local title = o.fresh and (i18n.t("New goal") .. ": ") or ""
  kit.text(title .. i18n.t(o.title), s.x + 9, s.y + 8, o.fresh and C("turquoise") or C("saffron"), 1, alpha)
  for i, step in ipairs(steps) do
    local y = s.y + 8 + i * 10
    kit.text(step.done and "x" or "-", s.x + 9, y, step.done and C("grey") or C("cream"), 1, alpha)
    kit.text(i18n.t(step.text), s.x + 19, y, step.done and C("parchment_dark") or C("cream"), 1, alpha)
  end
end

local function draw_toasts(toasts, w, y0)
  local s = layout.safe()
  local y = y0
  for _, t in ipairs(toasts) do
    local text = i18n.t(t.text)
    local tw = math.min(kit.width(text) + (t.icon and 26 or 10), math.floor(w * 0.45))
    local lines = kit.wrap(text, tw - (t.icon and 26 or 10))
    local h = 6 + #lines * 10
    local x = s.x + s.w - 32 - tw -- (clear of the carpet's gauge at the edge)
    kit.frame("thread", x, y, tw, h, nil, t.alpha)
    if t.icon then kit.icon(t.icon, x + 4, y + (h - 16) // 2, 1, nil, t.alpha) end
    for i, line in ipairs(lines) do
      kit.text(line, x + (t.icon and 22 or 5), y + 4 + (i - 1) * 10, C("cream"), 1, t.alpha)
    end
    y = y + h + 3
  end
  return y
end

local function draw_gauge(g, alpha)
  local s = layout.safe()
  local x, y, h = s.x + s.w - 26, s.y + math.floor(s.h * 0.28), 110
  kit.frame("thread", x, y - 13, 26, h + 26, nil, alpha)
  local top = 1000
  local frac = math.max(0, math.min(1, math.log(1 + math.max(0, g.height)) / math.log(1 + top)))
  for k = 0, 10 do
    render.ui_rect(x + 6, y + h - 8 - k * 9, k % 5 == 0 and 7 or 4, 1, C("parchment_dark", alpha))
  end
  local my = y + h - 8 - math.floor(frac * 90)
  render.ui_rect(x + 15, my - 1, 5, 3, C("saffron", alpha))
  kit.text_c(g.height >= 1000 and string.format("%.1fk", g.height / 1000) or string.format("%dm", math.floor(g.height)),
    x + 13, y - 9, C("cream"), 1, alpha)
  kit.text_c(tostring(math.floor(g.speed * 3.6 + 0.5)), x + 13, y + h, C("saffron"), 1, alpha)
end

local function draw_ring(mana, cx, cy, alpha)
  local r = 14
  kit.frame("thread", cx - r - 12, cy - r - 12, 2 * r + 25, 2 * r + 25, nil, alpha)
  -- The ring drawn as segments of short arcs of small squares (pixel art), a glyph outside each big one.
  local total, cap = 0, math.max(1, mana.cap or 1)
  for _, m in ipairs(mana.colours) do total = total + m.amount end
  local scale = math.max(cap, total)
  local a0 = -math.pi / 2
  for _, m in ipairs(mana.colours) do
    local sweep = 2 * math.pi * m.amount / scale
    local colour, glyph = kit.mana(m.name, alpha)
    for t = a0, a0 + sweep, 0.06 do
      for rr = r - 4, r do
        render.ui_rect(math.floor(cx + math.cos(t) * rr), math.floor(cy + math.sin(t) * rr), 1, 1, colour)
      end
    end
    if sweep > 0.45 then
      local mid = a0 + sweep / 2
      kit.text_c(glyph, cx + math.cos(mid) * (r + 7), cy + math.sin(mid) * (r + 7) - 4, colour, 1, alpha)
    end
    a0 = a0 + sweep
  end
  kit.frame("chip", cx - 10, cy - 6, 21, 12, nil, alpha)
  kit.text_c(tostring(math.floor(total + 0.5)), cx + 1, cy - 4, C("cream"), 1, alpha)
end

local function draw_hand(hand, cx, cy, alpha)
  local nw = math.max(40, kit.width(i18n.t(hand.name or "-")) + 10)
  kit.frame("thread", cx - nw // 2, cy - 18, nw, 64, nil, alpha)
  kit.frame("chip", cx - 14, cy - 14, 29, 29, nil, alpha)
  if not kit.icon(hand.icon or "spark", cx - 8, cy - 8, 1, nil, alpha) then
    kit.icon("spark", cx - 8, cy - 8, 1, nil, alpha)
  end
  if hand.charge and hand.charge > 0 then
    render.ui_rect(cx - 12, cy + 12, math.floor(24 * math.min(1, hand.charge)), 2, C("cream", alpha))
  end
  kit.text_c(i18n.t(hand.name or "-"), cx, cy + 17, C("cream"), 1, alpha)
  if hand.key then
    local w = kit.binding_icon(hand.binding) and 16 or (kit.width(hand.key) + 6)
    kit.chip(hand.key, cx - w // 2, cy + 30, hand.binding, alpha)
  end
end

local function draw_vitals(v, calm, alpha, bottom)
  local s = layout.safe()
  local needs = calm and {} or v.needs
  local h = 22 + (#needs > 0 and 12 or 0)
  local y = bottom - h
  kit.frame("kilim", s.x, y, 140, h, nil, alpha)
  kit.icon("health", s.x + 6, y + 3, 1, nil, alpha)
  local frac = v.max and v.max > 0 and math.max(0, math.min(1, v.hp / v.max)) or 0
  render.ui_rect(s.x + 24, y + 8, 106, 7, C("ink", alpha))
  render.ui_rect(s.x + 25, y + 9, math.floor(104 * frac), 5, C("pomegranate", alpha))
  render.ui_rect(s.x + 25, y + 9, math.floor(104 * frac), 1, C("saffron_light", alpha))
  local x = s.x + 8
  for _, n in ipairs(needs) do
    kit.icon(n.icon, x - 3, y + 17, 1, nil, alpha)
    local tone = n.word == "low" and C("saffron") or C("pomegranate")
    x = x + 12 + kit.text(i18n.t(n.label), x + 12, y + 21, tone, 1, alpha)
    x = x + 6
  end
end

local function draw_prompt(p, cy)
  local w, _ = layout.grid()
  local width = kit.width(p.key) + 6 + 4 + kit.width(i18n.t(p.verb))
  local x = math.floor(w / 2 - width / 2)
  kit.frame("thread", x - 5, cy - 4, width + 10, 17)
  local cw = kit.chip(p.key, x, cy, p.binding)
  kit.text(i18n.t(p.verb), x + cw + 4, cy, C("cream"))
  layout.claim({ x = x - 2, y = cy - 3, w = width + 4, h = 15 })
end

-- Draws the HUD of state st (from hud.gather).
function hud.draw(_, st)
  if not st then return end
  local w = layout.grid()
  local dim = st.calm and 0.55 or 1
  draw_compass(st.compass, w, dim)
  if st.objective and st.objective.title then draw_objective(st.objective, st.calm, dim) end
  if not st.calm then
    local y = draw_toasts(st.toasts, w, layout.safe().y + 22)
    if #st.mods > 0 then draw_toasts({ { text = table.concat(st.mods, "  "), alpha = 1 } }, w, y) end
  end
  if st.gauge and not st.calm then draw_gauge(st.gauge, dim) end
  -- The bottom: the hands at the corners (where a rider's hands are), the mana ring on the left
  -- wrist, health above the left hand; the middle is left to the carpet's front edge.
  local s = layout.safe()
  local bottom = s.y + s.h
  local cy = bottom - 46
  if input.mode() == "touch" then cy = bottom - 150 end -- (above the sticks and buttons a thumb uses)
  if st.hands then
    draw_hand(st.hands.left, s.x + 28, cy, dim)
    if st.mana then draw_ring(st.mana, s.x + 28 + 62, cy + 6, dim) end
    draw_hand(st.hands.right, s.x + s.w - 28, cy, dim)
  end
  if st.vitals and st.vitals.hp then draw_vitals(st.vitals, st.calm, dim, cy - 24) end
  if st.prompt and not st.calm then draw_prompt(st.prompt, bottom - 96) end
end

return hud
