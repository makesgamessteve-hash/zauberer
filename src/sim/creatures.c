/* SPDX-License-Identifier: MIT */
#include "sim/creatures.h"

#include "core/array.h"
#include "core/dmath.h"
#include "core/hash.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <math.h>
#include <string.h>

#define TURN_RATE 3.0     /* rad/s */
#define ACCEL 6.0         /* m/s^2 */
#define HUNGER_RATE 0.002 /* per second */
#define STAMINA 50.0      /* seconds a runner lasts when its species does not say */
#define EARSHOT 0.3       /* share of its hearing within which it notices other creatures in the dark */
#define REST_RATE 0.05    /* energy per second resting */
#define FEAR_FADE 0.9     /* fear kept per decision */
#define WANDER_RANGE 30.0 /* metres */
#define PACK_RANGE 60.0
#define PACK_KEEP 15.0
#define ATTACK_RANGE 1.6
#define ATTACK_COOL 1.5 /* seconds */
#define HIT_COOL 0.5    /* seconds between blows from bodies */
#define FAST_BODY 4.0   /* m/s: slower bodies only nudge */
#define COMMITMENT 0.05 /* utility kept for what it is doing already */
#define HARM_STEP 0.25  /* seconds between looks at heat and smoke */
#define GRAVITY 9.81
#define BURN 25.0         /* health per second in a burning cell */
#define SMOKE_THICK 0.005 /* kg/m^3 that chokes */
#define SCENT_FAINT 1e-7  /* kg/m^3 that a nose notices */
#define SCENT_RATE 1e-6   /* kg of scent a creature leaves per kg of body and second */
#define SCENT_EVERY 1.0   /* seconds between scent marks */
#define SCENT_LOOK 12.0   /* metres ahead a tracker smells for the stronger trail */
#define THIRST_DRINK 0.06 /* thirst quenched per second drinking */
#define WATER_LOOK 60.0   /* metres: how far a thirsty creature looks for water */
#define STALK_RANGE 25.0  /* metres: a hunter this close gives chase */
#define GIVE_UP 0.15      /* energy below which a hunter gives up the chase */
#define GIVE_UP_TIME 30.0 /* seconds it then leaves prey alone */
#define BITE 0.4          /* health a bite takes per kg of the hunter */
#define FEED_RATE 0.05    /* hunger eaten away per second at a kill */
#define DANGER_RANGE 35.0 /* metres: a hunter this close is fled */
#define CROWD 2.5         /* metres: herd mates keep this far apart */
#define FLOCK_RANGE 15.0  /* metres: herd mates heading the same way */
#define STIMULI_MAX 64
#define EVENTS_MAX 256

typedef struct creature {
  uint32_t id;
  int species;
  zb_planet_pos pos;
  double heading, speed;
  double health, hunger, fear, energy;
  zb_action action;
  double decide_in, attack_cool, hit_cool, phase, sees_player;
  double falling;       /* m/s down: a dead flier dropping */
  double thirst, eaten; /* eaten: of a carcass, 0 .. 1 */
  double scent_wait, gave_up_until;
  double flock;    /* the heading its herd mates pull it to */
  uint32_t target; /* the creature it hunts or eats */
  zb_planet_pos goal, threat, water;
  bool has_goal, has_threat, has_water, has_flock, fled_told;
  bool alive, frozen;
  double bond; /* bound to the player: the distance it keeps (0: free) */
} creature;

typedef struct stimulus {
  zb_stimulus_kind kind;
  zb_planet_pos pos;
  double strength, until;
} stimulus;

struct zb_creatures {
  const zb_allocator *a;
  zb_world *world;
  zb_creature_sources src;
  zb_animal species[ZB_SPECIES_MAX];
  int species_count;
  uint32_t prey[ZB_SPECIES_MAX]; /* bit j: species i hunts species j */
  zb_array list;                 /* creature, in id order */
  zb_array cart;                 /* zb_dvec3 per creature: where they are this tick */
  uint32_t next_id;
  uint64_t rng;
  zb_planet_pos player;
  bool has_player, on_foot;
  double visibility, presence; /* of the player: 1 plain */
  stimulus stimuli[STIMULI_MAX];
  int stimulus_count;
  zb_creature_event events[EVENTS_MAX];
  int event_count;
  double time, harm_wait;
};

static creature *C(const zb_creatures *c, int i) { return &((creature *)c->list.data)[i]; }
static int count(const zb_creatures *c) { return (int)c->list.len; }
static const zb_planet *P(const zb_creatures *c) { return zb_world_planet(c->world); }

static double rnd(zb_creatures *c) {
  c->rng ^= c->rng << 13;
  c->rng ^= c->rng >> 7;
  c->rng ^= c->rng << 17;
  return (double)(c->rng >> 11) / 9007199254740992.0;
}

static double dist(const zb_creatures *c, const zb_planet_pos *a, const zb_planet_pos *b) {
  double s = zb_planet_distance(P(c), a, b), h = (double)a->alt - b->alt;
  return sqrt(s * s + h * h);
}

/* The heading from a to b (radians clockwise from north). */
static double bearing(const zb_creatures *c, const zb_planet_pos *a, const zb_planet_pos *b) {
  zb_dvec3 pa = zb_planet_to_cart(P(c), a), pb = zb_planet_to_cart(P(c), b), e, n, u;
  zb_planet_frame(zb_dv_norm(pa), &e, &n, &u);
  zb_dvec3 d = zb_dv_sub(pb, pa);
  return zb_datan2(zb_dv_dot(d, e), zb_dv_dot(d, n));
}

/* The place metres away along a heading. */
static zb_planet_pos ahead(const zb_creatures *c, const zb_planet_pos *from, double heading, double metres) {
  double h;
  return zb_planet_move(P(c), from, heading, metres, 0.0, 0.0, &h);
}

static double wrap(double a) {
  while (a > ZB_DPI) {
    a -= 2.0 * ZB_DPI;
  }
  while (a < -ZB_DPI) {
    a += 2.0 * ZB_DPI;
  }
  return a;
}

