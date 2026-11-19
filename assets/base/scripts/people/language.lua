-- SPDX-License-Identifier: MIT
-- Languages (concept 10.8): a phonology from the style family (onsets,
-- vowels, codas, syllable patterns), suffixes for names of settlements,
-- rivers, mountains, peoples, holy places and realms, and a lexicon of roots
-- made on demand from the language's seed. Daughter languages copy their
-- parent and apply sound changes, so related languages share recognisable
-- roots; a few roots of each daughter are new. Every word is the same on
-- every platform for the same seed.
local rng = require("people.rng")

local language = {}

language.MEANINGS = {
  "water", "river", "mountain", "stone", "sun", "moon", "star", "fire", "ash", "gold", "silver", "red", "white",
  "black", "blue", "green", "field", "garden", "gate", "tower", "king", "queen", "lord", "mother", "father",
  "son", "daughter", "sword", "banner", "seven", "three", "weep", "bridge", "holy", "light", "dark", "wind",
  "sand", "salt", "palm", "rose", "lion", "eagle", "horse", "wolf", "snake", "falcon", "house", "city", "new",
  "old", "high", "great", "small", "spring", "well", "valley", "lake", "sea", "forest", "war", "peace",
  "victory", "death", "song", "dream", "weave", "carpet", "thread", "god", "people", "land", "road", "faith",
  "wise", "brave", "first", "last", "iron", "copper", "salt", "silk", "bread", "wine", "rain", "storm", "ice",
  "dawn", "dusk", "blood", "bone", "heart", "eye", "hand", "word", "book", "teacher", "pilgrim", "shepherd",
}

-- Sound changes a daughter language may apply to its parent's words.
local CHANGES = {
  { "k", "ch" }, { "p", "f" }, { "s", "h" }, { "t", "d" }, { "g", "j" }, { "b", "v" }, { "d", "z" },
  { "kh", "h" }, { "sh", "s" }, { "q", "k" }, { "a", "e" }, { "o", "u" }, { "u", "o" }, { "i", "e" },
  { "aa", "o" }, { "r", "l" }, { "n", "m" },
}

local function capital(s)
  return (s:gsub("^%l", string.upper))
end

local DIGRAPHS = { kh = true, sh = true, ch = true, dh = true, bh = true, gh = true, zh = true, ts = true, dz = true,
  ng = true, tt = true }
local VOWEL = { a = true, e = true, i = true, o = true, u = true, y = true }

