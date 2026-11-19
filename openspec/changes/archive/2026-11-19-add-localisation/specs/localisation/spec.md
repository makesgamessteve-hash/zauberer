## ADDED Requirements

### Requirement: English needs no table
English SHALL be the language without a table: text is shown as it is, and an unknown language SHALL fall back to English.

#### Scenario: English needs no table
- **WHEN** English or an unknown language is chosen
- **THEN** lines and formats are shown as written

### Requirement: Languages from packages
The languages SHALL be those that packages offer as locale/<code>/language.lua (a name and a plural rule), English first; the string tables of every package SHALL be merged.

#### Scenario: Languages from packages
- **WHEN** the languages are listed
- **THEN** English comes first and German (Deutsch) is offered by the base game

### Requirement: Text in German
A line SHALL be translated whole or by the most specific format frame that matches it; captured values (item and species names, lists, things with amounts, colour glyphs) SHALL be translated in turn; translations MAY reorder values ({n}) and choose plural forms by the first number.

#### Scenario: Text in German
- **WHEN** German lines with frames, lists, amounts, reordered words, plurals and padded columns are translated
- **THEN** each comes out as the German table says

### Requirement: Untranslated text is found
Text without an entry SHALL be shown as it is; on request the game SHALL collect it (whole lines, and captured values) for translators.

#### Scenario: Untranslated text is found
- **WHEN** a generated name and a known line are shown while collecting
- **THEN** only the name is collected, once

### Requirement: The system's language at the first start
At the first start the language SHALL be the system's preferred one when a package offers it, else English; a language the player chose SHALL win from then on.

#### Scenario: The system's language at the first start
- **WHEN** the settings are loaded first with German, then an unknown language, then with English chosen
- **THEN** German, English, and the choice

### Requirement: The player's language from the system
engine.info().language SHALL give the player's preferred language: the system's first preferred locale, or ZB_LANGUAGE when set (headless: ZB_LANGUAGE, else en).

#### Scenario: The player's language from the system
- **WHEN** the game starts headless with ZB_LANGUAGE=de and no settings
- **THEN** engine.info().language is de and the game is in German

### Requirement: Extended Latin letters
The pixel font SHALL have the letters of German and of the other left-to-right Latin languages (accents, cedilla, ogonek, caron, Turkish, Nordic and Polish letters) and typographic quotes, dashes and signs; letters it lacks SHALL show the box.

#### Scenario: Extended Latin letters
- **WHEN** a line of such letters is laid out
- **THEN** every letter has its own glyph and a letter outside the set is the box

### Requirement: The game in German
In German everything the HUD and the menu show SHALL have an entry; only generated names and numbers stay as they are.

#### Scenario: The game in German
- **WHEN** the game is played and every menu page shown in German while collecting
- **THEN** nothing but names and numbers lacks German
