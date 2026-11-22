# Proposal: Story moments presented as such

Milestone: UI redesign. Fourth of six changes; needs `add-ui-drawing-layer`, `add-ui-art-kit` and
`redesign-hud`. Mockups: docs/design/ui/03-dialogue.png, 04-chapter-card.png.

## Why

The story is told in HUD lines:
- "Story: Stay hidden";
- "Farrokh presses a plain rug ... into your arms";
- "Azhdar burns carpet after carpet";
- "Story: duel".

They arrive in the same list as the pack's contents and scroll away. People's words have no speaker,
choices look like any other line, and the turn into a new act is one more line. The story's best
moments read like a log.

## What Changes

- **Speech:** what people say ("Farrokh: ...") is shown in a framed box low on the screen with the
  speaker's name plate instead of a toast. Lines said one after another wait their turn, each for
  its reading time; confirm goes on. The world and the heir go on meanwhile (speech never stops
  play). Choices in talk are a feature of their own, left to a later change.
- **Narration:** what the world does in the story (the hatch closing, fire over the roofs) is shown
  as a framed band under the compass, one line at a time, each for its reading time.
- **Chapter cards:** a turn of the story shows a card across the middle: the act, its name, the day
  and the hour. Its band draws out from the middle, it holds three seconds, and it fades. Confirm
  skips it.
- **The objective card** (from `redesign-hud`) announces a new goal with "New goal" for a moment.
- **Place cards:** coming to a settlement, ruin or site shows its name and kind once, in a small card
  under the compass ("Anarvan, village").
- The journal keeps every line spoken and narrated, as before.

## Platforms

- **Touch:** a tap on the speech box goes on.
- **Gamepad:** A goes on.
- **WebXR:** the box and cards are drawn on the HUD panel.

## Co-op

Speech and narration show on the screen of the heir who hears them; chapter cards show to all.

## Impact

New capability `story-presentation`. `ui/story.lua` (speech, narration, chapter and place cards);
`main.lua` routes lines of speech and story narration to it, turns the story's beats into cards and
places come to into place cards.