/* ---- Set-up ---- */

zb_creatures *zb_creatures_create(const zb_allocator *a, zb_world *world, uint64_t seed) {
  zb_creatures *c = zb_calloc(a, sizeof *c);
  if (!c) {
    return NULL;
  }
  c->a = a;
  c->world = world;
  c->rng = seed * 0x9E3779B97F4A7C15ull | 1;
  c->next_id = 1;
  c->visibility = c->presence = 1.0;
  zb_array_init(&c->list, a, sizeof(creature));
  zb_array_init(&c->cart, a, sizeof(zb_dvec3));
  return c;
}

void zb_creatures_destroy(zb_creatures *c) {
  if (c) {
    zb_array_free(&c->list);
    zb_array_free(&c->cart);
    zb_free(c->a, c);
  }
}

void zb_creatures_set_world(zb_creatures *c, zb_world *world) {
  c->world = world;
  zb_array_clear(&c->list);
  c->stimulus_count = c->event_count = 0;
  c->has_player = false;
  c->visibility = c->presence = 1.0;
}

void zb_creatures_set_sources(zb_creatures *c, const zb_creature_sources *s) { c->src = *s; }

int zb_creatures_define(zb_creatures *c, const zb_animal *s) {
  bool ok = s->id[0] && s->length > 0.05 && s->length <= 50.0 && s->mass > 0.0 && s->walk > 0.0 && s->run >= s->walk &&
            s->run <= 60.0 && s->health > 0.0 && s->diet >= 0 && s->diet < ZB_DIETS;
  if (!ok) {
    return -1;
  }
  int k = zb_creatures_find_species(c, s->id);
  if (k < 0) {
    if (c->species_count >= ZB_SPECIES_MAX) {
      return -1;
    }
    k = c->species_count++;
  }
  c->species[k] = *s; /* a later definition of the same id replaces it */
  c->species[k].id[sizeof c->species[k].id - 1] = '\0';
  if (c->species[k].activity < 0 || c->species[k].activity >= ZB_ACTIVITIES) {
    c->species[k].activity = ZB_ACTIVE_ANY;
  }
  c->species[k].eats_count = s->eats_count < 0 ? 0 : (s->eats_count > ZB_PREY_MAX ? ZB_PREY_MAX : s->eats_count);
  for (int i = 0; i < c->species_count; ++i) { /* prey may be defined before or after their hunters */
    c->prey[i] = 0;
    for (int e = 0; e < c->species[i].eats_count; ++e) {
      c->species[i].eats[e][sizeof c->species[i].eats[e] - 1] = '\0';
      int j = zb_creatures_find_species(c, c->species[i].eats[e]);
      c->prey[i] |= j >= 0 ? 1u << j : 0u;
    }
  }
  return k;
}

const zb_animal *zb_creatures_species(const zb_creatures *c, int index) {
  return index >= 0 && index < c->species_count ? &c->species[index] : NULL;
}

int zb_creatures_find_species(const zb_creatures *c, const char *id) {
  for (int k = 0; id && k < c->species_count; ++k) {
    if (!strcmp(c->species[k].id, id)) {
      return k;
    }
  }
  return -1;
}

static double ground_at(const zb_creatures *c, const zb_planet_pos *pos) {
  return zb_world_ground_height(c->world, pos);
}

/* The height a creature keeps: on the ground (or ice), or above it. */
static float height_for(const zb_creatures *c, const zb_animal *s, const zb_planet_pos *pos) {
  double h = ground_at(c, pos);
  if (c->src.water) {
    zb_water_cell w;
    zb_surface_water_cell_at(c->src.water, pos, &w);
    if (w.loaded && w.frozen) {
      h = w.level > h ? w.level : h;
    }
  }
  return (float)(s->flies ? h + s->cruise : h);
}

uint32_t zb_creatures_spawn(zb_creatures *c, int species, const zb_planet_pos *pos) {
  if (species < 0 || species >= c->species_count || count(c) >= ZB_CREATURES_MAX) {
    return 0;
  }
  const zb_animal *s = &c->species[species];
  creature cr;
  memset(&cr, 0, sizeof cr);
  cr.id = c->next_id++;
  cr.species = species;
  cr.pos = *pos;
  cr.pos.alt = height_for(c, s, pos);
  cr.heading = rnd(c) * 2.0 * ZB_DPI - ZB_DPI;
  cr.health = s->health;
  cr.hunger = 0.3 + 0.3 * rnd(c);
  cr.energy = 1.0;
  cr.decide_in = rnd(c) / ZB_CREATURE_DECIDE_HZ; /* staggered */
  cr.sees_player = -1.0;
  cr.alive = true;
  return zb_array_push(&c->list, &cr) ? cr.id : 0;
}

static int find(const zb_creatures *c, uint32_t id) {
  int lo = 0, hi = count(c) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t m = C(c, mid)->id;
    if (m == id) {
      return mid;
    }
    if (m < id) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return -1;
}

bool zb_creatures_remove(zb_creatures *c, uint32_t id) {
  int i = find(c, id);
  if (i < 0) {
    return false;
  }
  creature *all = (creature *)c->list.data;
  memmove(&all[i], &all[i + 1], sizeof(creature) * (c->list.len - (size_t)i - 1));
  c->list.len--;
  return true;
}

void zb_creatures_set_player(zb_creatures *c, const zb_planet_pos *pos, bool on_foot) {
  c->has_player = pos != NULL;
  if (pos) {
    c->player = *pos;
  }
  c->on_foot = on_foot;
}

bool zb_creatures_bond(zb_creatures *c, uint32_t id, double metres) {
  int i = find(c, id);
  if (i < 0) {
    return false;
  }
  creature *cr = C(c, i);
  cr->bond = metres > 0.0 ? metres : 0.0;
  if (cr->bond > 0.0) {
    cr->fear = 0.0;
    cr->has_threat = false;
  }
  return true;
}

