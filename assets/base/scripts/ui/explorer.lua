-- SPDX-License-Identifier: MIT
-- The world explorer, a testing tool: the whole played planet as a globe
-- (dragged, spun and zoomed; settlements, sites and the carpet marked; the
-- land coloured by realm, culture or faith) beside a lore explorer over the
-- history database (each kind listed, each entity's fields with its ids as
-- links, what refers to it, its events and the lore written from it). The
-- globe swings to whatever has a place. It shows everything, whatever the heir
-- has learnt, and its words are not translated.
local history = require("people.history")
local lore = require("people.lore")
local land = require("people.land")
local kit = require("ui.kit")
local layout = require("ui.layout")

local explorer = {}

explorer.KINDS = { "culture", "language", "faith", "polity", "dynasty", "settlement", "figure", "artifact", "site",
  "event" }
explorer.NAMES = { culture = "Cultures", language = "Languages", faith = "Faiths", polity = "Realms",
  dynasty = "Dynasties", settlement = "Settlements", figure = "Figures", artifact = "Artifacts", site = "Sites",
  event = "Events" }
explorer.LAYERS = { "biomes", "realms", "cultures", "faiths" }
explorer.LAYER_OF = { realms = "cell_realm", cultures = "cell_culture", faiths = "cell_faith" }
explorer.MIN_DIST, explorer.MAX_DIST = 1.06, 5.0 -- the eye's distance from the centre, in planet radii
explorer.FOV = 40
explorer.PANEL = 0.44 -- the share of the screen the lore takes, on the right
explorer.ROW = 10     -- grid pixels per row
explorer.SPIN = 1.5   -- radians per second at full spin, the whole planet in view
explorer.ZOOM = 1.6   -- zoom rate (1/s) at full input
explorer.MAX_REFS = 40 -- rows of a kind referring to an entity before "and n more"
explorer.FILL = 10    -- points across a land cell when an overlay colours it

local SKIP = { id = true, index = true, name = true, seed = true, pos = true }
local CELL_FIELDS = { cell = true, cradle = true, capital_cell = true, core = true, place = true }

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end
local function dot(a, b) return a[1] * b[1] + a[2] * b[2] + a[3] * b[3] end
local function cross(a, b)
  return { a[2] * b[3] - a[3] * b[2], a[3] * b[1] - a[1] * b[3], a[1] * b[2] - a[2] * b[1] }
end
local function norm(v)
  local l = math.sqrt(dot(v, v))
  return l > 0 and { v[1] / l, v[2] / l, v[3] / l } or { 0, 0, 1 }
end

function explorer.new()
  return { open = false, lon = 0, lat = 0.35, dist = 4.2, layer = 1, stack = { { view = "index", at = 1, top = 1 } },
    hold = 0, refs = {} }
end

-- ---- the globe --------------------------------------------------------------

-- The globe's direction (render axes, y up) of planet-centred coordinates (+Z north).
function explorer.unit(x, y, z)
  return norm({ x, z, -y })
end

-- ... of a planet position.
function explorer.dir_of(pos)
  return explorer.unit(world.to_cartesian(pos))
end

-- Longitude and latitude (radians) of a globe direction.
function explorer.lonlat(v)
  return math.atan(v[1], v[3]), math.asin(clamp(v[2], -1, 1))
end

