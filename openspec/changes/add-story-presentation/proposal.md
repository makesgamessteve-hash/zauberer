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

- **Speech:** what people say is shown in a framed box low on the screen with the speaker's name
  plate.
  - Long speech goes on with confirm.
  - Choices are numbered rows with their chips, chosen with the number keys, up/down and confirm,
    the mouse, or a tap.
  - The world above stays visible. Alone, the heir's actions wait while a box is open; in company
    nothing waits.
- **Narration:** what the world does (the hatch closing, fire over the roofs) is shown as a framed
  narration line under the compass, one at a time, each for its reading time.
- **Chapter cards:** a turn of the story shows a card across the middle: the act, its name, the day
  and the hour. Its band draws out from the middle, it holds three seconds, and it fades. Confirm
  skips it.
- **The objective card** (from `redesign-hud`) announces a new goal with "New goal" for a moment.
- **Place cards:** entering a settlement, ruin or site shows its name and kind once, in a small card
  under the compass ("Kesh, a market town").
- Story beats come from the director's beats as before. Only their presentation changes, and the
  journal keeps every line spoken and narrated.

## Platforms

- **Touch:** choices are buttons.
- **Gamepad:** the d-pad and A choose.
- **WebXR:** the box and cards are drawn on the HUD panel.

## Co-op

- Speech to one heir shows on their screen.
- Narration and chapter cards show to all.
- A choice that binds everyone (the moment of choice at the end) is made by the host, and the
  others see "Waiting for <name>".

## Impact

New capability `story-presentation`. `story/prologue.lua`, `story/saga.lua` and `people/npcs.lua`
give speech, narration and beats as events instead of lines; `ui/story.lua` draws them;
`ui/journal.lua` keeps them.
