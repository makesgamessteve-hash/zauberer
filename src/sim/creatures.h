/* SPDX-License-Identifier: MIT */
/* Creatures in the active tier (concept 5.12, M4's first creatures).
 *
 * Species are data (from the scripts): size, speed, whether they fly, diet,
 * senses, fears and a mana metabolism for magical ones. Each creature senses
 * the player (sight scaled by daylight), stimuli such as noises and lights,
 * the scent the wind carries, and fire nearby; five times a second, staggered,
 * a utility choice picks what it does: wander, graze, rest, flee, hunt the
 * player, or follow a scent upwind. Walkers keep to the ground and keep out of
 * open water (ice holds them); fliers keep their height above the ground.
 * Heat, smoke and fast bodies hurt them; magic hurts and heals them through
 * the scripts. When one dies an event tells the scripts (magical creatures
 * then release motes). Outside the active tier they wait where they are. */
#ifndef ZB_SIM_CREATURES_H
#define ZB_SIM_CREATURES_H

#include "sim/air.h"
#include "sim/bodies.h"
#include "sim/fire.h"
#include "sim/water.h"

#define ZB_SPECIES_MAX 32
#define ZB_CREATURES_MAX 512
#define ZB_CREATURE_DECIDE_HZ 5

typedef enum zb_diet { ZB_DIET_HERBIVORE, ZB_DIET_CARNIVORE, ZB_DIET_OMNIVORE, ZB_DIETS } zb_diet;

typedef enum zb_action {
  ZB_ACT_WANDER,
  ZB_ACT_GRAZE,
  ZB_ACT_REST,
  ZB_ACT_FLEE,
  ZB_ACT_HUNT,
  ZB_ACT_INVESTIGATE, /* follow a scent or a noise */
  ZB_ACTIONS
} zb_action;

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
  double resist[ZB_HARMS]; /* 0 .. 1 share of each harm ignored */
  bool magical;
  int mana_color; /* zb_mana_color of a magical one, -1 */
  double motes;   /* mana it releases at death */
  float color[3]; /* body colour for drawing */
} zb_animal;

typedef struct zb_creature_state {
  uint32_t id;
  int species;
  zb_planet_pos pos;
  double heading; /* radians clockwise from north */
  double speed;   /* m/s */
  double health, hunger, fear, energy;
  zb_action action;
  bool alive, frozen;
  double sees_player; /* metres to the player when seen, else -1 */
} zb_creature_state;

typedef enum zb_creature_event_kind { ZB_CREATURE_DIED, ZB_CREATURE_FLED, ZB_CREATURE_ATTACKED } zb_creature_event_kind;

typedef struct zb_creature_event {
  zb_creature_event_kind kind;
  uint32_t id;
  int species;
  zb_planet_pos pos;
  zb_harm cause; /* for deaths */
} zb_creature_event;

typedef struct zb_creatures_stats {
  int alive, dead, frozen;
  int by_action[ZB_ACTIONS];
} zb_creatures_stats;

/* What the creatures read of the active tier (any may be NULL). */
typedef struct zb_creature_sources {
  const zb_fire *fire;
  const zb_surface_water *water;
  const zb_air *air;
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
/* A stimulus: a noise (heard within strength x hearing), a light (seen), or a
 * threat (feared) at pos for seconds. */
typedef enum zb_stimulus_kind { ZB_STIMULUS_NOISE, ZB_STIMULUS_LIGHT, ZB_STIMULUS_THREAT } zb_stimulus_kind;
void zb_creatures_stimulus(zb_creatures *c, zb_stimulus_kind kind, const zb_planet_pos *pos, double strength,
                           double seconds);
/* Hurts (amount > 0) or heals (amount < 0) a creature; returns whether it lives. */
bool zb_creatures_hurt(zb_creatures *c, uint32_t id, zb_harm harm, double amount);
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
