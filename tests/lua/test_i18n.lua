-- SPDX-License-Identifier: MIT
-- Unit tests of languages (assets/base/scripts/ui/i18n.lua) with the base
-- game's German string table.
local i18n = require("ui.i18n")

local function german(fn)
  i18n.set("de")
  local ok, err = pcall(fn)
  i18n.set("en")
  if not ok then error(err, 0) end
end

-- spec: localisation: English needs no table
test.case("English text is shown as it is", function()
  test.expect_eq(i18n.set("en"), "en")
  test.expect_eq(i18n.t("Health 5/10"), "Health 5/10")
  test.expect_eq(i18n.f("Bag %d/%d folds", 1, 12), "Bag 1/12 folds")
  test.expect_eq(i18n.set("xx"), "en", "an unknown language is English")
  test.expect_eq(i18n.name(), "English")
end)

-- spec: localisation: Languages from packages
test.case("the languages packages offer", function()
  local codes = {}
  for _, l in ipairs(i18n.languages()) do codes[#codes + 1] = l.code .. "=" .. l.name end
  test.expect_eq(codes[1], "en=English", "English first")
  test.expect(table.concat(codes, " "):find("de=Deutsch", 1, true), table.concat(codes, " "))
end)

-- spec: localisation: Text in German
test.case("whole lines, frames and the values they capture", function()
  german(function()
    test.expect_eq(i18n.code(), "de")
    test.expect_eq(i18n.t("Resume"), "Weiterspielen")
    test.expect_eq(i18n.t("Health 5/10"), "Gesundheit 5/10")
    -- A list of things with amounts, one of them spoilt.
    test.expect_eq(i18n.t("Pack 3.2/20 kg: boiled water 0.5, cooked meat 1 (spoilt)"),
      "Gepäck 3.2/20 kg: Abgekochtes Wasser 0.5, Gekochtes Fleisch 1 (verdorben)")
    -- Words in another order ({1}, {2}).
    test.expect_eq(i18n.t("They say the old herbalist lives near Vushananagar, 2 days' ride to the east."),
      "Man sagt, die alte Kräuterfrau lebe nahe Vushananagar, 2 Tagesritte nach Osten.")
    -- Plural forms by the first number.
    test.expect_eq(i18n.t("1 days' ride to the west"), "1 Tagesritt nach Westen")
    -- Padded columns, mana colours with their glyphs, a capitalised name.
    test.expect_eq(i18n.t("speed  45 km/h  heading 090 E"), "Tempo 45 km/h  Kurs 090 O")
    test.expect_eq(i18n.t("Mana 12/100: *fire 10, =water 2"), "Mana 12/100: *Feuer 10, =Wasser 2")
    test.expect_eq(i18n.t("Codex: Fern observed"), "Kodex: Farn beobachtet")
    -- Formatting straight into German.
    test.expect_eq(i18n.f("Bag %d/%d folds: %s", 3, 40, "healing herbs 2"), "Beutel 3/40 Falten: Heilkräuter 2")
    test.expect_eq(i18n.f("Not a key %d", 7), "Not a key 7", "unknown formats are formatted in English")
  end)
end)

-- spec: localisation: Untranslated text is found
test.case("text without an entry is shown as it is and collected", function()
  german(function()
    i18n.collect(true)
    test.expect_eq(i18n.t("Zuhrabad"), "Zuhrabad", "a generated name stays")
    test.expect_eq(i18n.t("Health 1/2"), "Gesundheit 1/2")
    test.expect_eq(i18n.t("Zuhrabad"), "Zuhrabad", "(cached)")
    local missed = i18n.collect(false)
    test.expect_eq(#missed, 1)
    test.expect_eq(missed[1], "Zuhrabad")
  end)
  test.expect_eq(#i18n.collect(false), 0, "nothing is collected when not asked")
end)

test.case("plural rules", function()
  test.expect_eq(i18n.PLURAL.one_other(1), "one")
  test.expect_eq(i18n.PLURAL.one_other(2), "other")
  test.expect_eq(i18n.PLURAL.one_few_many(1), "one")
  test.expect_eq(i18n.PLURAL.one_few_many(3), "few")
  test.expect_eq(i18n.PLURAL.one_few_many(12), "many")
  test.expect_eq(i18n.PLURAL.other(1), "other")
end)