void zb_creatures_set_player_presence(zb_creatures *c, double visibility, double presence) {
  c->visibility = visibility < 0.0 ? 0.0 : (visibility > 2.0 ? 2.0 : visibility);
  c->presence = presence < 0.0 ? 0.0 : (presence > 3.0 ? 3.0 : presence);
}

void zb_creatures_stimulus(zb_creatures *c, zb_stimulus_kind kind, const zb_planet_pos *pos, double strength,
                           double seconds) {
  if (c->stimulus_count >= STIMULI_MAX) { /* the oldest makes room */
    memmove(&c->stimuli[0], &c->stimuli[1], sizeof(stimulus) * (STIMULI_MAX - 1));
    c->stimulus_count--;
  }
  c->stimuli[c->stimulus_count++] = (stimulus){kind, *pos, strength, c->time + (seconds > 0.0 ? seconds : 0.0)};
}

static void event_by(zb_creatures *c, zb_creature_event_kind kind, const creature *cr, zb_harm cause, uint32_t by) {
  if (c->event_count < EVENTS_MAX) {
    c->events[c->event_count++] = (zb_creature_event){kind, cr->id, cr->species, cr->pos, cause, by};
  }
}

static void event(zb_creatures *c, zb_creature_event_kind kind, const creature *cr, zb_harm cause) {
  event_by(c, kind, cr, cause, 0);
}

/* Hurts (or heals) a creature; by: the creature that dealt it, 0 for none. */
static void harm_by(zb_creatures *c, creature *cr, zb_harm h, double amount, uint32_t by) {
  if (!cr->alive) {
    return;
  }
  const zb_animal *s = &c->species[cr->species];
  if (amount > 0.0) {
    amount *= 1.0 - (s->resist[h] < 1.0 ? s->resist[h] : 1.0);
  }
  cr->health -= amount;
  cr->health = cr->health > s->health ? s->health : cr->health;
  if (cr->health <= 0.0) {
    cr->health = 0.0;
    cr->alive = false;
    cr->speed = 0.0;
    event_by(c, ZB_CREATURE_DIED, cr, h, by);
  }
}

static void harm(zb_creatures *c, creature *cr, zb_harm h, double amount) { harm_by(c, cr, h, amount, 0); }

bool zb_creatures_hurt(zb_creatures *c, uint32_t id, zb_harm h, double amount) {
  int i = find(c, id);
  if (i < 0 || h < 0 || h >= ZB_HARMS) {
    return false;
  }
  creature *cr = C(c, i);
  harm(c, cr, h, amount);
  if (amount > 0.0 && cr->alive) { /* pain is a reason to run */
    cr->fear = cr->fear > 0.8 ? cr->fear : 0.8;
  }
  return cr->alive;
}

static double unit(double v) { return v > 1.0 ? 1.0 : v; }

bool zb_creatures_set_needs(zb_creatures *c, uint32_t id, double hunger, double thirst, double energy) {
  int i = find(c, id);
  if (i < 0) {
    return false;
  }
  creature *cr = C(c, i);
  cr->hunger = hunger >= 0.0 ? unit(hunger) : cr->hunger;
  cr->thirst = thirst >= 0.0 ? unit(thirst) : cr->thirst;
  cr->energy = energy >= 0.0 ? unit(energy) : cr->energy;
  cr->decide_in = 0.0; /* it reconsiders at once */
  return true;
}

/* ---- Perception and choice ---- */

typedef struct senses {
  double light;  /* 0 night .. 1 day */
  double player; /* metres when perceived, else -1 */
  bool fire;     /* burning nearby */
  zb_planet_pos fire_at;
  double fire_distance;
  bool scent;    /* a scent on the wind */
  double upwind; /* heading into the wind */
  bool noise;
  zb_planet_pos noise_at;
  double noise_strength;
  bool threat;
  zb_planet_pos threat_at;
} senses;

static void sense(zb_creatures *c, const creature *cr, senses *out) {
  const zb_animal *s = &c->species[cr->species];
  memset(out, 0, sizeof *out);
  out->player = -1.0;
  zb_sky_place sky;
  zb_calendar_sky(zb_world_calendar(c->world), zb_world_time(c->world), zb_dv_norm(zb_planet_to_cart(P(c), &cr->pos)),
                  &sky);
  out->light = sky.daylight;
  double sight = s->sight * (0.3 + 0.7 * out->light);
  if (c->has_player) {
    double d = dist(c, &cr->pos, &c->player);
    if (d <= sight * c->visibility || d <= s->hearing * 0.25) {
      out->player = d;
    }
  }
  /* Fire: the cell it stands on and two rings around it. */
  out->fire_distance = 1e9;
  for (int ring = 0; c->src.fire && ring < 3; ++ring) {
    int n = ring == 0 ? 1 : 8;
    for (int k = 0; k < n; ++k) {
      double r = ring * 8.0;
      zb_planet_pos p = ahead(c, &cr->pos, k * ZB_DPI / 4.0, r);
      zb_fire_cell fc;
      zb_fire_cell_at(c->src.fire, &p, &fc);
      if (fc.burning && r < out->fire_distance) {
        out->fire = true;
        out->fire_at = p;
        out->fire_distance = r;
      }
    }
  }
  if (c->src.air && s->smell > 0.0) {
    zb_air_sample as;
    zb_air_sample_at(c->src.air, &cr->pos, &as);
    if (as.loaded && as.gas[ZB_GAS_SCENT] > SCENT_FAINT * 30.0 / s->smell) {
      out->scent = true;
      out->upwind = zb_datan2(-as.east, -as.north);
    }
  }
  for (int k = 0; k < c->stimulus_count; ++k) {
    const stimulus *st = &c->stimuli[k];
    double d = dist(c, &cr->pos, &st->pos);
    if (st->kind == ZB_STIMULUS_NOISE && d <= s->hearing * st->strength) {
      out->noise = true;
      out->noise_at = st->pos;
      out->noise_strength = 1.0 - d / (s->hearing * st->strength);
    } else if (st->kind == ZB_STIMULUS_LIGHT && d <= sight * (1.0 + (1.0 - out->light) * st->strength)) {
      if (out->light < 0.5) { /* a bright light in the dark: a fire to a wild animal */
        out->fire = out->fire || d < 20.0;
        out->fire_at = out->fire && d < out->fire_distance ? st->pos : out->fire_at;
        out->fire_distance = d < out->fire_distance ? d : out->fire_distance;
      }
    } else if (st->kind == ZB_STIMULUS_THREAT && d <= sight) {
      out->threat = true;
      out->threat_at = st->pos;
    }
  }
}

