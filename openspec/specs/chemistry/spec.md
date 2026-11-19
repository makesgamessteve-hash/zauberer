# chemistry Specification

## Purpose
The chemistry beneath alchemy: substances and reactions as data, mixtures in vessels, the processes of the apparatus, and knowing substances by analysis.

## Requirements

### Requirement: Substances and reactions are data
Substances SHALL be data with a phase, boiling point, solvent and dissolving, and the item they are bottled as; reactions SHALL be data with reactants and products in proportions, a temperature range with a best temperature, a rate and optionally a solvent, catalyst or mana; both SHALL be checked on load.

#### Scenario: Substances and reactions are data
- **WHEN** the substances and reactions are read, and broken ones are checked (no phase, unknown substances, no proportions, a bad temperature range, an unknown catalyst, a bottle of no item)
- **THEN** water is a solvent and the healing draught is bottled, and each broken one is refused with what is wrong

### Requirement: Processes in a vessel
Goods SHALL go into a vessel as their substances by weight; heating SHALL run the reactions in range (faster at their best and ground), needing their solvent and mana, and boil off liquids above their boiling point; filtering SHALL take out undissolved solids; bottling SHALL make items of full bottles.

#### Scenario: Processes in a vessel
- **WHEN** herbs are put in a vessel dry, then in water, ground or not, too cold, filtered, heated with and without life mana, boiled, and bottled
- **THEN** dry herbs do not steep, ground herbs steep faster, cold nothing happens, the spent matter is filtered out, the draught comes only with life mana and uses it, the water boils off, and the draught is bottled in vials with what makes no full bottle left

### Requirement: Fermenting, distilling and knowing
Fermentation SHALL need its catalyst; distilling SHALL catch what boils below a temperature in a new vessel; substances SHALL be unknown until analysed, then known by name.

#### Scenario: Fermenting, distilling and knowing
- **WHEN** dates ferment with and without yeast, the spirit is distilled, quintessence of light is made from it with light mana, and the vessels are analysed
- **THEN** only with yeast is there spirit, distilled it is caught apart from the water, the quintessence comes, and analysed the substances are known by name, once
