-- SPDX-License-Identifier: MIT
-- Story moments on screen: Farrokh's words in the speech box with his plate,
-- one line after the other (confirm goes on), not as toasts and kept in the
-- journal; the story's narration under the compass; the turn to the next part
-- of the story as a chapter card (confirm skips it) with the new goal; the
-- village's place card the first time only.
-- spec: story-presentation: Talking to Farrokh
-- spec: story-presentation: The night of the fire
-- spec: story-presentation: A market town
scenario { name = "story screens" }
local game = demo.game
local story = require("ui.story")
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
input.inject("story", 1, 1) -- (the prologue begins, in Anarvan)
step(3)
local S = game.story
expect(S ~= nil, "the prologue runs")
local ui = game.story_ui
step(5)

-- The village's card, the first time.
local shown = story.shown(ui)
expect(shown.place == "Anarvan", "the place card: " .. tostring(shown.place))

-- Farrokh speaks twice.
demo.pack.say(game.pack, "Farrokh: Fetch the water before the sun is high.")
demo.pack.say(game.pack, "Farrokh: The goats will not wait either.")
step(2)
shown = story.shown(ui)
expect_eq(shown.speaker, "Farrokh")
expect_eq(shown.words, "Fetch the water before the sun is high.")
for _, t in ipairs(game.hud_ui.toasts) do expect(not t.base:find("Farrokh"), "not a toast: " .. t.base) end
input.inject("confirm", 1, 1)
step(2)
expect_eq(story.shown(ui).words, "The goats will not wait either.", "confirm goes on")
step(math.ceil(60 * story.read_time("The goats will not wait either.") * 1.5) + 10)
expect(story.shown(ui).speaker == nil, "said and gone")
local kept = 0
for _, l in ipairs(game.journal.lines) do if l.text:find("Farrokh:", 1, true) then kept = kept + 1 end end
expect(kept >= 2, "the journal keeps both: " .. kept)

-- Narration from the story.
S.lines[#S.lines + 1] = "The hatch closes over you. The cellar is warded."
step(3)
expect_eq(story.shown(ui).narration, "The hatch closes over you. The cellar is warded.")

-- The chores done: the story turns; a card with the act, its name and the day; confirm skips it.
local beat = S.beat
S.chores = { water = true, goats = true, pomegranates = true }
S.talked = true
for _ = 1, 600 do
  step(1)
  if S.beat ~= beat then break end
end
expect(S.beat ~= beat, "the story turned")
step(2)
local card = story.shown(ui).card
expect(card and card.act and card.title and card.day, "a chapter card with the act, its name and the day")
expect(game.hud_ui.state.objective and game.hud_ui.state.objective.fresh, "the new goal, marked as new")
input.inject("confirm", 1, 1)
step(2)
expect(story.shown(ui).card == nil, "confirm skips the card")

-- Away from the village and back: no second card.
step(60 * story.PLACE_TIME + 10)
local home = world.move(game.carpet.pos, 0, 0, 0, 0)
game.walker = nil
game.carpet.pos = world.move(home, 0, 8000, 0, 40)
step(30)
game.carpet.pos = home
step(30)
expect(story.shown(ui).place == nil, "Anarvan's card does not show again")
