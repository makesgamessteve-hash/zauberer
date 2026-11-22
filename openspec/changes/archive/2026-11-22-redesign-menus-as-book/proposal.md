# Proposal: The menu as the father's book

Milestone: UI redesign. Fifth of six changes; needs `add-ui-drawing-layer` and `add-ui-art-kit`.
Mockups: docs/design/ui/05-book-bag.png, 06-book-map.png, 07-title.png.

## Why

The menus are centred lists of words on the dimmed world:
- the pages are a chain of sub-pages (main, journal, map, help, settings, controls, packages,
  credits, company, save and load);
- the map is drawn with letters (`~ . ^ A`);
- the bag and the spellbook have no pages at all;
- the mouse cannot point at anything.

The concept asks for diegetic UI: the spellbook, the map table. And the heir's story begins with
the father's books.

## What Changes

- **The book.** Escape (Start, the touch Menu button) opens the father's book over the dimmed world:
  two parchment pages in a kilim-bound cover, with tabs above: **Menu** (resume, save and load,
  company, controls, packages, credits, quit), **Journal**, **Map**, **Help** and **Settings**.
  Q/E or the shoulder buttons turn the tabs; the page's lines are on the left page with the
  focused one on a lapis focus bar, what the chosen row is about on the right page (what a setting
  does, the current goal), the bindings for the page under the book.
- **The painted map.** The map is painted into a texture from the land the heir has seen: sea,
  shore, lowland, highland and peaks in the parchment's inks, unseen land left as paper, with the
  places by name, the heir, the carpet, the goal, a north mark and a scale.
- **The title** stands over the waiting world: the name in the carpet's colours over a woven band,
  the choices in a framed panel, the save Continue would load (its name and day) beside them.
- **Pointing everywhere.** Every row and tab can be hovered and clicked with the mouse and tapped,
  and keeps working with keys and pads.
- **Turning pages** has a sound.
- Left for later changes: the Bag page with item slots and actions, the Weaves page, zooming the
  map with the wheel or a pinch.

## Platforms

- **Touch:** rows and tabs are tapped.
- **WebXR:** the book is drawn on the HUD panel, with pages turned by the controller's stick.

## Co-op

- The book never pauses the shared world.
- The Map page shows the other heirs.
- Company (remove a heir, close the session, quick messages) is in the cover's corner menu.

## Impact

New capability `game-menu`. Modified requirements: `game-flow` Title screen, `accessibility` The
menu, `world-map` The map. `ui/book.lua` draws the menu's pages (ui/menu.lua keeps what they do and
gains the tabs and pointing), `ui/worldmap.lua` paints the map, `data/input.lua` the tab actions.
