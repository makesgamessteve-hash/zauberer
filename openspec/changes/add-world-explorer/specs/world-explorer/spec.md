## ADDED Requirements

### Requirement: Opening the explorer
The world explorer SHALL open with the explorer action (F4) in flight or from the book's Menu page and close with the explorer action or Escape; while it is open the heir's actions SHALL be idle and, alone, the world SHALL stand still.

#### Scenario: Opening and closing
- **WHEN** F4 is pressed in flight, then Escape, then the Menu page's explorer row is chosen
- **THEN** the explorer opens with the world held, closes, and opens again from the menu

### Requirement: The globe
The explorer SHALL show the played planet as a globe that turns by dragging and by the spin keys and zooms from the whole planet to a region, marking every settlement, the sites and the carpet, coloured by realms, cultures or faiths as chosen, and opening a settlement clicked on.

#### Scenario: Turning, zooming and marks
- **WHEN** the globe is dragged, spun with the keys, zoomed in with the wheel, its overlay turned to realms, and a settlement on it is clicked
- **THEN** it turns, comes nearer within its limits, the settlements and the carpet are marked, the land is coloured by realm, and the clicked settlement's page opens

### Requirement: The lore explorer
The explorer SHALL list every kind of the history database with its count, list each kind's entities sorted by name (events by year), and show an entity's fields with its ids as links, what refers to it, its events and the lore written from it; the globe SHALL swing to an entity with a place.

#### Scenario: Following the lore
- **WHEN** the tester opens the realms, a realm, its capital through the link, and goes back
- **THEN** the realm's page shows its book of kings, its settlements and events, the capital's page opens with the globe turned to it, and going back returns to the realm