-- The camera orbiting the globe for a screen of w x h: the eye straight above the point looked at,
-- turned so the globe's middle (and that point) sit in the middle of the left part, the lore panel
-- taking the right.
function explorer.view(x, w, h)
  local cl = math.cos(x.lat)
  local back = { cl * math.sin(x.lon), math.sin(x.lat), cl * math.cos(x.lon) }
  local f0 = { -back[1], -back[2], -back[3] }
  local r0 = norm(cross(f0, { 0, 1, 0 }))
  local aspect = w / math.max(1, h)
  local t = math.tan(math.rad(explorer.FOV) / 2)
  local a = math.atan(explorer.PANEL * t * aspect)
  local ca, sa = math.cos(a), math.sin(a)
  local u = cross(r0, f0) -- (turned about the camera's own up)
  local f = { f0[1] * ca + r0[1] * sa, f0[2] * ca + r0[2] * sa, f0[3] * ca + r0[3] * sa }
  local r = { r0[1] * ca - f0[1] * sa, r0[2] * ca - f0[2] * sa, r0[3] * ca - f0[3] * sa }
  local eye = { back[1] * x.dist, back[2] * x.dist, back[3] * x.dist }
  local target = { eye[1] + f[1], eye[2] + f[2], eye[3] + f[3] }
  return { eye = eye, target = target, up = u, f = f, r = r, u = u, t = t, aspect = aspect, w = w, h = h,
    fov = explorer.FOV }
end

-- Where a point of the globe shows on the screen (grid pixels), nil behind the eye or the globe.
function explorer.project(v, p)
  if dot(p, v.eye) <= dot(p, p) then return nil end -- (the far side)
  local d = { p[1] - v.eye[1], p[2] - v.eye[2], p[3] - v.eye[3] }
  local z = dot(d, v.f)
  if z <= 1e-6 then return nil end
  local sx = dot(d, v.r) / (z * v.t * v.aspect)
  local sy = dot(d, v.u) / (z * v.t)
  return (sx + 1) / 2 * v.w, (1 - sy) / 2 * v.h
end

-- Turns the globe so a direction faces the eye (eased in over the next ticks).
function explorer.look_at(x, v)
  x.goal_lon, x.goal_lat = explorer.lonlat(v)
  x.mark = v
end

-- The colour standing for an entity on the overlays.
local HUES = { { 0.85, 0.33, 0.25 }, { 0.25, 0.55, 0.85 }, { 0.95, 0.75, 0.2 }, { 0.35, 0.75, 0.4 },
  { 0.7, 0.4, 0.8 }, { 0.2, 0.75, 0.75 }, { 0.9, 0.55, 0.7 }, { 0.6, 0.45, 0.25 }, { 0.55, 0.85, 0.2 },
  { 0.45, 0.45, 0.95 }, { 0.95, 0.5, 0.15 }, { 0.75, 0.75, 0.75 } }
function explorer.hue(id)
  local h = 7
  for i = 1, #id do h = (h * 31 + id:byte(i)) % 1000003 end
  return HUES[h % #HUES + 1]
end

-- ---- the lore ---------------------------------------------------------------

-- Sets what the explorer shows: the history database and the land grid (both may be nil while the
-- world is being peopled), and where the carpet is.
function explorer.set_source(x, db, grid, carpet_pos)
  if x.db ~= db then
    x.db, x.grid, x.refs, x.marks, x.cells, x.cells_next = db, grid, {}, nil, nil, nil
    for _, e in ipairs(x.stack) do e.rows = nil end
  end
  x.carpet = carpet_pos
end

local function name_of(db, id)
  local r = db and history.get(db, id)
  if not r then return tostring(id) end
  return r.name or r.title or id
end

-- The globe direction of an entity or a cell, if it has a place.
function explorer.place_of(x, id, depth)
  local db, grid = x.db, x.grid
  if not db then return nil end
  if type(id) == "number" then
    local c = grid and grid.cells and grid.cells[id]
    return c and c.dir and explorer.unit(c.dir[1], c.dir[2], c.dir[3]) or nil
  end
  local r = history.get(db, id)
  if not r then return nil end
  if type(r.pos) == "table" and r.pos.face then return explorer.dir_of(r.pos) end
  if (depth or 0) > 2 then return nil end
  for _, k in ipairs({ "capital", "place", "lost_at", "burial", "polity", "culture" }) do
    local v = r[k]
    if v ~= nil and v ~= id then
      local p = explorer.place_of(x, v, (depth or 0) + 1)
      if p then return p end
    end
  end
  for k in pairs(CELL_FIELDS) do
    if type(r[k]) == "number" then return explorer.place_of(x, r[k]) end
  end
  return nil
end

-- Every entity referring to an id, by kind.
function explorer.referring(x, id)
  if x.refs[id] then return x.refs[id] end
  local out = {}
  local function names(v)
    if v == id then return true end
    if type(v) == "table" then
      for k, w in pairs(v) do
        if w == id or k == id then return true end
      end
    end
    return false
  end
  for _, kind in ipairs(explorer.KINDS) do
    local list = kind == "event" and x.db.events or x.db[kind]
    for _, r in ipairs(list or {}) do
      if r.id ~= id then
        for k, v in pairs(r) do
          if k ~= "id" and names(v) then
            out[kind] = out[kind] or {}
            table.insert(out[kind], r)
            break
          end
        end
      end
    end
  end
  x.refs[id] = out
  return out
end

local function short(v)
  if type(v) == "number" then
    return math.type(v) == "integer" and tostring(v) or string.format("%.2f", v)
  end
  return tostring(v)
end

-- One line of a list: the entity's name and what tells it apart.
function explorer.label(_, r)
  if history.kind_of(r.id) == "event" then
    return string.format("%5d  %s", r.year or 0, r.title or r.kind or r.id)
  end
  local extra = r.type or r.role or r.kind or r.family or ""
  if r.population then extra = extra .. " " .. r.population end
  if r.epithet then extra = r.epithet end
  return (r.name or r.id) .. (extra ~= "" and ("  (" .. extra .. ")") or "")
end

local function add(rows, text, go, colour)
  rows[#rows + 1] = { text = text, go = go, colour = colour }
end

local function prose(rows, lines, width)
  for _, line in ipairs(lines) do
    for _, piece in ipairs(kit.wrap(tostring(line), width)) do add(rows, "  " .. piece, nil, "parchment") end
  end
end

-- The lore written from an entity, as lines.
function explorer.lore_of(db, r)
  local kind = history.kind_of(r.id)
  local out = {}
  local function try(f, ...)
    local ok, v = pcall(f, ...)
    return ok and v or nil
  end
  if kind == "polity" then
    local book = try(lore.book_of_kings, db, r.id, r.culture)
    if type(book) == "table" then
      out[#out + 1] = "Book of kings, as its people tell it:"
      for _, l in ipairs(book) do out[#out + 1] = type(l) == "table" and (l.text or l[1] or "") or l end
    end
  elseif kind == "site" then
    local ins = try(lore.inscription, db, r.id)
    if type(ins) == "table" then
      out[#out + 1] = "Inscription: " .. tostring(ins.native or "")
      out[#out + 1] = "It says: " .. tostring(ins.gloss or "")
    end
  elseif kind == "event" then
    local is_war = false
    for _, w in ipairs(db.wars or {}) do if w == r.id then is_war = true end end
    if is_war then
      for _, side in ipairs({ r.winner, r.loser }) do
        local who = history.get(db, side)
        local acc = who and try(lore.war_account, db, r.id, who.culture)
        if type(acc) == "table" then
          out[#out + 1] = (acc.title or "A war") .. ", as " .. name_of(db, who.culture) .. " tell it:"
          for _, l in ipairs(acc.lines or {}) do out[#out + 1] = l end
        end
      end
    end
    local first = history.get(db, r.participants and r.participants[1] or "")
    local teller = first and (first.culture or (history.kind_of(first.id) == "culture" and first.id)) or nil
    local legend = try(lore.legend, db, r.id, teller, 300, 2000000)
    if type(legend) == "string" and legend ~= "" then out[#out + 1] = "Told far away, long after: " .. legend end
  end
  return out
end

-- The rows of a view: { text, go (where it leads), colour }.
function explorer.rows(x, e, width)
  local db, rows = x.db, {}
  width = width or 270
  if not db then
    add(rows, "The world is still being peopled...", nil, "grey")
    return rows
  end
  if e.view == "index" then
    local info = world.info and world.info() or {}
    add(rows, string.format("Seed %s  -  year %s  -  %s", tostring(info.seed or db.seed), short(db.year or 0),
      db.variant or "?"), nil, "saffron")
    if info.name then add(rows, "World: " .. tostring(info.name), nil, "parchment") end
    for _, era in ipairs(db.eras or {}) do
      local t = type(era) == "table" and (era.name or era.title) or era
      if t then add(rows, "Era: " .. tostring(t) .. (type(era) == "table" and era.from and
        string.format(" (%s to %s)", short(era.from), short(era.to or "")) or ""), nil, "parchment") end
    end
    add(rows, string.format("Roads %d  -  borders %d  -  wars %d", #(db.roads or {}), #(db.borders or {}),
      #(db.wars or {})), nil, "parchment")
    for _, k in ipairs(explorer.KINDS) do
      local n = #(k == "event" and db.events or db[k] or {})
      add(rows, string.format("%s (%d)", explorer.NAMES[k], n), { view = "list", of = k })
    end
  elseif e.view == "list" then
    local list = {}
    for _, r in ipairs(e.of == "event" and db.events or db[e.of] or {}) do list[#list + 1] = r end
    if e.of == "event" then
      table.sort(list, function(a, b)
        if (a.year or 0) ~= (b.year or 0) then return (a.year or 0) < (b.year or 0) end
        return a.id < b.id
      end)
    else
      table.sort(list, function(a, b) return tostring(a.name or a.id) < tostring(b.name or b.id) end)
    end
    for _, r in ipairs(list) do add(rows, explorer.label(db, r), { view = "entity", id = r.id }) end
  elseif e.view == "entity" then
    local r = history.get(db, e.id)
    if not r then
      add(rows, "Nothing is " .. tostring(e.id), nil, "grey")
      return rows
    end
    add(rows, string.format("%s  [%s]", r.name or r.title or r.id, r.id), nil, "saffron")
    if type(r.pos) == "table" and r.pos.face then
      local lon, lat = explorer.lonlat(explorer.dir_of(r.pos))
      add(rows, string.format("position: %.1f N, %.1f E", math.deg(lat), math.deg(lon)), nil, "parchment")
    end
    local keys = {}
    for k in pairs(r) do if not SKIP[k] then keys[#keys + 1] = k end end
    table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
    for _, k in ipairs(keys) do
      local v = r[k]
      if type(v) == "string" and db.by_id[v] then
        add(rows, k .. ": " .. name_of(db, v), { view = "entity", id = v })
      elseif type(v) == "number" and CELL_FIELDS[k] then
        add(rows, k .. ": cell " .. v, { view = "cell", cell = v })
      elseif type(v) ~= "table" then
        for i, piece in ipairs(kit.wrap(k .. ": " .. short(v), width)) do
          add(rows, (i > 1 and "  " or "") .. piece, nil, "parchment")
        end
      elseif #v > 0 then
        local links = type(v[1]) == "string" and db.by_id[v[1]] ~= nil
        if links then
          for _, id in ipairs(v) do
            if type(id) == "string" and db.by_id[id] then
              add(rows, k .. ": " .. name_of(db, id), { view = "entity", id = id })
            end
          end
        else
          local parts = {}
          for _, item in ipairs(v) do
            parts[#parts + 1] = type(item) == "table" and (item.name or item.title or item.id or "{...}") or short(item)
          end
          for i, piece in ipairs(kit.wrap(k .. ": " .. table.concat(parts, ", "), width)) do
            add(rows, (i > 1 and "  " or "") .. piece, nil, "parchment")
          end
        end
      else
        local sub = {}
        for k2 in pairs(v) do sub[#sub + 1] = k2 end
        table.sort(sub, function(a, b) return tostring(a) < tostring(b) end)
        if #sub > 0 then add(rows, k .. ":", nil, "parchment") end
        for _, k2 in ipairs(sub) do
          local w = v[k2]
          if type(k2) == "string" and db.by_id[k2] then
            add(rows, "  " .. name_of(db, k2) .. (type(w) ~= "table" and (": " .. short(w)) or ""),
              { view = "entity", id = k2 })
          elseif type(w) == "string" and db.by_id[w] then
            add(rows, "  " .. tostring(k2) .. ": " .. name_of(db, w), { view = "entity", id = w })
          elseif type(w) ~= "table" then
            add(rows, "  " .. tostring(k2) .. ": " .. short(w), nil, "parchment")
          end
        end
      end
    end
    -- What refers to it.
    local refs = explorer.referring(x, r.id)
    for _, kind in ipairs(explorer.KINDS) do
      local list = refs[kind]
      if list then
        add(rows, string.format("%s naming it (%d)", explorer.NAMES[kind], #list), nil, "turquoise")
        if kind == "event" then
          table.sort(list, function(a, b) return (a.year or 0) < (b.year or 0) end)
        end
        for i, o in ipairs(list) do
          if i > explorer.MAX_REFS then
            add(rows, string.format("  ... and %d more", #list - explorer.MAX_REFS), nil, "grey")
            break
          end
          add(rows, "  " .. explorer.label(db, o), { view = "entity", id = o.id })
        end
      end
    end
    -- The lore written from it.
    local texts = explorer.lore_of(db, r)
    if #texts > 0 then
      add(rows, "Lore", nil, "turquoise")
      prose(rows, texts, width - 12)
    end
  elseif e.view == "cell" then
    local c = x.grid and x.grid.cells and x.grid.cells[e.cell]
    add(rows, "Cell " .. tostring(e.cell), nil, "saffron")
    if c then
      for _, k in ipairs({ "class", "biome", "height", "temp", "rain", "coast", "river", "relief" }) do
        if c[k] ~= nil and type(c[k]) ~= "table" then add(rows, k .. ": " .. short(c[k]), nil, "parchment") end
      end
    end
    for _, layer in ipairs({ "cell_realm", "cell_culture", "cell_faith" }) do
      local id = db[layer] and db[layer][e.cell]
      if id then add(rows, layer:sub(6) .. ": " .. name_of(db, id), { view = "entity", id = id }) end
    end
    for _, id in ipairs((db.cell_settlements or {})[e.cell] or {}) do
      if type(id) == "string" then add(rows, "settlement: " .. name_of(db, id), { view = "entity", id = id }) end
    end
    for _, ev in ipairs(history.events(db, { place = e.cell })) do
      add(rows, explorer.label(db, ev), { view = "entity", id = ev.id })
    end
  end
  return rows
end

-- ---- navigating -------------------------------------------------------------

function explorer.top(x) return x.stack[#x.stack] end

local function rows_of(x, e)
  if not e.rows then
    e.rows = explorer.rows(x, e, x.text_width)
    if e.rows[e.at] and not e.rows[e.at].go then -- (a fresh view: on the first row that leads somewhere)
      for i, r in ipairs(e.rows) do
        if r.go then
          e.at = i
          break
        end
      end
    end
  end
  return e.rows
end
explorer.rows_of = rows_of

-- Where a view puts the globe.
local function focus(x, e)
  local v
  if e.view == "entity" then v = explorer.place_of(x, e.id)
  elseif e.view == "cell" then v = explorer.place_of(x, e.cell) end
  if v then explorer.look_at(x, v) end
end

function explorer.push(x, go)
  local e = { view = go.view, of = go.of, id = go.id, cell = go.cell, at = 1, top = 1 }
  x.stack[#x.stack + 1] = e
  rows_of(x, e)
  focus(x, e)
  return e
end

-- Back one view; false at the top.
function explorer.back(x)
  if #x.stack <= 1 then return false end
  table.remove(x.stack)
  focus(x, explorer.top(x))
  return true
end

function explorer.open(x)
  x.open = true
end

function explorer.close(x)
  x.open = false
  x.drag = nil
end

-- Moves the selection by n rows (to the rows that lead somewhere, if there are any).
function explorer.move(x, n)
  local e = explorer.top(x)
  local rows = rows_of(x, e)
  if #rows == 0 then return end
  local any = false
  for _, r in ipairs(rows) do if r.go then any = true end end
  local step = n > 0 and 1 or -1
  for _ = 1, math.abs(n) do
    local i = e.at
    repeat
      i = i + step
    until i < 1 or i > #rows or not any or rows[i].go
    if i >= 1 and i <= #rows then e.at = i end
  end
end

function explorer.choose(x)
  local e = explorer.top(x)
  local r = rows_of(x, e)[e.at]
  if r and r.go then explorer.push(x, r.go) end
end

function explorer.zoom(x, k)
  x.dist = clamp(1 + (x.dist - 1) * k, explorer.MIN_DIST, explorer.MAX_DIST)
end

-- The screen's parts: the globe on the left, the lore panel on the right (grid pixels).
function explorer.boxes(w, h)
  local s = layout.safe()
  local px = math.floor(w * (1 - explorer.PANEL))
  local panel = { x = px, y = s.y + 4, w = s.x + s.w - px - 4, h = s.h - 8 }
  local gy = s.y + s.h - 26
  return {
    panel = panel,
    list = { x = panel.x + 8, y = panel.y + 22, w = panel.w - 16, h = panel.h - 30 },
    globe = { x = 0, y = 0, w = px, h = h },
    zoom_in = { x = s.x + 6, y = gy, w = 20, h = 20 },
    zoom_out = { x = s.x + 30, y = gy, w = 20, h = 20 },
    layer = { x = s.x + 54, y = gy, w = 90, h = 20 },
    close = { x = s.x + 6, y = s.y + 6, w = 50, h = 16 },
    back = { x = s.x + 60, y = s.y + 6, w = 44, h = 16 },
  }
end

local function marks(x)
  if x.marks or not x.db then return x.marks or {} end
  local m = {}
  local capitals = {}
  for _, p in ipairs(x.db.polity or {}) do if p.capital then capitals[p.capital] = true end end
  for _, st in ipairs(x.db.settlement or {}) do
    if type(st.pos) == "table" and st.pos.face then
      m[#m + 1] = { id = st.id, v = explorer.dir_of(st.pos), capital = capitals[st.id], kind = "settlement",
        size = (st.population or 0) > 2000 and 3 or 2, name = st.name }
    end
  end
  for _, site in ipairs(x.db.site or {}) do
    if type(site.pos) == "table" and site.pos.face then
      m[#m + 1] = { id = site.id, v = explorer.dir_of(site.pos), kind = "site", size = 1 }
    end
  end
  x.marks = m
  return m
end
explorer.marks = marks

-- The land cells, each as points across it (for the overlays), worked out a few cells a frame.
explorer.CELLS_PER_FRAME = 8
local function cells(x)
  if not (x.grid and x.grid.cells and x.grid.n) then return {} end
  x.cells = x.cells or {}
  x.cells_next = x.cells_next or 1
  local out, n = x.cells, explorer.FILL
  local last = math.min(#x.grid.cells, x.cells_next + explorer.CELLS_PER_FRAME - 1)
  for i = x.cells_next, last do
    local c = x.grid.cells[i]
    local pts = {}
    for b = 0, n - 1 do
      for a = 0, n - 1 do
        local pos = land.point(x.grid, c, (a + 0.5) / n, (b + 0.5) / n)
        if world.height(pos) > 0 then pts[#pts + 1] = explorer.dir_of(pos) end -- (the land, not the sea)
      end
    end
    if #pts > 0 then out[#out + 1] = { i = c.id or i, pts = pts, step = explorer.dir_of(land.point(x.grid, c, 0, 0)),
      step2 = explorer.dir_of(land.point(x.grid, c, 1 / n, 1 / n)) } end
  end
  x.cells_next = last + 1
  return out
end

-- The settlement shown nearest a screen point (within 6 grid pixels), if any.
function explorer.hit(x, v, sx, sy)
  local best, bd = nil, 36
  for _, mk in ipairs(marks(x)) do
    if mk.kind == "settlement" then
      local px, py = explorer.project(v, mk.v)
      if px then
        local d = (px - sx) ^ 2 + (py - sy) ^ 2
        if d < bd then best, bd = mk, d end
      end
    end
  end
  return best
end

local function inside(b, px, py) return px >= b.x and px < b.x + b.w and py >= b.y and py < b.y + b.h end

-- One tick. st(name): an action's state (read past the block the menu and the explorer put on the
-- heir); p: the pointer in grid pixels (layout.pointer), or nil.
function explorer.tick(x, dt, st, p)
  local w, h = layout.grid()
  local b = explorer.boxes(w, h)
  x.text_width = b.list.w
  if st("explorer").pressed then
    explorer.close(x)
    return
  end
  if st("quit").pressed then
    if not explorer.back(x) then explorer.close(x) end
    return
  end
  local e = explorer.top(x)
  local rows = rows_of(x, e)
  local visible = math.max(1, b.list.h // explorer.ROW)
  -- The rows: up and down (held, repeating), Enter.
  local y = (st("steer").y or 0)
  if math.abs(y) > 0.5 then
    if x.hold == 0 or x.hold > 0.35 then explorer.move(x, y > 0 and -1 or 1) end
    x.hold = x.hold + dt
    if x.hold > 0.35 then x.hold = x.hold - 0.05 end
  else
    x.hold = 0
  end
  if st("confirm").pressed then explorer.choose(x) end
  if st("tab_next").pressed then x.layer = x.layer % #explorer.LAYERS + 1 end
  if st("tab_prev").pressed then x.layer = (x.layer - 2) % #explorer.LAYERS + 1 end
  -- The globe: spun with the keys, zoomed with climb.
  local spin = st("steer").x or 0
  if spin ~= 0 then
    x.lon = x.lon + spin * explorer.SPIN * dt * (x.dist - 1) / 2
    x.goal_lon = nil
  end
  local climb = st("climb").x or 0
  if climb ~= 0 then explorer.zoom(x, math.exp(-climb * explorer.ZOOM * dt)) end
  -- The pointer: rows, the wheel, dragging and clicking the globe, the chips.
  if p then
    if inside(b.list, p.x, p.y) then
      local i = e.top + math.floor((p.y - b.list.y) / explorer.ROW)
      if p.wheel and p.wheel ~= 0 then
        e.top = e.top - math.floor(p.wheel * 3 + 0.5)
      elseif p.pressed and rows[i] then
        e.at = i
        if rows[i].go then explorer.push(x, rows[i].go) end
      end
    elseif p.pressed and inside(b.zoom_in, p.x, p.y) then explorer.zoom(x, 0.7)
    elseif p.pressed and inside(b.zoom_out, p.x, p.y) then explorer.zoom(x, 1 / 0.7)
    elseif p.pressed and inside(b.layer, p.x, p.y) then x.layer = x.layer % #explorer.LAYERS + 1
    elseif p.pressed and inside(b.close, p.x, p.y) then
      explorer.close(x)
      return
    elseif p.pressed and inside(b.back, p.x, p.y) then explorer.back(x)
    elseif inside(b.globe, p.x, p.y) then
      if p.wheel and p.wheel ~= 0 then explorer.zoom(x, 0.85 ^ p.wheel) end
      if p.pressed then x.drag = { x = p.x, y = p.y, moved = 0 } end
    end
    if x.drag and p.held then
      local rate = 2.2 * (x.dist - 1) / math.max(1, h)
      local dx, dy = p.x - x.drag.x, p.y - x.drag.y
      x.lon, x.lat = x.lon - dx * rate, clamp(x.lat + dy * rate, -1.35, 1.35)
      x.drag.moved = x.drag.moved + math.abs(dx) + math.abs(dy)
      x.drag.x, x.drag.y = p.x, p.y
      if dx ~= 0 or dy ~= 0 then x.goal_lon = nil end
    end
    if x.drag and (p.released or not p.held) then
      if x.drag.moved < 3 then -- a click, not a drag: a settlement under it opens
        local mk = explorer.hit(x, explorer.view(x, w, h), p.x, p.y)
        if mk then explorer.push(x, { view = "entity", id = mk.id }) end
      end
      x.drag = nil
    end
  end
  -- The globe swings to what was opened.
  if x.goal_lon then
    local k = math.min(1, 6 * dt)
    local d = ((x.goal_lon - x.lon + math.pi) % (2 * math.pi)) - math.pi
    x.lon = x.lon + d * k
    x.lat = x.lat + (clamp(x.goal_lat, -1.35, 1.35) - x.lat) * k
    if math.abs(d) < 1e-3 and math.abs(x.goal_lat - x.lat) < 1e-3 then x.goal_lon = nil end
  end
  -- Keep the selection in view.
  e = explorer.top(x)
  rows = rows_of(x, e)
  if st("steer").y ~= 0 or st("confirm").pressed then
    if e.at < e.top then e.top = e.at end
    if e.at >= e.top + visible then e.top = e.at - visible + 1 end
  end
  e.top = clamp(e.top, 1, math.max(1, #rows - visible + 1))
end

-- ---- drawing ----------------------------------------------------------------

local function dot_at(px, py, size, c, a)
  render.ui_rect(math.floor(px - size / 2), math.floor(py - size / 2), size, size, { c[1], c[2], c[3], a or 1 })
end

-- Draws the explorer: the globe (3D), its marks, and the lore panel.
function explorer.draw(x)
  local w, h = layout.grid()
  local b = explorer.boxes(w, h)
  local v = explorer.view(x, w, h)
  render.camera({ eye = v.eye, target = v.target, up = v.up, fov = v.fov, near = 0.004, far = 20 })
  local shown = world.draw_globe(nil, { 0, 0, 0 }, 1, 0)
  if not shown then kit.text("(no globe for this world)", 20, h // 2, kit.colour("grey")) end
  -- The overlay: the land coloured by whose it is.
  local layer = explorer.LAYERS[x.layer]
  local map = x.db and explorer.LAYER_OF[layer] and x.db[explorer.LAYER_OF[layer]]
  if map then
    for _, c in ipairs(cells(x)) do
      local id = map[c.i]
      if id then
        local hue = explorer.hue(id)
        local ax, ay = explorer.project(v, c.step)
        local bx, by = explorer.project(v, c.step2)
        local gap = (ax and bx) and math.max(math.abs(ax - bx), math.abs(ay - by)) or 3
        local size = clamp(math.floor(gap * 0.6), 1, 40) -- (opaque dots apart: the land shows between them)
        for _, p in ipairs(c.pts) do
          local px, py = explorer.project(v, p)
          if px and px < b.panel.x then dot_at(px, py, size, hue) end
        end
      end
    end
  end
  -- The marks: settlements, capitals, sites, the carpet, what is open.
  local near = x.dist < 1.7
  local labels = 0
  for _, mk in ipairs(marks(x)) do
    if mk.kind ~= "site" or near then
      local px, py = explorer.project(v, mk.v)
      if px and px < b.panel.x then
        if mk.kind == "site" then
          dot_at(px, py, 1, kit.colour("turquoise"))
        elseif mk.capital then
          dot_at(px, py, 4, kit.colour("saffron"))
          if near and labels < 40 then
            labels = labels + 1
            kit.text(mk.name or "", math.floor(px) + 3, math.floor(py) - 4, kit.colour("saffron_light"))
          end
        else
          dot_at(px, py, mk.size + (near and 1 or 0), kit.colour("cream"), 0.9)
        end
      end
    end
  end
  if x.carpet then
    local px, py = explorer.project(v, explorer.dir_of(x.carpet))
    if px then
      local red = kit.colour("pomegranate")
      render.ui_rect(math.floor(px) - 4, math.floor(py), 9, 1, red)
      render.ui_rect(math.floor(px), math.floor(py) - 4, 1, 9, red)
    end
  end
  if x.mark then
    local px, py = explorer.project(v, x.mark)
    if px then -- (a ring around what is open)
      local sx, sy, c = math.floor(px), math.floor(py), kit.colour("saffron_light")
      render.ui_rect(sx - 7, sy - 7, 15, 1, c)
      render.ui_rect(sx - 7, sy + 7, 15, 1, c)
      render.ui_rect(sx - 7, sy - 7, 1, 15, c)
      render.ui_rect(sx + 7, sy - 7, 1, 15, c)
    end
  end
  -- The chips.
  kit.frame("chip", b.zoom_in.x, b.zoom_in.y, b.zoom_in.w, b.zoom_in.h)
  kit.text_c("+", b.zoom_in.x + 10, b.zoom_in.y + 6)
  kit.frame("chip", b.zoom_out.x, b.zoom_out.y, b.zoom_out.w, b.zoom_out.h)
  kit.text_c("-", b.zoom_out.x + 10, b.zoom_out.y + 6)
  kit.frame("chip", b.layer.x, b.layer.y, b.layer.w, b.layer.h)
  kit.text_c("Q/E " .. layer, b.layer.x + b.layer.w // 2, b.layer.y + 6)
  kit.frame("chip", b.close.x, b.close.y, b.close.w, b.close.h)
  kit.text_c("F4 close", b.close.x + b.close.w // 2, b.close.y + 4)
  if #x.stack > 1 then
    kit.frame("chip", b.back.x, b.back.y, b.back.w, b.back.h)
    kit.text_c("Back", b.back.x + b.back.w // 2, b.back.y + 4)
  end
  -- The lore panel.
  local pn = b.panel
  kit.frame("kilim", pn.x, pn.y, pn.w, pn.h)
  local e = explorer.top(x)
  local crumbs = {}
  for i, s in ipairs(x.stack) do
    crumbs[i] = s.view == "index" and "World" or s.view == "list" and explorer.NAMES[s.of]
      or s.view == "cell" and ("cell " .. s.cell) or name_of(x.db, s.id)
  end
  local title = table.concat(crumbs, " > ")
  if kit.width(title) > pn.w - 16 then title = "... > " .. crumbs[#crumbs] end
  kit.text(title, pn.x + 8, pn.y + 8, kit.colour("saffron"))
  local rows = rows_of(x, e)
  local visible = math.max(1, b.list.h // explorer.ROW)
  render.ui_clip(b.list.x - 2, b.list.y - 1, b.list.w + 4, b.list.h + 2)
  for i = e.top, math.min(#rows, e.top + visible - 1) do
    local r = rows[i]
    local ry = b.list.y + (i - e.top) * explorer.ROW
    if i == e.at then kit.frame("focus", b.list.x - 2, ry - 1, b.list.w + 4, explorer.ROW) end
    kit.text(r.text, b.list.x, ry + 1, kit.colour(r.colour or (r.go and "cream" or "parchment")))
  end
  render.ui_unclip()
  if #rows > visible then
    local th = math.max(6, b.list.h * visible // #rows)
    local ty = b.list.y + (b.list.h - th) * (e.top - 1) // math.max(1, #rows - visible)
    render.ui_rect(pn.x + pn.w - 6, ty, 2, th, kit.colour("saffron", 0.8))
  end
end

return explorer
