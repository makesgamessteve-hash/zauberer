-- SPDX-License-Identifier: MIT
-- Languages: text is translated where it is shown. A language is a folder
-- locale/<code>/ in any package: language.lua names it ({ name, plural }) and
-- every other file there is a string table, merged in package order, that
-- maps the English text to the language's text. Keys may be format frames:
-- "Bag %d/%d folds: %s" matches "Bag 3/40 folds: herbs", and the values the
-- frame captured are translated in turn (a list "a, b" item by item). A
-- translation takes the values in order (%s, %d) or by number ({1}, {2}) when
-- the words go in another order. An entry can be a table of plural forms
-- ({ one = ..., other = ... }) chosen by the language's rule and the first
-- number captured. English needs no table; text without an entry is shown as
-- it is (and, when collecting, remembered for the translators).
local i18n = {}

i18n.DEFAULT = "en"
i18n.CACHE = 4096 -- translated lines kept before the cache starts again

-- Plural rules: which form a count takes.
i18n.PLURAL = {
  one_other = function(n) return n == 1 and "one" or "other" end, -- English, German, ...
  one_few_many = function(n) -- (for packages that add such languages)
    local m10, m100 = n % 10, n % 100
    if m10 == 1 and m100 ~= 11 then return "one" end
    if m10 >= 2 and m10 <= 4 and (m100 < 12 or m100 > 14) then return "few" end
    return "many"
  end,
  other = function() return "other" end,
}

local current = { code = i18n.DEFAULT, name = "English", plural = i18n.PLURAL.one_other, exact = {}, frames = {} }
local cache, cached = {}, 0
local collecting = nil

local function read_table(path)
  local text = vfs.read(path)
  if not text then return nil end
  local chunk = load(text, "=" .. path, "t", {})
  if not chunk then return nil end
  local ok, t = pcall(chunk)
  return ok and type(t) == "table" and t or nil
end

