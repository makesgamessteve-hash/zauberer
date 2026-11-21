/* SPDX-License-Identifier: MIT */
/* Creatures in the active tier (concept 5.12).
 *
 * Species are data (from the scripts): size, speed, whether they fly, diet,
 * what they hunt, when in the day they are about, senses, fears and a mana
 * metabolism for magical ones. Each creature senses the player (sight scaled
 * by daylight), other creatures, stimuli such as noises and lights, the scent
 * the wind carries, water and fire nearby. Five times a second, staggered, a
 * utility choice picks what it does from its needs (hunger, thirst, rest,
 * safety): wander, graze, rest, flee, hunt the player or its prey (stalking,
 * then a chase it gives up when spent), eat a kill, drink, watch a danger
 * far off, or follow a scent upwind. Herds and packs keep together, apart and
 * heading the same way, and stampede when one of them takes fright. Every
 * creature leaves its scent in the air. Walkers keep to the ground and keep
 * out of open water (ice holds them); fliers keep their height above the
 * ground. Heat, smoke, fast bodies and bites hurt them; magic hurts and heals
 * them through the scripts. When one dies an event tells the scripts, with
 * its killer (magical creatures then release motes); the dead fall to the
 * ground. Outside the active tier they wait where they are. */
#ifndef ZB_SIM_CREATURES_H
#define ZB_SIM_CREATURES_H

#include "sim/air.h"
#include "sim/bodies.h"
#include "sim/fire.h"
#include "sim/water.h"

#define ZB_SPECIES_MAX 32
#define ZB_CREATURES_MAX 512
#define ZB_CREATURE_DECIDE_HZ 5
#define ZB_PREY_MAX 8 /* species a predator hunts */

typedef enum zb_diet { ZB_DIET_HERBIVORE, ZB_DIET_CARNIVORE, ZB_DIET_OMNIVORE, ZB_DIETS } zb_diet;

typedef enum zb_action {
  ZB_ACT_WANDER,
  ZB_ACT_GRAZE,
  ZB_ACT_REST,
  ZB_ACT_FLEE,
  ZB_ACT_HUNT,
  ZB_ACT_INVESTIGATE, /* follow a scent or a noise */
  ZB_ACT_DRINK,       /* go to water and drink */
  ZB_ACT_FEED,        /* eat a kill */
  ZB_ACT_ALERT,       /* stand and watch a danger far off */
  ZB_ACT_FOLLOW,      /* keep near the player it is bound to */
  ZB_ACTIONS
} zb_action;

/* When in the day a species is about (twilight: from dusk to dawn, resting in
 * full day); the rest of the time it rests. */
typedef enum zb_activity {
  ZB_ACTIVE_ANY,
  ZB_ACTIVE_DAY,
  ZB_ACTIVE_NIGHT,
  ZB_ACTIVE_TWILIGHT,
  ZB_ACTIVITIES
} zb_activity;

typedef enum zb_harm {
  ZB_HARM_HEAT,
  ZB_HARM_IMPACT,
  ZB_HARM_COLD,
  ZB_HARM_POISON,
  ZB_HARM_SMOKE,
  ZB_HARM_MAGIC,
  ZB_HARMS
} zb_harm;

typedef struct zb_animal {
  char id[32];
  double length;    /* metres nose to tail */
  double mass;      /* kg */
  double walk, run; /* m/s */
  bool flies;
  double cruise; /* metres above the ground for fliers */
  zb_diet diet;
  bool pack;                    /* keeps together with its kind */
  double sight, hearing, smell; /* metres */
  double health;
  double aggression;       /* 0 .. 1: how readily it hunts the player */
  double fear_of_fire;     /* 0 .. 1 */
  double fear_of_player;   /* 0 .. 1 */
  double flight;           /* metres: within it the player frightens it (0: 25) */
  double resist[ZB_HARMS]; /* 0 .. 1 share of each harm ignored */
  bool magical;
  int mana_color; /* zb_mana_color of a magical one, -1 */
  double motes;   /* mana it releases at death */
  float color[3]; /* body colour for drawing */
  zb_activity activity;
  double thirst;              /* per second while about (0: never drinks) */
  double stamina;             /* seconds it can run before it is spent (0: 50) */
  char eats[ZB_PREY_MAX][32]; /* ids of the species it hunts */
  int eats_count;
} zb_animal;

typedef struct zb_creature_state {
  uint32_t id;
  int species;
  zb_planet_pos pos;
  double heading; /* radians clockwise from north */
  double speed;   /* m/s */
  double health, hunger, thirst, fear, energy;
  zb_action action;
  bool alive, frozen;
  double sees_player; /* metres to the player when seen, else -1 */
  uint32_t target;    /* the creature it hunts or eats, 0 for none */
  double eaten;       /* of a carcass: 0 .. 1 eaten by predators */
  double bond;        /* metres it keeps from the player it is bound to (0: free) */
} zb_creature_state;

