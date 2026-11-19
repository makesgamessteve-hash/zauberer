-- SPDX-License-Identifier: MIT
-- What people say (concept 6.10): greetings by how they feel about the
-- heirs, the openings of topics, and the requests the world's state gives
-- rise to. %s are filled from the world and the history database.
return {
  greet = {
    warm = { "Peace be with you, child of the road.", "Welcome, welcome! Sit, there is tea.",
      "Ah, a guest. Our house is yours." },
    neutral = { "Good day, traveller.", "Yes? What do you want?", "You are not from here." },
    wary = { "Keep your hands where I can see them.", "We do not want trouble.", "State your business." },
    afraid = { "Please... take what you want and go.", "Stay back, mage!" },
    hostile = { "Get out of our village!", "Witch! Begone!" },
  },
  topics = { "rumours", "directions", "history", "faith", "songs", "grudges", "prices", "work" },
  topic_intro = {
    rumours = "They say...", directions = "If you are going to %s, ", history = "In the old days...",
    faith = "As for the gods...", songs = "There is a song we sing:", grudges = "Do not speak to me of them.",
    prices = "At the market here,", work = "If you are looking for work...",
  },
  requests = {
    predator = "Something is taking our flocks at night. Kill it, and the village will not forget.",
    lost_child = "My little one went into the woods at dawn and has not come back. Please find %s!",
    poisoned_well = "The well tastes bitter and the old ones are sick. Something is in the water.",
    drought = "No rain since the spring. The fields are dust. If only someone could call the rain...",
    wildfire = "Smoke to the %s! The fire is coming this way!",
    bandits = "Bandits in the hills by %s rob every caravan. Trade is dying.",
  },
  services = {
    trade = "Look, look: the best goods of %s!", lodging = "A room for the night, two coins, with bread.",
    healing = "Let me see that wound.", teaching = "I can teach you a little of %s, if you have patience.",
    guide = "I know the way to %s. Follow me.",
  },
}