static void pick_goal(zb_creatures *c, creature *cr, double range) {
  double a = rnd(c) * 2.0 * ZB_DPI, r = range * (0.3 + 0.7 * rnd(c));
  cr->goal = ahead(c, &cr->pos, a, r);
  cr->has_goal = true;
}

/* Whether a species is about at a daylight (0 night .. 1 day). */
static bool about(const zb_animal *s, double light) {
  switch (s->activity) {
    case ZB_ACTIVE_DAY: return light > 0.3;
    case ZB_ACTIVE_NIGHT: return light < 0.3;
    case ZB_ACTIVE_TWILIGHT: return light < 0.7; /* dusk to dawn: it rests in full day */
    default: return true;
  }
}

static int index_of(const zb_creatures *c, const creature *cr) { return (int)(cr - C(c, 0)); }

/* Distance between creatures i and j this tick (the chord: metres apart). */
static double apart(const zb_creatures *c, int i, int j) {
  const zb_dvec3 *x = (const zb_dvec3 *)c->cart.data;
  return zb_dv_len(zb_dv_sub(x[i], x[j]));
}

/* What a creature makes of the creatures around it. */
typedef struct neighbours {
  int prey, danger, carcass; /* nearest of each, -1 for none */
  double prey_d, danger_d, carcass_d;
  double east, north; /* where its herd mates pull it (separation, alignment, cohesion) */
  int mates;
} neighbours;

static void look_around(const zb_creatures *c, const creature *cr, double sight, neighbours *out) {
  const zb_animal *s = &c->species[cr->species];
  int me = index_of(c, cr);
  out->prey = out->danger = out->carcass = -1;
  out->prey_d = out->danger_d = out->carcass_d = 1e9;
  out->east = out->north = 0.0;
  out->mates = 0;
  zb_dvec3 e, n, u, sum = zb_dv(0, 0, 0);
  const zb_dvec3 *x = (const zb_dvec3 *)c->cart.data;
  zb_planet_frame(zb_dv_norm(x[me]), &e, &n, &u);
  double align_e = 0.0, align_n = 0.0, sep_e = 0.0, sep_n = 0.0;
  for (int i = 0; i < count(c); ++i) {
    const creature *o = C(c, i);
    if (i == me || o->frozen) {
      continue;
    }
    double d = apart(c, me, i);
    if (d > sight && d > PACK_RANGE) {
      continue;
    }
    bool hunted = (c->prey[cr->species] >> o->species) & 1u;
    if (hunted && o->alive && d <= sight && d < out->prey_d && c->time >= cr->gave_up_until) {
      out->prey = i;
      out->prey_d = d;
    } else if (hunted && !o->alive && o->eaten < 1.0 && d <= sight && d < out->carcass_d) {
      out->carcass = i;
      out->carcass_d = d;
    }
    if (o->alive && ((c->prey[o->species] >> cr->species) & 1u) && d <= sight && d < out->danger_d) {
      out->danger = i;
      out->danger_d = d;
    }
    if (s->pack && o->alive && o->species == cr->species && d < PACK_RANGE) {
      sum = zb_dv_add(sum, x[i]);
      out->mates++;
      if (d < FLOCK_RANGE) {
        align_e += sin(o->heading);
        align_n += cos(o->heading);
      }
      if (d < CROWD && d > 1e-6) {
        zb_dvec3 away = zb_dv_scale(zb_dv_sub(x[me], x[i]), (CROWD - d) / (d * CROWD));
        sep_e += zb_dv_dot(away, e);
        sep_n += zb_dv_dot(away, n);
      }
    }
  }
  if (out->mates > 0) {
    zb_dvec3 to = zb_dv_sub(zb_dv_scale(sum, 1.0 / out->mates), x[me]);
    double far = zb_dv_len(to) > PACK_KEEP ? 1.0 / zb_dv_len(to) : 0.0; /* drawn back only when it strays */
    double align = sqrt(align_e * align_e + align_n * align_n);
    out->east = zb_dv_dot(to, e) * far + (align > 0.0 ? align_e / align : 0.0) * 0.5 + sep_e * 2.0;
    out->north = zb_dv_dot(to, n) * far + (align > 0.0 ? align_n / align : 0.0) * 0.5 + sep_n * 2.0;
  }
}

/* Open water within reach of a thirsty creature: the nearest of a few rings. */
static bool find_water(const zb_creatures *c, const creature *cr, zb_planet_pos *out) {
  if (!c->src.water) {
    return false;
  }
  for (double r = 6.0; r <= WATER_LOOK; r *= 2.0) {
    for (int k = 0; k < 16; ++k) {
      zb_planet_pos p = ahead(c, &cr->pos, k * ZB_DPI / 8.0, r);
      zb_water_cell w;
      zb_surface_water_cell_at(c->src.water, &p, &w);
      if (w.loaded && !w.frozen && w.depth > 0.05) {
        *out = p;
        return true;
      }
    }
  }
  return false;
}

/* Where a scent leads: of three places ahead into the wind, the one that
 * smells strongest. */
static zb_planet_pos follow_scent(const zb_creatures *c, const creature *cr, double upwind) {
  zb_planet_pos best = ahead(c, &cr->pos, upwind, SCENT_LOOK);
  double most = -1.0;
  for (int k = -1; k <= 1; ++k) {
    zb_planet_pos p = ahead(c, &cr->pos, upwind + 0.6 * k, SCENT_LOOK);
    zb_air_sample as;
    zb_air_sample_at(c->src.air, &p, &as);
    if (as.loaded && as.gas[ZB_GAS_SCENT] > most) {
      most = as.gas[ZB_GAS_SCENT];
      best = p;
    }
  }
  return best;
}