-- Keeps a word pronounceable: at most two consonant sounds in a row, no
-- letter three times running.
function language.tidy(word)
  local tokens, i = {}, 1
  while i <= #word do
    local two = word:sub(i, i + 1)
    if DIGRAPHS[two] then tokens[#tokens + 1], i = two, i + 2 else tokens[#tokens + 1], i = word:sub(i, i), i + 1 end
  end
  local out, run = {}, 0
  for k, t in ipairs(tokens) do
    if VOWEL[t:sub(1, 1)] then
      run = 0
      out[#out + 1] = t
    else
      run = run + 1
      local prev = tokens[k - 1]
      if run <= 2 and not (prev and (t == prev or t == prev:sub(-1))) then out[#out + 1] = t end
    end
  end
  local w, n = table.concat(out), 1
  while n > 0 do w, n = w:gsub("(%a)%1%1", "%1%1") end
  return w
end

-- A language from its record (what the history database keeps): { seed,
-- family (style family id), changes (sound changes from the parent), parent
-- (the parent language object, or nil), replace (share of new roots) }.
function language.make(rec, family, parent)
  local ph = family.phonology
  local l = { seed = rec.seed, family = family.id, script = family.script, onsets = ph.onsets, vowels = ph.vowels,
    codas = ph.codas, patterns = ph.patterns, suffixes = ph.suffixes, changes = rec.changes or {}, parent = parent,
    coined = {}, replace = rec.replace or 0.15 }
  if parent then
    l.suffixes = {}
    local kinds = {}
    for kind in pairs(parent.suffixes) do kinds[#kinds + 1] = kind end
    table.sort(kinds)
    for _, kind in ipairs(kinds) do
      local list = {}
      for i, suf in ipairs(parent.suffixes[kind]) do
        for _, ch in ipairs(l.changes) do suf = suf:gsub(ch[1], ch[2]) end
        list[i] = suf
      end
      l.suffixes[kind] = list
    end
  end
  return l
end

-- A new language for a style family; key: a stable number or string.
function language.new(seed, key, family)
  return language.make({ seed = rng.hash(seed, "language", key) }, family)
end

-- A word made of syllables from the language's sounds, from a hash.
function language.word(lang, h, syllables)
  local s = rng.new(h)
  local out = {}
  local pattern = s:pick(lang.patterns)
  if syllables then
    pattern = string.rep("CV", syllables)
  end
  for c in pattern:gmatch(".") do
    if c == "C" then out[#out + 1] = s:pick(lang.onsets) else out[#out + 1] = s:pick(lang.vowels) end
  end
  out[#out + 1] = s:pick(lang.codas)
  return table.concat(out)
end

local function apply(changes, word)
  for _, ch in ipairs(changes) do
    word = word:gsub(ch[1], ch[2])
  end
  return word
end

-- The root word for a meaning (stable; inherited roots carry the sound changes).
function language.root(lang, meaning)
  local cached = lang.coined[meaning]
  if cached then return cached end
  local w
  if lang.parent and rng.float(lang.seed, "kept", meaning) >= (lang.replace or 0.15) then
    w = language.tidy(apply(lang.changes, language.root(lang.parent, meaning)))
  else
    w = language.word(lang, rng.hash(lang.seed, "root", meaning))
  end
  lang.coined[meaning] = w
  return w
end

-- The record of a daughter language: the parent's sounds and words through
-- a few sound changes.
function language.daughter(seed, key, steps)
  local rec = { seed = rng.hash(seed, "language", key), changes = {}, replace = 0.15 }
  local s = rng.new(rec.seed, "changes")
  for _ = 1, steps or 2 do
    local ch = s:pick(CHANGES)
    rec.changes[#rec.changes + 1] = { ch[1], ch[2] }
  end
  return rec
end

-- A daughter language of a parent language object.
function language.derive(parent, seed, key, steps)
  local rec = language.daughter(seed, key, steps)
  local family = { id = parent.family, phonology = { onsets = parent.onsets, vowels = parent.vowels,
    codas = parent.codas, patterns = parent.patterns, suffixes = parent.suffixes } }
  local l = language.make(rec, family, parent)
  l.script = parent.script
  return l
end

-- The share of the core meanings whose roots two languages share in form or
-- by a regular change (0 unrelated .. 1 the same).
function language.kinship(a, b)
  local function ancestors(l)
    local set, n = {}, 0
    while l do set[l] = n; l, n = l.parent, n + 1 end
    return set
  end
  local up = ancestors(a)
  local l, n = b, 0
  while l do
    if up[l] then
      local depth = up[l] + n
      return depth == 0 and 1 or (0.85 ^ depth)
    end
    l, n = l.parent, n + 1
  end
  return 0
end

-- A name: kind "settlement", "river", "mountain", "people", "holy", "realm"
-- or "person"; key makes different names of the same kind; meaning (optional)
-- builds it on that root.
function language.name(lang, kind, key, meaning)
  local h = rng.hash(lang.seed, "name", kind, key)
  if kind == "person" then
    return capital(language.tidy(language.word(lang, h, 2 + (h & 1))))
  end
  local stem = meaning and language.root(lang, meaning) or language.word(lang, h)
  local list = lang.suffixes[kind]
  local suffix = list and list[1 + (h >> 8) % #list] or ""
  if #stem > 7 then stem = stem:sub(1, 7) end
  return capital(language.tidy(stem .. suffix))
end

-- Words of a phrase in the language, one root per meaning (unknown meanings are
-- coined on the spot), and the gloss.
function language.phrase(lang, meanings)
  local words = {}
  for i, m in ipairs(meanings) do words[i] = language.root(lang, m) end
  return table.concat(words, " "), table.concat(meanings, " ")
end

return language
