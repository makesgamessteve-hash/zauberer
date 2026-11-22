# game-menu Specification

## Purpose
TBD - created by archiving change redesign-menus-as-book. Update Purpose after archive.

## Requirements

### Requirement: The book's pages
The menu SHALL be drawn as the father's book over the dimmed world: two parchment pages in a kilim-bound cover with the tabs Menu, Journal, Map, Help and Settings above, turned with Q and E or the shoulder buttons or by pointing at a tab; the page's lines SHALL be on the left page with the focused one on a focus bar, what the chosen row is about on the right page, and the bindings for the page under the book; the map SHALL be painted across both pages.

#### Scenario: Turning the tabs
- **WHEN** the book is opened, E is pressed five times, Q once, and the Map tab is clicked
- **THEN** the tabs turn in order and wrap to Menu, Q turns back, the clicked tab opens the Map page, the map is painted into a texture, and a page turn is heard

### Requirement: Pointing and focus
Every row and tab of the book SHALL be hoverable and clickable with the mouse and tappable, as well as reachable with keys and pads; the focused row SHALL always be visible.

#### Scenario: Mouse, keys and touch
- **WHEN** a row of the Settings page is hovered, the next one is reached with the down arrow, and a third is clicked
- **THEN** each in turn is focused, and the click chooses it
