/* SPDX-License-Identifier: MIT */
/* Rigid bodies and projectiles in the active tier (concept 5.10).
 *
 * Bodies are spheres, capsules and boxes of a registry material (its density
 * gives the mass, its friction the grip). They fall towards the planet's
 * centre, collide with the 1 m heightfield and with each other, and are
 * resolved with sequential impulses (restitution, friction, rolling
 * resistance) at a fixed 60 Hz. Pairs are found on a 4 m grid. Bodies that
 * have come to rest sleep until something touches them, pushes them or
 * changes the ground or water under them; bodies outside the active tier are
 * frozen where they are.
 *
 * Water holds bodies up by the volume under its surface (wood floats, stone
 * sinks), slows them and carries them along with its flow.
 *
 * Projectiles (thrown stones, arrows, bolts) are small spheres that fly under
 * gravity and drag. Each step is a swept test against the ground and the
 * bodies, so a fast projectile never passes through anything; a hit stops it
 * and hands its momentum to the body it struck.
 *
 * Everything runs in doubles on planet-centred coordinates and in a fixed
 * order, so peers and replays agree to the bit. */
#ifndef ZB_SIM_BODIES_H
#define ZB_SIM_BODIES_H

#include "sim/material.h"
#include "sim/water.h"

#define ZB_BODIES_HZ 60    /* steps per second */
#define ZB_BODIES_MAX 2048 /* bodies at a time */
#define ZB_PROJECTILES_MAX 256

typedef enum zb_shape { ZB_SHAPE_SPHERE, ZB_SHAPE_CAPSULE, ZB_SHAPE_BOX, ZB_SHAPES } zb_shape;

typedef struct zb_body_desc {
  zb_shape shape;
  double radius;      /* sphere and capsule, metres */
  double half_length; /* capsule: half the length of its straight part */
  double half[3];     /* box: half its size along its own x, y, z */
  int material;       /* a registry entry (solid or powder) */
  zb_planet_pos pos;  /* the centre (alt: metres above sea level) */
  /* At the start, m/s and rad/s towards east, north and up. The body's own
   * x, y and z axes point east, north and up (a capsule stands upright). */
  double velocity[3];
  double spin[3];
  /* Optional: the body's own x and z axes (east, north, up), e.g. to restore
   * a saved body; both zero: upright. */
  double axis_x[3], axis_z[3];
  bool asleep; /* starts at rest */
} zb_body_desc;

typedef struct zb_body_state {
  zb_shape shape;
  int material;
  double radius, half_length, half[3]; /* as described */
  zb_planet_pos pos;
  double velocity[3]; /* m/s towards east, north, up */
  double spin[3];     /* rad/s about east, north, up */
  double speed;
  double mass;      /* kg */
  double submerged; /* the share of its volume under water, 0..1 */
  bool asleep;
  bool frozen;   /* outside the active tier */
  bool grounded; /* touched the ground in the last step */
  /* The body's own axes in the east-north-up frame at its centre. */
  double axis_x[3], axis_y[3], axis_z[3];
} zb_body_state;

typedef struct zb_bodies_stats {
  int count, awake, frozen;
  int contacts;    /* in the last step */
  int projectiles; /* still flying */
  double kinetic;  /* J in the awake bodies */
} zb_bodies_stats;

/* What a sweep or a projectile hit: the ground (body 0) or a body. */
typedef struct zb_hit {
  bool hit;
  uint32_t body;
  zb_planet_pos pos; /* the projectile's centre at the moment of contact */
  double normal[3];  /* east, north, up: the surface it hit, outwards */
  double distance;   /* metres travelled along the sweep */
} zb_hit;

typedef struct zb_projectile_state {
  bool exists, flying;
  zb_planet_pos pos;
  double velocity[3]; /* east, north, up */
  zb_hit hit;         /* once it has stopped */
} zb_projectile_state;

/* For drawing: a body's place and orientation in planet-centred metres. */
typedef struct zb_body_pose {
  uint32_t id;
  zb_shape shape;
  int material;
  double radius, half_length, half[3];
  zb_dvec3 centre;
  zb_dvec3 axis[3]; /* the body's own x, y, z */
  bool asleep;
} zb_body_pose;

typedef struct zb_bodies zb_bodies;

/* water may be NULL: then the ground's own stage decides what is active and
 * nothing floats. */
zb_bodies *zb_bodies_create(const zb_allocator *a, zb_world *world, const zb_materials *materials,
                            const zb_surface_water *water);
void zb_bodies_destroy(zb_bodies *b);
void zb_bodies_set_world(zb_bodies *b, zb_world *world, const zb_surface_water *water);

/* A new body (an id, never 0), or 0 when the description is not valid (a
 * size of 1 cm to 50 m, a solid or powder material) or there are too many. */
uint32_t zb_bodies_add(zb_bodies *b, const zb_body_desc *d);
bool zb_bodies_remove(zb_bodies *b, uint32_t id);
void zb_bodies_clear(zb_bodies *b);
bool zb_bodies_get(const zb_bodies *b, uint32_t id, zb_body_state *out);
/* Gives a body an impulse at its centre (N s towards east, north, up) and
 * wakes it. */
bool zb_bodies_push(zb_bodies *b, uint32_t id, const double impulse[3]);

/* Runs the steps that fit into seconds (60 Hz; the remainder is kept). */
void zb_bodies_update(zb_bodies *b, double seconds);
/* The ground of a chunk changed: the bodies on it wake. */
void zb_bodies_ground_changed(zb_bodies *b, zb_chunk c);

/* Throws a projectile (radius 1 mm .. 0.5 m, mass 1 g .. 100 kg) from pos at
 * velocity (m/s east, north, up). Returns its id, or 0. Projectiles are
 * forgotten 10 s after they stop. */
uint32_t zb_bodies_launch(zb_bodies *b, const zb_planet_pos *pos, const double velocity[3], double radius, double mass);
bool zb_bodies_projectile(const zb_bodies *b, uint32_t id, zb_projectile_state *out);
/* Sweeps a sphere of radius (0 for a ray) from one place to another and
 * reports the first thing it touches. */
zb_hit zb_bodies_sweep(const zb_bodies *b, const zb_planet_pos *from, const zb_planet_pos *to, double radius);

void zb_bodies_get_stats(const zb_bodies *b, zb_bodies_stats *out);
uint64_t zb_bodies_hash(const zb_bodies *b);
/* Calls fn for every body in id order. */
typedef void (*zb_body_fn)(void *ctx, const zb_body_pose *pose);
void zb_bodies_each(const zb_bodies *b, zb_body_fn fn, void *ctx);
/* Ids of the bodies within radius metres of pos, in id order (up to max). */
int zb_bodies_near(const zb_bodies *b, const zb_planet_pos *pos, double radius, uint32_t *ids, int max);
const char *zb_shape_name(zb_shape s);

#endif
