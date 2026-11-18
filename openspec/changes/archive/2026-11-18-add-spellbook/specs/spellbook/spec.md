## ADDED Requirements

### Requirement: The journal
The spellbook SHALL write a weave into its journal the first time it is learned or cast, with its name, how it was learned and its casts, report the discovery and each new familiarity tier as news, and list the journal in the order of discovery.

#### Scenario: Discovery and familiarity
- **WHEN** a fireball is cast for the first time and nine more times, and a weave is learned from a book
- **THEN** the first cast discovers it with news, later casts do not, the tenth makes it Practised with news, and the journal lists both with their tiers

### Requirement: Prepared weaves and slots
Only weaves in the journal SHALL be prepared, each once, at most 32; prepared weaves SHALL be put in the left and right hand slots and eight quick-wheel slots, and other slots refused.

#### Scenario: Prepared weaves and hand slots
- **WHEN** a weave not in the journal is prepared, known weaves are prepared twice and put in slots, wrong slots and missing weaves are assigned, and more weaves than fit are prepared
- **THEN** the unknown one is refused, each is prepared once, the slots hold what was assigned, wrong assignments are refused, and the book holds at most 32

### Requirement: Kept with the game
The journal, the prepared weaves, the slots and the mage's mana, attunement and strain SHALL be saved and restored, leaving out entries that are no longer valid.

#### Scenario: The spellbook is saved
- **WHEN** a spellbook with casts, prepared weaves and slots is saved and restored, nothing is restored, and an invalid entry is restored
- **THEN** the casts, slots and entries come back without news, nothing gives an empty book, and the invalid entry is left out

### Requirement: Casting in play
In flight, holding a hand button SHALL charge the hand's weave at the channeling rate and letting go SHALL cast it along the carpet's heading, with control from a gesture drawn while charging or from gesture assist; the quick wheel SHALL put its weaves in the right hand; the HUD SHALL show the mana, the hands, the free weave, the charge, strain and news.

#### Scenario: Casting from the hands
- **WHEN** a new game starts and the right hand is held and let go, a gesture is drawn while charging, the quick wheel is used, and a hand is cast with no mana
- **THEN** the hands hold Fireball and Kindle, the charge shows and Kindle is cast and paid in Fire, the charged weave casts with the drawn gesture, the wheel puts Kindle in the right hand, and the refusal shows on the HUD

### Requirement: Free weaving in play
Words SHALL pick the free weave's verb and colours and a drawn gesture its form; the next left-hand cast within eight seconds SHALL be the free weave, and a free weave cast for the first time SHALL be discovered, prepared and put in the right hand; a drawing that is no form SHALL be reported.

#### Scenario: Free weaving and discovery
- **WHEN** the words for unmake fire are picked, a second colour is picked and dropped again, a circle is drawn with the mouse and the left hand cast, and a stroke that is no form is drawn
- **THEN** the HUD shows the free weave with its form, Unmake Fire is discovered with news and goes to the right hand, the free weave is spent, and the stroke is reported as no form
