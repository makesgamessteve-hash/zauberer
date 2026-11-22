## ADDED Requirements

### Requirement: The book's pages
The book SHALL have the tabs Journal, Map, Bag, Weaves, Codex and Settings, turned with Q and E or the shoulder buttons or by pointing at a tab; each page SHALL show the bindings it uses under the book; the cover's corner menu SHALL hold Save, Load, Company in a session, Help, Credits and Quit to title.

#### Scenario: Turning the tabs
- **WHEN** the book is opened and E is pressed six times, a tab is clicked, and the corner menu is opened
- **THEN** the tabs turn in order and wrap to Journal, the clicked tab opens, each page shows its bindings, and the corner menu lists its entries

### Requirement: The bag as slots
The Bag page SHALL show the pack and the endless bag as slots with each item's icon and count and the pack's weight against its limit; choosing an item SHALL open its page with what it is and what can be done with it (eat, drink, cook, drop, move between pack and bag).

#### Scenario: Eating a pomegranate from the book
- **WHEN** the Bag page is opened with three pomegranates, the first slot is chosen and Eat is chosen
- **THEN** the slot shows the icon and 3, the item's page describes it, eating leaves 2 and eases hunger, and the weight shown drops

### Requirement: Weaves in the book
The Weaves page SHALL list the weaves known with their forms and colours, and SHALL put a chosen weave into the left or right hand.

#### Scenario: Preparing Freeze
- **WHEN** Freeze is known, chosen on the Weaves page and put into the right hand
- **THEN** the right hand holds Freeze in the HUD

### Requirement: Pointing and focus
Every row, tab, slot and button SHALL be hoverable and clickable with the mouse and tappable, as well as reachable with keys and pads; the focused one SHALL always be visible.

#### Scenario: Mouse, keys and touch
- **WHEN** a slot is hovered and clicked with the mouse, the next one is reached with the right arrow, and a third is tapped
- **THEN** each in turn is focused and visibly marked, and the click and tap choose
