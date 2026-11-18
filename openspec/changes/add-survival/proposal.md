# Proposal: Survival

## Why

Concept 7.2: in the early game the mage has to eat, drink, rest and keep warm. The land
can make the mage sick through dirty water, spoilt food, raw meat and mosquitoes, and
poison through the wrong mushroom. M5's exit demo asks to survive seven days in two
biomes, and to get sick from swamp water and cure it.

## What Changes

- **Needs** (`player/survival.lua`), in game time:
  - food and water drain a day's worth at rest, faster with effort, cold, heat and
    sickness;
  - rest drains over sixteen hours awake;
  - empty food and water hurt.
- **Body temperature:**
  - the core settles towards what the air feels like: colder high up, in wind and
    when wet; warmer by a fire, in clothes and in shelter;
  - too cold hurts (frostbite), too hot hurts (heat stroke);
  - rain and wading soak, and a fire dries quickly.
- **Conditions:**
  - sickness is caught from dirty water, spoilt or raw food and mosquito bites; it
    worsens for hours, then passes over days, and remedies treat and cure it;
  - poison from what is eaten;
  - eating while sick nourishes less, and salt water makes one thirstier.
- **Sleep:** lying down on foot (the sleep action, L) sleeps up to ten hours or until
  rested. The world's time moves on, and the body asks less in its sleep.
- **Difficulty presets:** wanderer (half the drain, no disease), survivor (the
  default) and hermit (harsher), from the saved setting `difficulty`.
- **In the game:**
  - needs are worked out every tick from the weather, the wind, the ground, the walker,
    fires close by and mosquito bites;
  - their harms go through the damage model;
  - the HUD shows food, water, rest, temperature and conditions;
  - all of it is saved with the player.

## Impact

- New capability `survival`.
- What there is to eat and drink, and cooking, come with `add-harvest-and-cooking`. The
  carpet's cloak and tent for warmth and shelter come in M6.