typedef enum zb_creature_event_kind { ZB_CREATURE_DIED, ZB_CREATURE_FLED, ZB_CREATURE_ATTACKED } zb_creature_event_kind;

typedef struct zb_creature_event {
  zb_creature_event_kind kind;
  uint32_t id;
  int species;
  zb_planet_pos pos;
  zb_harm cause; /* for deaths */
  uint32_t by;   /* for deaths: the creature that killed it, 0 for none */
} zb_creature_event;

typedef struct zb_creatures_stats {
  int alive, dead, frozen;
  int by_action[ZB_ACTIONS];
} zb_creatures_stats;

/* What the creatures read of the active tier (any may be NULL). */
typedef struct zb_creature_sources {
  const zb_fire *fire;
  const zb_surface_water *water;
  zb_air *air; /* scent is left in it */
  const zb_bodies *bodies;
} zb_creature_sources;

typedef struct zb_creatures zb_creatures;

zb_creatures *zb_creatures_create(const zb_allocator *a, zb_world *world, uint64_t seed);
void zb_creatures_destroy(zb_creatures *c);
void zb_creatures_set_world(zb_creatures *c, zb_world *world);
void zb_creatures_set_sources(zb_creatures *c, const zb_creature_sources *s);
/* A species; returns its index, or -1 (bad values or too many). */
int zb_creatures_define(zb_creatures *c, const zb_animal *s);
const zb_animal *zb_creatures_species(const zb_creatures *c, int index);
int zb_creatures_find_species(const zb_creatures *c, const char *id);
/* A creature of a species at pos (on the ground, or at its cruising height);
 * returns its id, or 0. */
uint32_t zb_creatures_spawn(zb_creatures *c, int species, const zb_planet_pos *pos);
bool zb_creatures_remove(zb_creatures *c, uint32_t id);
/* Where the player is, and whether on foot (a carpet high up is less of a
 * threat and harder to reach). */
void zb_creatures_set_player(zb_creatures *c, const zb_planet_pos *pos, bool on_foot);
/* How the creatures perceive the player: visibility scales the distance they
 * see the player at (0 .. 2: camouflage, light, movement; 1 plain), presence
 * how much the player frightens them (0 .. 3: a vivid cloak awes; 1 plain). */
void zb_creatures_set_player_presence(zb_creatures *c, double visibility, double presence);
/* Binds a creature to the player (summoned, a familiar): it keeps within
 * metres of the player, fears and hunts the player no more; 0 lets it go.
 * False when there is no such creature. */
bool zb_creatures_bond(zb_creatures *c, uint32_t id, double metres);
/* The player is defeated: the hunters within radius metres of the player
 * leave the player alone for the given seconds (they turn away now). Returns
 * how many did. */
int zb_creatures_spare_player(zb_creatures *c, double radius, double seconds);
/* A stimulus: a noise (heard within strength x hearing), a light (seen), or a
 * threat (feared) at pos for seconds. */
typedef enum zb_stimulus_kind { ZB_STIMULUS_NOISE, ZB_STIMULUS_LIGHT, ZB_STIMULUS_THREAT } zb_stimulus_kind;
void zb_creatures_stimulus(zb_creatures *c, zb_stimulus_kind kind, const zb_planet_pos *pos, double strength,
                           double seconds);
/* Hurts (amount > 0) or heals (amount < 0) a creature; returns whether it lives. */
bool zb_creatures_hurt(zb_creatures *c, uint32_t id, zb_harm harm, double amount);
/* Sets a creature's needs, 0 .. 1 (a negative value leaves one as it is):
 * hunger, thirst and energy. Returns whether it exists. */
bool zb_creatures_set_needs(zb_creatures *c, uint32_t id, double hunger, double thirst, double energy);
void zb_creatures_update(zb_creatures *c, double seconds);
bool zb_creatures_get(const zb_creatures *c, uint32_t id, zb_creature_state *out);
int zb_creatures_near(const zb_creatures *c, const zb_planet_pos *pos, double radius, uint32_t *ids, int max);
/* Events since last asked (deaths, fleeing, attacks), up to max. */
int zb_creatures_events(zb_creatures *c, zb_creature_event *out, int max);
void zb_creatures_get_stats(const zb_creatures *c, zb_creatures_stats *out);
uint64_t zb_creatures_hash(const zb_creatures *c);
/* Calls fn for every creature (alive or not) in id order. */
typedef void (*zb_creature_fn)(void *ctx, const zb_creature_state *s, double phase);
void zb_creatures_each(const zb_creatures *c, zb_creature_fn fn, void *ctx);
const char *zb_action_name(zb_action a);
const char *zb_harm_name(zb_harm h);

#endif