/* A creature that takes fright frightens its herd mates near it. */
static void stampede(zb_creatures *c, const creature *cr) {
  int me = index_of(c, cr);
  for (int i = 0; i < count(c); ++i) {
    creature *o = C(c, i);
    if (i != me && o->alive && o->species == cr->species && o->fear < cr->fear * 0.8 && apart(c, me, i) < PACK_RANGE) {
      o->fear = cr->fear * 0.8;
      o->threat = cr->threat;
      o->has_threat = true;
      o->decide_in = 0.0; /* it sees the others run at once */
    }
  }
}

static void decide(zb_creatures *c, creature *cr) {
  const zb_animal *s = &c->species[cr->species];
  senses se;
  sense(c, cr, &se);
  cr->sees_player = se.player;
  double sight = s->sight * (0.3 + 0.7 * se.light), notice = s->hearing * EARSHOT;
  neighbours nb;
  look_around(c, cr, sight > notice ? sight : notice, &nb);
  /* Fear: of fire, of hunters, of the player close by, of threats and loud noises. */
  double fear = cr->fear * FEAR_FADE;
  if (se.fire) {
    double f = s->fear_of_fire * (se.fire_distance < 12.0 ? 1.0 : 0.6);
    if (f > fear) {
      fear = f;
      cr->threat = se.fire_at;
      cr->has_threat = true;
    }
  }
  if (nb.danger >= 0 && nb.danger_d < DANGER_RANGE) {
    double f = 1.0 - nb.danger_d / (DANGER_RANGE * 1.5);
    if (f > fear) {
      fear = f;
      cr->threat = C(c, nb.danger)->pos;
      cr->has_threat = true;
    }
  }
  double flight = s->flight > 0.0 ? s->flight : 25.0;
  if (cr->bond <= 0.0 && se.player >= 0.0 && se.player < flight) { /* (the bound fear the player no more) */
    double f = s->fear_of_player * c->presence * (1.0 - se.player / flight);
    f = f > 1.0 ? 1.0 : f;
    if (f > fear) {
      fear = f;
      cr->threat = c->player;
      cr->has_threat = true;
    }
  }
  if (se.threat && 0.9 > fear) {
    fear = 0.9;
    cr->threat = se.threat_at;
    cr->has_threat = true;
  }
  if (se.noise && s->fear_of_player * se.noise_strength > fear) {
    fear = s->fear_of_player * se.noise_strength;
    cr->threat = se.noise_at;
    cr->has_threat = true;
  }
  cr->fear = fear > 1.0 ? 1.0 : fear;
  double calm = 1.0 - cr->fear;
  bool eats_plants = s->diet != ZB_DIET_CARNIVORE, eats_meat = s->diet != ZB_DIET_HERBIVORE;
  bool awake = about(s, se.light);
  if (cr->thirst > 0.3 && !cr->has_water) {
    cr->has_water = find_water(c, cr, &cr->water);
  }
  /* The chase is given up when the hunter is spent. */
  if (cr->action == ZB_ACT_HUNT && cr->target && cr->energy < GIVE_UP) {
    cr->gave_up_until = c->time + GIVE_UP_TIME;
    cr->target = 0;
    nb.prey = -1;
  }
  double score[ZB_ACTIONS];
  score[ZB_ACT_FLEE] = cr->fear * 1.2;
  double hunt_player = eats_meat && se.player >= 0.0 && cr->bond <= 0.0
                           ? s->aggression * (0.4 + cr->hunger) * (c->on_foot ? 1.0 : 0.1) * calm
                           : 0.0;
  bool chasing = cr->action == ZB_ACT_HUNT && cr->target; /* a chase goes on until the hunter is spent */
  double hunt_prey =
      nb.prey >= 0 && cr->energy > (chasing ? GIVE_UP : GIVE_UP * 2.0) ? (0.2 + cr->hunger * 0.9) * calm : 0.0;
  score[ZB_ACT_HUNT] = hunt_player > hunt_prey ? hunt_player : hunt_prey;
  score[ZB_ACT_FEED] = nb.carcass >= 0 && cr->hunger > 0.05 ? (0.3 + cr->hunger) * calm : 0.0;
  score[ZB_ACT_INVESTIGATE] =
      ((se.scent && eats_meat ? 0.3 + 0.4 * cr->hunger : 0.0) + (se.noise ? 0.2 : 0.0)) * calm * (awake ? 1.0 : 0.3);
  score[ZB_ACT_GRAZE] = eats_plants && !s->flies && awake ? cr->hunger * 0.8 * calm : 0.0;
  score[ZB_ACT_DRINK] = cr->has_water ? cr->thirst * calm : 0.0;
  score[ZB_ACT_REST] = ((1.0 - cr->energy) * 0.7 + (awake ? 0.0 : 0.6)) * calm * (nb.danger >= 0 ? 0.2 : 1.0);
  score[ZB_ACT_ALERT] = nb.danger >= 0 && nb.danger_d >= DANGER_RANGE ? 0.35 * calm + 0.1 : 0.0;
  score[ZB_ACT_WANDER] = 0.25;
  score[ZB_ACT_FOLLOW] =
      cr->bond > 0.0 && c->has_player && dist(c, &cr->pos, &c->player) > cr->bond ? 0.9 : 0.0; /* (to the player) */
  score[cr->action] += COMMITMENT; /* no dithering between two close choices */
  zb_action best = ZB_ACT_WANDER;
  for (int a = 0; a < ZB_ACTIONS; ++a) {
    best = score[a] > score[best] ? (zb_action)a : best;
  }
  if (best == ZB_ACT_FLEE && cr->action != ZB_ACT_FLEE && !cr->fled_told) {
    event(c, ZB_CREATURE_FLED, cr, ZB_HARM_HEAT);
    cr->fled_told = true;
  }
  if (best == ZB_ACT_FLEE && s->pack) {
    stampede(c, cr);
  }
  if (best != ZB_ACT_FLEE) {
    cr->fled_told = cr->fear > 0.3 && cr->fled_told;
  }
  cr->target = 0;
  if (best == ZB_ACT_HUNT && hunt_prey >= hunt_player) {
    cr->target = C(c, nb.prey)->id;
  } else if (best == ZB_ACT_FEED) {
    cr->target = C(c, nb.carcass)->id;
  } else if (best == ZB_ACT_ALERT) {
    cr->threat = C(c, nb.danger)->pos;
    cr->has_threat = true;
  } else if (best == ZB_ACT_INVESTIGATE) {
    cr->goal = se.noise ? se.noise_at : follow_scent(c, cr, se.upwind);
    cr->has_goal = true;
  } else if (best == ZB_ACT_WANDER && (!cr->has_goal || cr->action != ZB_ACT_WANDER)) {
    pick_goal(c, cr, WANDER_RANGE);
  }
  cr->action = best;
  cr->has_flock = nb.mates > 0 && (nb.east != 0.0 || nb.north != 0.0);
  cr->flock = cr->has_flock ? zb_datan2(nb.east, nb.north) : cr->heading;
}

