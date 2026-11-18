## ADDED Requirements

### Requirement: The grammar
A weave SHALL be a verb (create, unmake, move, shape, bind, sense, ward, call), one or two different colours, a form (tap, line, circle, spiral, triangle, square, zigzag, wave), any modifiers (far, wide, lasting, many, delayed, upon touch, returning) and a power of more than 0 and at most 1000 mana; every such combination SHALL be accepted and others refused with a reason; the data SHALL name known combinations and give the Old Tongue words.

#### Scenario: Words, colours, forms and modifiers
- **WHEN** valid and invalid weaves are checked, colours are related on the wheel, and weaves are keyed, named and spoken
- **THEN** valid ones pass and each invalid one is refused with its reason, opposites and neighbours are found (the wheel closes), keys do not depend on the order of colours, known combinations get their names and others a made-up one, and the words are the Old Tongue's

### Requirement: Cost
A weave SHALL cost its power times the verb's base, the form's factor, the modifiers' factors and a distance factor, times a factor from the mana of its colours where it is cast (between 0.5 in rich places and 3 in thin ones), less the efficiency (at most 0.6); creating stone without material SHALL cost four times as much and water twice; two colours SHALL share the cost six to four.

#### Scenario: The cost of a weave
- **WHEN** the same weave is priced with other forms, modifiers, distances, places, efficiencies and with or without material
- **THEN** wider forms, modifiers and distance cost more, rich places less and thin ones more, efficiency lowers the cost up to its limit, matter from nothing is dearer, and two colours share the cost

### Requirement: Control and misfires
Control SHALL come from the gesture's accuracy and the weave's familiarity tier; misfires SHALL be likelier with poor control, opposing colours, overcharge and weaves never cast before, and less likely with neighbouring colours, within 2 % and 90 %; a cast SHALL resolve deterministically from its seed, and a misfire SHALL distort the weave: going off at the hand, straying, weakening, coming late or overshooting.

#### Scenario: Control and misfires
- **WHEN** control and misfire chances are computed for good and poor gestures, familiar and new weaves, opposing and neighbouring colours and overcharges, and volatile weaves are resolved with many seeds
- **THEN** the chances move as described and stay within their bounds, the same seed gives the same plan, and every kind of misfire happens with its distortion

### Requirement: The mage's mana
The mage SHALL hold mana of all colours in one reservoir up to its capacity, pay a weave's cost per colour or none of it when short, grow attunement per colour with casts and time in rich places, and build strain when pouring mana faster than the channeling rate, which eases with time and refuses weaving at 90.

#### Scenario: The mage's mana
- **WHEN** mana is added beyond the capacity, a weave is paid with too little and with enough, and the mage overcharges, rests in a fiery place and casts
- **THEN** only what fits is stored, a short payment takes nothing, strain grows with overcharge and refuses at 90 until it eases, and attunement grows with the place and the casts

### Requirement: Casting
A cast SHALL be aimed along a ray, priced at the caster's place, paid from the reservoir and recorded in the journal, and SHALL be refused, saying why, without enough mana, for an invalid weave, or when the mage is burnt out.

#### Scenario: Casting and its refusals
- **WHEN** a mage casts with plenty of mana, with none, with a wrong verb, while burnt out, sloppily with volatile colours, and overcharged
- **THEN** the casts are paid and journalled, the refusals say why, misfires happen and are reported, and overcharging strains

### Requirement: Effects through the simulation
Weaves SHALL act only through the simulation: heat and heat sinks, water condensed, removed, frozen and thawed, ground raised and dug, stones conjured and hurled, gusts pushing bodies, lights and wards that fade, healing, withering and purifying events, blinking, the senses and two-colour pairs; bolts SHALL act where they land, delays SHALL hold weaves back, and weaves the world does not answer yet SHALL fizzle.

#### Scenario: Weaves through the simulation
- **WHEN** a mage by the carpet casts the named weaves of the base game, two-colour weaves, a bind and a delayed light
- **THEN** a fire bloom burns and Unmake Fire puts it out, fireballs, jets, thunderballs and stones fly and act where they land, water condenses, freezes, thaws and dries, a wall rises and a pit is dug, a conjured stone costs more than fire and a gust moves it, mud is made, lights and wards come and go, the senses report, Blink moves the carpet ten metres, steam and healing rain come from their pairs, the bind fizzles and the delayed light comes later
