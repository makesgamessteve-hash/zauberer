# lore Specification

## Purpose
Lore from the history database: wars told from a side, books of kings, legends, inscriptions in invented scripts, what people know, the lore journal.

## Requirements

### Requirement: Told from a side
A war SHALL be told as a glorious victory by the winners' people, as treachery by the losers' and plainly by others.

#### Scenario: Told from a side
- **WHEN** one war is told by its winners, its losers and a stranger
- **THEN** the titles and lines differ as their sides, and the stranger names the winner plainly

### Requirement: Books of kings
A realm's book of kings SHALL list its dynasties and rulers with their epithets; told by a people it fought, its good kings SHALL be blackened.

#### Scenario: Books of kings
- **WHEN** a realm's book is told by its own people and by its enemy
- **THEN** its own keep their names and epithets, the enemy keeps no good king

### Requirement: Legends drift
Legends SHALL drift with age and distance: dates vague, numbers grown, gods fighting, eruptions the wrath of the teller's god, prophets in pillars of light.

#### Scenario: Legends drift
- **WHEN** events are told near and far, young and old
- **THEN** the near ones are true to the record and the far ones grown into myth

### Requirement: Inscriptions in a people's script
Inscriptions at tombs, battlefields, elder ruins and holy places SHALL be words in the place's language (the Old Tongue at elder ruins) with a gloss, drawn with the script of its family, one of seven generated fonts.

#### Scenario: Inscriptions in a people's script
- **WHEN** every kind of site is inscribed
- **THEN** each has words, a gloss and a script whose font exists

### Requirement: What people know
People SHALL know the wars of their realm told their way, their faith's ways, grudges where they lost, songs of their heroes and recent rumours.

#### Scenario: What people know
- **WHEN** the knowledge of a loser and a winner is asked
- **THEN** both know the history, the loser holds a grudge

### Requirement: The lore journal
What is learnt from books, talk and visits SHALL go into the journal once, counted by Codex section, with pages for known entries only, saved with the game.

#### Scenario: The lore journal
- **WHEN** entries are learnt twice, counted, paged and saved
- **THEN** each is learnt once into its section, unknown ones have no page, and the journal comes back