/* ---- Moving ---- */

/* Whether a walker may step onto pos: not into open water. */
static bool walkable(const zb_creatures *c, const zb_planet_pos *pos) {
  if (!c->src.water) {
    return true;
  }
  zb_water_cell w;
  zb_surface_water_cell_at(c->src.water, pos, &w);
  return !w.loaded || w.frozen || (!w.fixed && w.depth < 0.5) || (w.fixed && w.depth < 0.3);
}

/* A bite: the hunter's weight tells how hard. */
static void bite(zb_creatures *c, creature *cr, creature *prey) {
  harm_by(c, prey, ZB_HARM_IMPACT, BITE * c->species[cr->species].mass, cr->id);
  if (prey->alive) { /* pain is a reason to run */
    prey->fear = 1.0;
    prey->threat = cr->pos;
    prey->has_threat = true;
  }
  cr->attack_cool = ATTACK_COOL;
}

/* How fast it can run now: spent, it slows to a trot. */
static double top_speed(const zb_animal *s, const creature *cr) {
  double fresh = cr->energy / 0.3;
  return s->walk + (s->run - s->walk) * (fresh < 1.0 ? (fresh > 0.0 ? fresh : 0.0) : 1.0);
}

static void move(zb_creatures *c, creature *cr, double dt) {
  const zb_animal *s = &c->species[cr->species];
  double want = 0.0, toward = cr->heading, run = top_speed(s, cr);
  int t = cr->target ? find(c, cr->target) : -1;
  creature *other = t >= 0 ? C(c, t) : NULL;
  switch (cr->action) {
    case ZB_ACT_FLEE:
      want = run;
      if (cr->has_threat) {
        toward = bearing(c, &cr->threat, &cr->pos); /* away from it */
      }
      break;
    case ZB_ACT_HUNT:
      if (other && other->alive) { /* stalk, then chase */
        double d = dist(c, &cr->pos, &other->pos);
        want = d > STALK_RANGE && other->action != ZB_ACT_FLEE ? s->walk : run; /* it bolts: the chase is on */
        toward = bearing(c, &cr->pos, &other->pos);
        if (d < ATTACK_RANGE + c->species[other->species].length * 0.5 && cr->attack_cool <= 0.0) {
          bite(c, cr, other);
        }
      } else if (!cr->target) { /* the player */
        want = run;
        toward = c->has_player ? bearing(c, &cr->pos, &c->player) : cr->heading;
        if (c->has_player && c->on_foot && dist(c, &cr->pos, &c->player) < ATTACK_RANGE && cr->attack_cool <= 0.0) {
          event(c, ZB_CREATURE_ATTACKED, cr, ZB_HARM_IMPACT);
          cr->attack_cool = ATTACK_COOL;
        }
      }
      break;
    case ZB_ACT_FEED:
      if (other && dist(c, &cr->pos, &other->pos) > 2.0) {
        want = s->walk;
        toward = bearing(c, &cr->pos, &other->pos);
      }
      break;
    case ZB_ACT_DRINK:
      if (cr->has_water && dist(c, &cr->pos, &cr->water) > 3.0) {
        want = s->walk;
        toward = bearing(c, &cr->pos, &cr->water);
      }
      break;
    case ZB_ACT_ALERT:
      if (cr->has_threat) {
        toward = bearing(c, &cr->pos, &cr->threat); /* watching it */
      }
      break;
    case ZB_ACT_FOLLOW:
      if (c->has_player) {
        double d = dist(c, &cr->pos, &c->player);
        want = d > cr->bond * 3.0 ? run : (d > cr->bond ? s->walk : 0.0);
        toward = bearing(c, &cr->pos, &c->player);
      }
      break;
    case ZB_ACT_INVESTIGATE:
    case ZB_ACT_WANDER:
      want = cr->action == ZB_ACT_WANDER ? s->walk * 0.5 : s->walk;
      if (cr->has_goal) {
        toward = bearing(c, &cr->pos, &cr->goal);
        if (dist(c, &cr->pos, &cr->goal) < 2.0) {
          cr->has_goal = false;
          want = 0.0;
        }
      }
      break;
    default: break; /* grazing and resting stand still */
  }
  if (cr->has_flock && want > 0.0 && (cr->action == ZB_ACT_WANDER || cr->action == ZB_ACT_INVESTIGATE)) {
    double with = cr->action == ZB_ACT_WANDER ? 1.0 : 0.4; /* the herd leads a wander, the trail a search */
    toward = zb_datan2(sin(toward) + with * sin(cr->flock), cos(toward) + with * cos(cr->flock));
  }
  double turn = wrap(toward - cr->heading), most = TURN_RATE * dt;
  cr->heading = wrap(cr->heading + (turn > most ? most : (turn < -most ? -most : turn)));
  double dv = want - cr->speed, step = ACCEL * dt;
  cr->speed += dv > step ? step : (dv < -step ? -step : dv);
  if (cr->speed <= 0.0) {
    return;
  }
  double heading = cr->heading;
  zb_planet_pos next = zb_planet_move(P(c), &cr->pos, cr->heading, cr->speed * dt, 0.0, 0.0, &heading);
  if (!s->flies && !walkable(c, &next)) {
    if (cr->action == ZB_ACT_DRINK) { /* at the water's edge: it drinks here */
      cr->water = cr->pos;
      cr->speed = 0.0;
      return;
    }
    cr->heading = wrap(cr->heading + ZB_DPI / 2.0 + rnd(c)); /* turns away from the water */
    cr->speed *= 0.5;
    cr->has_goal = false;
    return;
  }
  float h = height_for(c, s, &next);
  next.alt = s->flies ? cr->pos.alt + (float)((h - cr->pos.alt) * (dt * 2.0 > 1.0 ? 1.0 : dt * 2.0)) : h;
  cr->pos = next;
  cr->heading = heading;
  cr->phase += cr->speed * dt / (s->length * 0.6);
  cr->energy -= (cr->speed > s->walk ? 1.0 / (s->stamina > 0.0 ? s->stamina : STAMINA) : 0.0) * dt;
}

