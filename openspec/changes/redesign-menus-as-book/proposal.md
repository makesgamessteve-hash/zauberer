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

- **The book.** Escape (Start, the touch Menu button) opens the father's book over the dimmed
  world: two parchment pages in a kilim-bound cover, with tabs above.
  - **Journal:** goals, the story so far, people heard, the body's state (food, water, rest,
    warmth), the day.
  - **Map:** the painted map.
  - **Bag:** the pack and the endless bag as slots with icons, the chosen item's page with what it
    is and what can be done with it.
  - **Weaves:** the spellbook's weaves with their forms and colours; putting one in a hand.
  - **Codex:** creatures, plants and materials met.
  - **Settings:** with a line describing each setting and its effect shown live.

  Q/E or the shoulder buttons turn the tabs. The bindings for the page show under the book.
  Save, Load, Company, Help, Credits and Quit to title are in the cover's corner menu.
- **The painted map.** The map is painted into a texture from the land the heir has seen: sea,
  shore, lowland, highland and peaks in the parchment's inks, unseen land left as paper. It shows:
  - the way flown, the places with their names, the heir, the carpet, the goal and the other heirs;
  - a compass rose and a scale.

  It is moved with the stick, arrows, mouse drag or a finger, and zoomed with the triggers, the
  wheel or a pinch.
- **The title.** The title stands over the waiting world:
  - the name in the carpet's colours over a woven band;
  - the choices in a framed panel;
  - beside them the save that Continue would load (whose world, the day, where, the act, when it
    was saved).
- **Pointing everywhere.** Every row, tab, slot and button can be hovered and clicked with the mouse
  and tapped, and keeps working with keys and pads; focus is always visible (a lapis bar with a
  saffron edge).
- **Turning pages** has a sound and a short slide; reduced motion makes it a cut.

## Platforms

- **Touch:** tabs and slots are at least 32 grid pixels; pinch zooms the map.
- **WebXR:** the book is drawn on the HUD panel, with pages turned by the controller's stick.

## Co-op

- The book never pauses the shared world.
- The Map page shows the other heirs.
- Company (remove a heir, close the session, quick messages) is in the cover's corner menu.

## Impact

New capability `game-menu`.

Modified requirements:
- `game-flow`: Title screen.
- `accessibility`: The menu.
- `world-map`: The map.

Code: `ui/menu.lua` is split into `ui/book.lua` and the pages under `ui/book/`; `ui/worldmap.lua`
paints a texture; `ui/text.lua` lines remain only for wrapping.