-- A format frame as a Lua pattern: literal text, and a capture for every
-- %s, %d, %.1f (and the like).
local function frame_pattern(key)
  local out, captures, literal = { "^" }, 0, 0
  local i = 1
  while i <= #key do
    local c = key:sub(i, i)
    if c == "%" then
      local spec = key:match("^%%[%-%d%.]*[sdfiq%%]", i)
      if spec == "%%" then
        out[#out + 1] = "%%"
        literal = literal + 1
        i = i + 2
      elseif spec then
        local conv = spec:sub(-1)
        captures = captures + 1
        local pad = spec:find("%d", 2) and conv ~= "f" and "%s*" or "" -- (a width pads with spaces)
        local capture = (conv == "d" or conv == "i") and "(%-?%d+)" or (conv == "f") and "(%-?[%d%.]+)" or "(.-)"
        out[#out + 1] = pad .. capture
        i = i + #spec
      else
        out[#out + 1] = "%%"
        literal = literal + 1
        i = i + 1
      end
    elseif c == " " then -- any run of spaces matches any other (columns are padded)
      local j = i
      while key:sub(j + 1, j + 1) == " " do j = j + 1 end
      out[#out + 1] = "%s+"
      literal = literal + 1
      i = j + 1
    else
      out[#out + 1] = c:find("[%^%$%(%)%.%[%]%*%+%-%?]") and "%" .. c or c
      literal = literal + 1
      i = i + 1
    end
  end
  out[#out + 1] = "$"
  return captures > 0 and table.concat(out) or nil, literal
end

-- Loads the language with this code (from every package). Unknown codes and
-- "en" give English. Returns the code in use.
function i18n.set(code)
  cache, cached = {}, 0
  current = { code = i18n.DEFAULT, name = "English", plural = i18n.PLURAL.one_other, exact = {}, frames = {} }
  if not code or code == i18n.DEFAULT then return current.code end
  local dir = "locale/" .. code
  local meta = read_table(dir .. "/language.lua")
  if not meta then return current.code end
  current.code, current.name = code, meta.name or code
  current.plural = i18n.PLURAL[meta.plural or "one_other"] or i18n.PLURAL.one_other
  local files = {}
  for _, e in ipairs(vfs.list(dir) or {}) do
    if not e.dir and e.name:match("%.lua$") and e.name ~= "language.lua" then files[#files + 1] = e.name end
  end
  table.sort(files)
  for _, name in ipairs(files) do
    for key, value in pairs(read_table(dir .. "/" .. name) or {}) do
      if type(key) == "string" then current.exact[key] = value end
    end
  end
  for key, value in pairs(current.exact) do
    local pattern, literal = frame_pattern(key)
    if pattern then
      current.frames[#current.frames + 1] = { pattern = pattern, literal = literal, value = value, key = key }
    end
  end
  -- The most specific frames first (more literal text), then by key for a stable order.
  table.sort(current.frames, function(a, b)
    if a.literal ~= b.literal then return a.literal > b.literal end
    return a.key < b.key
  end)
  return current.code
end

function i18n.code() return current.code end
function i18n.name() return current.name end

-- The languages every package offers: { code, name } sorted by code, English first.
function i18n.languages()
  local out = { { code = i18n.DEFAULT, name = "English" } }
  for _, e in ipairs(vfs.list("locale") or {}) do
    if e.dir and e.name ~= i18n.DEFAULT then
      local meta = read_table("locale/" .. e.name .. "/language.lua")
      if meta then out[#out + 1] = { code = e.name, name = meta.name or e.name } end
    end
  end
  table.sort(out, function(a, b)
    if (a.code == i18n.DEFAULT) ~= (b.code == i18n.DEFAULT) then return a.code == i18n.DEFAULT end
    return a.code < b.code
  end)
  return out
end

local function pick(value, count)
  if type(value) ~= "table" then return value end
  return value[current.plural(count or 1)] or value.other or value.one
end

-- Puts the captured values into a translation: {n} by number, else %s/%d in order.
local function fill(text, values)
  if text:find("{%d+}") then
    return (text:gsub("{(%d+)}", function(n) return values[tonumber(n)] or "" end))
  end
  local k = 0
  return (text:gsub("%%[%-%d%.]*[sdfiq%%]", function(spec)
    if spec == "%%" then return "%" end
    k = k + 1
    return values[k] or ""
  end))
end

local translate

-- An entry, also when the text only differs in its first letter's case
-- ("Healing draught" finds "healing draught", whose German noun is capitalised anyway).
local function lookup(v)
  local e = current.exact[v]
  if e ~= nil then return e end
  local first = v:sub(1, 1)
  if first:match("%u") then return current.exact[first:lower() .. v:sub(2)] end
  if first:match("%l") then return current.exact[first:upper() .. v:sub(2)] end
  return nil
end

-- A captured value: a known word or phrase, a list of them, or a frame of its own.
local function value_of(v, depth)
  if v == "" or tonumber(v) then return v end
  local exact = lookup(v)
  if exact then return pick(exact) end
  local glyph, rest = v:match("^([%*~%^%+=#@%%])(%a.*)$") -- a mana colour's glyph stays before its name
  if glyph then return glyph .. value_of(rest, depth) end
  if depth < 4 then
    local t = translate(v, depth + 1)
    if t then return t end
  end
  if v:find(", ", 1, true) then
    local parts = {}
    for part in (v .. ", "):gmatch("(.-), ") do parts[#parts + 1] = value_of(part, depth) end
    return table.concat(parts, ", ")
  end
  local thing, have = v:match("^(.-) (%d+/%d+)$") -- "resin 0/1": a thing, had of needed
  if thing and lookup(thing) then return pick(lookup(thing)) .. " " .. have end
  local name, amount = v:match("^(.-) (%-?[%d%.]+)$") -- "boiled water 0.5": a thing and how much
  if name and lookup(name) then return pick(lookup(name), tonumber(amount)) .. " " .. amount end
  if collecting and v:find("%a") then collecting["(value) " .. v] = true end
  return v
end

-- The translation of one piece of text, or nil when no entry fits.
translate = function(text, depth)
  local exact = lookup(text)
  if exact then return pick(exact) end
  for _, f in ipairs(current.frames) do
    local caps = { text:match(f.pattern) }
    if caps[1] then
      local count
      for i, v in ipairs(caps) do
        if not count and tonumber(v) then count = tonumber(v) end
        caps[i] = value_of(v, depth)
      end
      return fill(pick(f.value, count), caps)
    end
  end
  return nil
end

-- The text in the current language (as it is when nothing fits).
function i18n.t(text)
  if current.code == i18n.DEFAULT or type(text) ~= "string" or text == "" or tonumber(text) then return text end
  local hit = cache[text]
  if hit then return hit end
  local out = translate(text, 0)
  if not out then
    out = text
    if collecting then collecting[text] = true end
  end
  if cached >= i18n.CACHE then cache, cached = {}, 0 end
  cache[text], cached = out, cached + 1
  return out
end

-- Formats in the current language: i18n.f("Bag %d/%d folds: %s", 3, 40, "herbs").
function i18n.f(fmt, ...)
  if current.code == i18n.DEFAULT then return string.format(fmt, ...) end
  local entry = current.exact[fmt]
  if not entry then return i18n.t(string.format(fmt, ...)) end
  local values, count = {}, nil
  for i = 1, select("#", ...) do
    local v = select(i, ...)
    if not count and type(v) == "number" then count = v end
    values[i] = type(v) == "string" and value_of(v, 0) or tostring(v)
  end
  return fill(pick(entry, count), values)
end

-- Remembers text that had no translation (for translators and the tests);
-- i18n.collect(false) stops and returns the sorted list.
function i18n.collect(on)
  if on then
    collecting = {}
    cache, cached = {}, 0
    return nil
  end
  local out = {}
  for text in pairs(collecting or {}) do out[#out + 1] = text end
  table.sort(out)
  collecting = nil
  return out
end

return i18n