/* Needs: hunger and thirst grow; grazing, eating a kill, drinking and rest
 * meet them. */
static void needs(zb_creatures *c, creature *cr, double dt) {
  const zb_animal *s = &c->species[cr->species];
  cr->hunger = cr->hunger + HUNGER_RATE * dt > 1.0 ? 1.0 : cr->hunger + HUNGER_RATE * dt;
  cr->thirst = cr->thirst + s->thirst * dt > 1.0 ? 1.0 : cr->thirst + s->thirst * dt;
  if (cr->action == ZB_ACT_GRAZE) {
    cr->hunger = cr->hunger - 0.02 * dt > 0.0 ? cr->hunger - 0.02 * dt : 0.0;
  }
  if (cr->action == ZB_ACT_REST) {
    cr->energy = cr->energy + REST_RATE * dt < 1.0 ? cr->energy + REST_RATE * dt : 1.0;
  }
  int t = cr->target ? find(c, cr->target) : -1;
  if (cr->action == ZB_ACT_FEED && t >= 0 && dist(c, &cr->pos, &C(c, t)->pos) <= 2.5) {
    creature *kill = C(c, t);
    double bite_of = FEED_RATE * dt;
    cr->hunger = cr->hunger - bite_of > 0.0 ? cr->hunger - bite_of : 0.0;
    kill->eaten += bite_of * s->mass / c->species[kill->species].mass;
    kill->eaten = kill->eaten > 1.0 ? 1.0 : kill->eaten;
  }
  if (cr->action == ZB_ACT_DRINK && cr->has_water && dist(c, &cr->pos, &cr->water) <= 3.5) {
    cr->thirst = cr->thirst - THIRST_DRINK * dt > 0.0 ? cr->thirst - THIRST_DRINK * dt : 0.0;
    cr->has_water = cr->thirst > 0.05;
  }
  cr->scent_wait -= dt;
  if (cr->scent_wait <= 0.0 && c->src.air) {
    zb_air_emit(c->src.air, &cr->pos, ZB_GAS_SCENT, s->mass * SCENT_RATE * SCENT_EVERY);
    cr->scent_wait += SCENT_EVERY;
  }
}

/* Heat and smoke. */
static void feel(zb_creatures *c, creature *cr, double dt) {
  const zb_animal *s = &c->species[cr->species];
  if (c->src.fire) {
    zb_fire_cell fc;
    zb_fire_cell_at(c->src.fire, &cr->pos, &fc);
    if (fc.burning && (!s->flies || cr->pos.alt - ground_at(c, &cr->pos) < 3.0)) {
      harm(c, cr, ZB_HARM_HEAT, BURN * dt);
    } else if (fc.temperature > 60.0) {
      harm(c, cr, ZB_HARM_HEAT, (fc.temperature - 60.0) / 10.0 * dt);
    }
  }
  if (c->src.air && cr->alive) {
    zb_air_sample as;
    zb_air_sample_at(c->src.air, &cr->pos, &as);
    if (as.loaded && as.gas[ZB_GAS_SMOKE] > SMOKE_THICK) {
      harm(c, cr, ZB_HARM_SMOKE, 2.0 * dt * as.gas[ZB_GAS_SMOKE] / SMOKE_THICK);
    }
  }
}

/* A fast body striking a creature: one blow of its kinetic energy (a health
 * point per kJ), then a moment before the next. */
static void struck(zb_creatures *c, creature *cr) {
  const zb_animal *s = &c->species[cr->species];
  uint32_t ids[8];
  int n = zb_bodies_near(c->src.bodies, &cr->pos, s->length * 0.5 + 1.0, ids, 8);
  for (int k = 0; k < n && cr->alive && cr->hit_cool <= 0.0; ++k) {
    zb_body_state b;
    if (zb_bodies_get(c->src.bodies, ids[k], &b) && b.speed > FAST_BODY) {
      harm(c, cr, ZB_HARM_IMPACT, 0.5 * b.mass * b.speed * b.speed / 1000.0);
      cr->hit_cool = HIT_COOL;
      cr->fear = cr->alive && cr->fear < 0.8 ? 0.8 : cr->fear;
    }
  }
}

/* The dead fall to the ground (a flier struck down drops out of the sky). */
static void drop(const zb_creatures *c, creature *cr, double dt) {
  const zb_animal *s = &c->species[cr->species];
  float floor = height_for(c, s, &cr->pos) - (s->flies ? (float)s->cruise : 0.0f);
  if (cr->pos.alt <= floor) {
    cr->falling = 0.0;
    return;
  }
  cr->falling += GRAVITY * dt;
  cr->pos.alt -= (float)(cr->falling * dt);
  if (cr->pos.alt <= floor) {
    cr->pos.alt = floor;
    cr->falling = 0.0;
  }
}

