# codex Specification

## Purpose
The mage's natural history of the living world: entries that grow from Seen to Understood by looking and sampling, pages that reveal more by level, warnings of dangers, and the yield of the right method.

## Requirements

### Requirement: Entries and levels
The Codex SHALL hold an entry for each plant, fungus, animal and insect species of the data; an entry SHALL grow Seen (in sight and range), Observed (looked at 3 s, animals 8 s), Studied (a sample taken; insects watched 30 s) and Understood (three samples; insects watched 90 s); each level from Observed SHALL be told on the HUD.

#### Scenario: Entries and levels
- **WHEN** things come into sight, are too far or behind, are looked at for a while, and samples are taken
- **THEN** what is in sight is seen quietly, what is too far or behind is not, the thing nearest the middle of the view is looked at and observed after its time, the HUD tells it and shows the progress, samples study and three understand, and long-watched insects are studied and understood

### Requirement: The Codex warns
From Observed an entry's page and news SHALL warn of its dangers (poison, attack, bites), and the Codex SHALL count an item's dangers as known once an entry it comes from is observed; each level SHALL reveal more of the page: what it is, where and when it lives, what it gives, and what it is good for.

#### Scenario: The Codex warns
- **WHEN** the fly agaric is seen and then observed, and a plant, an animal and an insect are known at their levels
- **THEN** the poison is unknown when seen and warned of once observed, and each page shows what its level reveals and no more

### Requirement: Reading the Codex and keeping it
The Codex SHALL open at the entry last learned, turn to the next and close after the last; it SHALL be kept with the player, and a broken or old save SHALL keep what it can.

#### Scenario: Reading the Codex and keeping it
- **WHEN** the empty Codex is opened, entries are learned and browsed, and the Codex is saved and read back, also from broken data
- **THEN** the empty Codex says so, the last learned comes first and the Codex closes after the last, and what was known comes back

### Requirement: The Codex in the game
The Codex SHALL look from the mage's eyes on foot (along the camera in flight); gathering SHALL sample what it takes; once understood, what is taken from an entry SHALL yield a quarter more; eating SHALL avoid a poison only once the Codex has warned of it; J SHALL open the Codex.

#### Scenario: The Codex in the game
- **WHEN** the mage on foot looks at a tree and follows a deer with the eyes, butchers four hares, is offered a fly agaric before and after observing it, and presses J
- **THEN** the tree and, after longer, the deer are observed and the HUD says so, the hares are studied and then understood and the fourth gives more meat, the unknown mushroom is tasted and the known one avoided, and the Codex opens at the entry last learned