static bool active(const zb_creatures *c, const creature *cr) {
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(c->world), cr->pos.chunk);
  if (!ct || ct->stage < ZB_STAGE_TERRAIN) {
    return false;
  }
  if (!c->src.water) {
    return true;
  }
  zb_water_cell w;
  zb_surface_water_cell_at(c->src.water, &cr->pos, &w);
  return w.loaded;
}

/* Where every creature is this tick, for the distances between them. */
static void place_all(zb_creatures *c) {
  zb_array_clear(&c->cart);
  if (zb_array_reserve(&c->cart, (size_t)count(c)) != ZB_OK) {
    return;
  }
  for (int i = 0; i < count(c); ++i) {
    zb_dvec3 x = zb_planet_to_cart(P(c), &C(c, i)->pos);
    zb_array_push(&c->cart, &x);
  }
}

void zb_creatures_update(zb_creatures *c, double seconds) {
  c->time += seconds;
  place_all(c);
  if (c->cart.len != c->list.len) {
    return; /* out of memory: they wait */
  }
  c->harm_wait += seconds;
  bool feel_now = c->harm_wait >= HARM_STEP;
  if (feel_now) {
    c->harm_wait = 0.0;
  }
  bool bodies_moving = false;
  if (c->src.bodies) {
    zb_bodies_stats bs;
    zb_bodies_get_stats(c->src.bodies, &bs);
    bodies_moving = bs.awake > 0;
  }
  int kept = 0;
  for (int k = 0; k < c->stimulus_count; ++k) {
    if (c->stimuli[k].until > c->time) {
      c->stimuli[kept++] = c->stimuli[k];
    }
  }
  c->stimulus_count = kept;
  for (int i = 0; i < count(c); ++i) {
    creature *cr = C(c, i);
    cr->frozen = !active(c, cr);
    if (!cr->alive && !cr->frozen) {
      drop(c, cr, seconds);
    }
    if (!cr->alive || cr->frozen) {
      continue;
    }
    needs(c, cr, seconds);
    cr->attack_cool -= seconds;
    cr->decide_in -= seconds;
    if (cr->decide_in <= 0.0) {
      cr->decide_in += 1.0 / ZB_CREATURE_DECIDE_HZ;
      decide(c, cr);
    }
    move(c, cr, seconds);
    cr->hit_cool -= seconds;
    if (bodies_moving) {
      struck(c, cr);
    }
    if (feel_now && cr->alive) {
      feel(c, cr, HARM_STEP);
    }
  }
}

/* ---- Queries ---- */

static void state_of(const creature *cr, zb_creature_state *out) {
  out->id = cr->id;
  out->species = cr->species;
  out->pos = cr->pos;
  out->heading = cr->heading;
  out->speed = cr->speed;
  out->health = cr->health;
  out->hunger = cr->hunger;
  out->fear = cr->fear;
  out->energy = cr->energy;
  out->action = cr->action;
  out->alive = cr->alive;
  out->frozen = cr->frozen;
  out->sees_player = cr->sees_player;
  out->bond = cr->bond;
  out->thirst = cr->thirst;
  out->target = cr->target;
  out->eaten = cr->eaten;
}

bool zb_creatures_get(const zb_creatures *c, uint32_t id, zb_creature_state *out) {
  int i = find(c, id);
  memset(out, 0, sizeof *out);
  if (i < 0) {
    return false;
  }
  state_of(C(c, i), out);
  return true;
}

int zb_creatures_near(const zb_creatures *c, const zb_planet_pos *pos, double radius, uint32_t *ids, int max) {
  int n = 0;
  for (int i = 0; i < count(c) && n < max; ++i) {
    if (dist(c, &C(c, i)->pos, pos) <= radius) {
      ids[n++] = C(c, i)->id;
    }
  }
  return n;
}

int zb_creatures_events(zb_creatures *c, zb_creature_event *out, int max) {
  int n = c->event_count < max ? c->event_count : max;
  memcpy(out, c->events, sizeof(zb_creature_event) * (size_t)n);
  memmove(c->events, c->events + n, sizeof(zb_creature_event) * (size_t)(c->event_count - n));
  c->event_count -= n;
  return n;
}

void zb_creatures_get_stats(const zb_creatures *c, zb_creatures_stats *out) {
  memset(out, 0, sizeof *out);
  for (int i = 0; i < count(c); ++i) {
    const creature *cr = C(c, i);
    if (!cr->alive) {
      out->dead++;
    } else if (cr->frozen) {
      out->frozen++;
    } else {
      out->alive++;
      out->by_action[cr->action]++;
    }
  }
}

uint64_t zb_creatures_hash(const zb_creatures *c) {
  uint64_t h = zb_hash64(&c->rng, sizeof c->rng, 0x63726561);
  for (int i = 0; i < count(c); ++i) {
    const creature *cr = C(c, i);
    h = zb_hash64(&cr->id, sizeof cr->id, h);
    h = zb_hash64(&cr->pos, sizeof cr->pos, h);
    h = zb_hash64(&cr->heading, sizeof cr->heading, h);
    h = zb_hash64(&cr->health, sizeof cr->health, h);
    h = zb_hash64(&cr->action, sizeof cr->action, h);
  }
  return h;
}

void zb_creatures_each(const zb_creatures *c, zb_creature_fn fn, void *ctx) {
  for (int i = 0; i < count(c); ++i) {
    zb_creature_state s;
    state_of(C(c, i), &s);
    fn(ctx, &s, C(c, i)->phase);
  }
}

const char *zb_action_name(zb_action a) {
  static const char *names[ZB_ACTIONS] = {"wander",      "graze", "rest", "flee",  "hunt",
                                          "investigate", "drink", "feed", "alert", "follow"};
  return a >= 0 && a < ZB_ACTIONS ? names[a] : "?";
}

const char *zb_harm_name(zb_harm h) {
  static const char *names[ZB_HARMS] = {"heat", "impact", "cold", "poison", "smoke", "magic"};
  return h >= 0 && h < ZB_HARMS ? names[h] : "?";
}
