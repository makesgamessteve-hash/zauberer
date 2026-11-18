/* SPDX-License-Identifier: MIT */
#include "sim/bodies.h"

#include "core/array.h"
#include "core/dmath.h"
#include "core/hash.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DT (1.0 / ZB_BODIES_HZ)
#define GRAVITY 9.81
#define WATER_DENSITY 1000.0
#define ITERATIONS 10
#define RESTITUTION 0.25
#define BOUNCE_SPEED 1.0 /* m/s: slower impacts do not bounce */
#define SLOP 0.005       /* metres of overlap left alone */
#define BAUMGARTE 0.2
#define MAX_BIAS 3.0     /* m/s of push-out */
#define ROLLING 0.1      /* rolling resistance: a ball stops on slopes gentler than this */
#define AIR_DAMPING 0.02 /* 1/s */
#define WATER_DRAG 2.0   /* 1/s when fully under water */
#define SLEEP_SPEED 0.08 /* m/s */
#define SLEEP_SPIN 0.1   /* rad/s */
#define SLEEP_AFTER 0.5  /* seconds at rest */
#define CELL 4.0         /* metres: the broadphase grid */
#define LARGE 8.0        /* metres: bodies bounded by more are checked against all */
#define PROJECTILE_DRAG 0.02
#define PROJECTILE_KEEP 10.0 /* seconds a stopped projectile is remembered */
#define MARCH 0.25           /* metres: the longest sweep step near the ground */
#define WET 0.005            /* metres of water that count */

typedef struct body {
  uint32_t id;
  zb_shape shape;
  int material;
  double radius, half_length, half[3];
  double mass, inv_mass, inv_inertia[3]; /* along the body's own axes */
  double density, friction, bound;
  zb_dvec3 x, v, w; /* centre, velocity, angular velocity (planet-centred) */
  double q[4];      /* orientation: w, x, y, z (body to planet-centred) */
  double still, submerged;
  int water_check;
  bool asleep, frozen, grounded, touched;
  bool lively; /* moving at the start of the step: its touch wakes sleepers */
  zb_chunk chunk;
} body;

typedef struct projectile {
  uint32_t id;
  zb_dvec3 x, v;
  double radius, mass, stopped_for;
  bool flying, frozen;
  zb_hit hit;
} projectile;

typedef struct contact {
  int a, b; /* body indices; b < 0: the ground */
  zb_dvec3 p, n, ra, rb, t1, t2;
  double depth, friction, bias, target, kn, kt1, kt2, ln, lt1, lt2;
} contact;

typedef struct cell_entry {
  uint64_t key;
  int index;
} cell_entry;

typedef struct pair {
  int a, b;
} pair;

struct zb_bodies {
  const zb_allocator *a;
  zb_world *world;
  const zb_materials *materials;
  const zb_surface_water *water;
  zb_array bodies;      /* body, in id order */
  zb_array projectiles; /* projectile, in id order */
  zb_array contacts;    /* contact */
  zb_array cells;       /* cell_entry */
  zb_array pairs;       /* pair */
  uint32_t next_id, next_projectile;
  double carry;
  uint64_t steps;
  int last_contacts;
};

static body *B(const zb_bodies *b, int i) { return &((body *)b->bodies.data)[i]; }
static int body_count(const zb_bodies *b) { return (int)b->bodies.len; }
static const zb_planet *P(const zb_bodies *b) { return zb_world_planet(b->world); }

/* ---- Small vector and rotation helpers ---- */

static zb_dvec3 axis_of(const double q[4], int k) {
  double w = q[0], x = q[1], y = q[2], z = q[3];
  switch (k) {
    case 0: return zb_dv(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y));
    case 1: return zb_dv(2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x));
    default: return zb_dv(2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
  }
}

static void normalize_q(double q[4]) {
  double l = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  for (int k = 0; k < 4; ++k) {
    q[k] /= l;
  }
}

/* The rotation taking the planet-centred axes onto x, y and z (orthonormal). */
static void quat_from_axes(zb_dvec3 x, zb_dvec3 y, zb_dvec3 z, double q[4]) {
  double m00 = x.x, m11 = y.y, m22 = z.z, tr = m00 + m11 + m22;
  if (tr > 0.0) {
    double s = sqrt(tr + 1.0) * 2.0;
    q[0] = 0.25 * s;
    q[1] = (y.z - z.y) / s;
    q[2] = (z.x - x.z) / s;
    q[3] = (x.y - y.x) / s;
  } else if (m00 > m11 && m00 > m22) {
    double s = sqrt(1.0 + m00 - m11 - m22) * 2.0;
    q[0] = (y.z - z.y) / s;
    q[1] = 0.25 * s;
    q[2] = (y.x + x.y) / s;
    q[3] = (z.x + x.z) / s;
  } else if (m11 > m22) {
    double s = sqrt(1.0 + m11 - m00 - m22) * 2.0;
    q[0] = (z.x - x.z) / s;
    q[1] = (y.x + x.y) / s;
    q[2] = 0.25 * s;
    q[3] = (z.y + y.z) / s;
  } else {
    double s = sqrt(1.0 + m22 - m00 - m11) * 2.0;
    q[0] = (x.y - y.x) / s;
    q[1] = (z.x + x.z) / s;
    q[2] = (z.y + y.z) / s;
    q[3] = 0.25 * s;
  }
  normalize_q(q);
}

/* The inverse inertia in planet-centred axes times v. */
static zb_dvec3 inv_inertia_mul(const body *bd, zb_dvec3 v) {
  zb_dvec3 out = zb_dv(0, 0, 0);
  if (bd->asleep || bd->frozen) {
    return out;
  }
  for (int k = 0; k < 3; ++k) {
    zb_dvec3 ax = axis_of(bd->q, k);
    out = zb_dv_add(out, zb_dv_scale(ax, bd->inv_inertia[k] * zb_dv_dot(ax, v)));
  }
  return out;
}

static double inv_mass_of(const body *bd) { return bd->asleep || bd->frozen ? 0.0 : bd->inv_mass; }

static void frame_at(const zb_bodies *b, zb_dvec3 x, zb_dvec3 *e, zb_dvec3 *n, zb_dvec3 *u) {
  zb_planet_frame(zb_dv_norm(x), e, n, u);
  (void)b;
}

static zb_dvec3 from_enu(const zb_bodies *b, zb_dvec3 at, const double v[3]) {
  zb_dvec3 e, n, u;
  frame_at(b, at, &e, &n, &u);
  return zb_dv_add(zb_dv_add(zb_dv_scale(e, v[0]), zb_dv_scale(n, v[1])), zb_dv_scale(u, v[2]));
}

static void to_enu(const zb_bodies *b, zb_dvec3 at, zb_dvec3 v, double out[3]) {
  zb_dvec3 e, n, u;
  frame_at(b, at, &e, &n, &u);
  out[0] = zb_dv_dot(v, e);
  out[1] = zb_dv_dot(v, n);
  out[2] = zb_dv_dot(v, u);
}

/* ---- The ground ---- */

static const zb_chunk_terrain *loaded(const zb_bodies *b, zb_chunk c) {
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(b->world), c);
  return ct && ct->stage >= ZB_STAGE_TERRAIN ? ct : NULL;
}

/* The ground point below p (along the planet's radius). */
static bool ground_below(const zb_bodies *b, zb_dvec3 p, zb_planet_pos *pos, zb_dvec3 *g) {
  *pos = zb_planet_from_cart(P(b), p);
  if (!loaded(b, pos->chunk)) {
    return false;
  }
  zb_planet_pos gp = *pos;
  gp.alt = (float)zb_world_ground_height(b->world, pos);
  *g = zb_planet_to_cart(P(b), &gp);
  return true;
}

/* The ground's point below p and its outward normal. */
static bool ground_plane(const zb_bodies *b, zb_dvec3 p, zb_dvec3 *g, zb_dvec3 *n, zb_planet_pos *at) {
  zb_planet_pos pos;
  if (!ground_below(b, p, &pos, g)) {
    return false;
  }
  zb_planet_pos px = pos, py = pos;
  float dx = pos.lx < 255.0f ? 0.5f : -0.5f, dy = pos.ly < 255.0f ? 0.5f : -0.5f;
  px.lx += dx;
  py.ly += dy;
  px.alt = (float)zb_world_ground_height(b->world, &px);
  py.alt = (float)zb_world_ground_height(b->world, &py);
  zb_dvec3 gx = zb_dv_sub(zb_planet_to_cart(P(b), &px), *g), gy = zb_dv_sub(zb_planet_to_cart(P(b), &py), *g);
  zb_dvec3 c = zb_dv_cross(gx, gy);
  if (zb_dv_dot(c, p) < 0.0) {
    c = zb_dv_scale(c, -1.0);
  }
  *n = zb_dv_norm(c);
  if (at) {
    *at = pos;
  }
  return true;
}

static double ground_friction(const zb_bodies *b, const zb_planet_pos *pos) {
  const zb_chunk_terrain *ct = loaded(b, pos->chunk);
  int x = (int)(pos->lx * ZB_SURFACE_RES / 256.0f), y = (int)(pos->ly * ZB_SURFACE_RES / 256.0f);
  x = x < 0 ? 0 : (x >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : x);
  y = y < 0 ? 0 : (y >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : y);
  int m = ct && ct->material ? ct->material[y * ZB_SURFACE_RES + x] : ZB_MAT_ROCK;
  int entry = m > ZB_MAT_NONE && m < ZB_MAT_COUNT ? b->materials->surface[m] : -1;
  return entry >= 0 ? b->materials->info[entry].friction : 0.6;
}

/* ---- Creation ---- */

zb_bodies *zb_bodies_create(const zb_allocator *a, zb_world *world, const zb_materials *materials,
                            const zb_surface_water *water) {
  zb_bodies *b = zb_calloc(a, sizeof *b);
  if (!b) {
    return NULL;
  }
  b->a = a;
  b->world = world;
  b->materials = materials;
  b->water = water;
  zb_array_init(&b->bodies, a, sizeof(body));
  zb_array_init(&b->projectiles, a, sizeof(projectile));
  zb_array_init(&b->contacts, a, sizeof(contact));
  zb_array_init(&b->cells, a, sizeof(cell_entry));
  zb_array_init(&b->pairs, a, sizeof(pair));
  b->next_id = b->next_projectile = 1;
  return b;
}

void zb_bodies_destroy(zb_bodies *b) {
  if (!b) {
    return;
  }
  zb_array_free(&b->bodies);
  zb_array_free(&b->projectiles);
  zb_array_free(&b->contacts);
  zb_array_free(&b->cells);
  zb_array_free(&b->pairs);
  zb_free(b->a, b);
}

void zb_bodies_clear(zb_bodies *b) {
  zb_array_clear(&b->bodies);
  zb_array_clear(&b->projectiles);
  b->carry = 0.0;
}

void zb_bodies_set_world(zb_bodies *b, zb_world *world, const zb_surface_water *water) {
  b->world = world;
  b->water = water;
  zb_bodies_clear(b);
}

static bool size_ok(double v) { return v >= 0.01 && v <= 50.0; }

static bool mass_properties(body *bd, double density) {
  double r = bd->radius, h = bd->half_length, i[3];
  switch (bd->shape) {
    case ZB_SHAPE_SPHERE: {
      double m = density * 4.0 / 3.0 * ZB_DPI * r * r * r;
      bd->mass = m;
      i[0] = i[1] = i[2] = 0.4 * m * r * r;
      bd->bound = r;
      break;
    }
    case ZB_SHAPE_CAPSULE: {
      double mc = density * ZB_DPI * r * r * 2.0 * h, ms = density * 4.0 / 3.0 * ZB_DPI * r * r * r, len = 2.0 * h;
      bd->mass = mc + ms;
      i[2] = mc * r * r / 2.0 + 0.4 * ms * r * r;
      i[0] = i[1] = mc * (len * len / 12.0 + r * r / 4.0) + ms * (0.4 * r * r + len * len / 4.0 + 3.0 * len * r / 8.0);
      bd->bound = h + r;
      break;
    }
    default: {
      double x = bd->half[0], y = bd->half[1], z = bd->half[2];
      double m = density * 8.0 * x * y * z;
      bd->mass = m;
      i[0] = m / 3.0 * (y * y + z * z);
      i[1] = m / 3.0 * (x * x + z * z);
      i[2] = m / 3.0 * (x * x + y * y);
      bd->bound = sqrt(x * x + y * y + z * z);
      break;
    }
  }
  if (!(bd->mass > 0.0) || !isfinite(bd->mass)) {
    return false;
  }
  bd->inv_mass = 1.0 / bd->mass;
  for (int k = 0; k < 3; ++k) {
    bd->inv_inertia[k] = 1.0 / i[k];
  }
  return true;
}

static bool vec_zero(const double v[3]) { return v[0] == 0.0 && v[1] == 0.0 && v[2] == 0.0; }

uint32_t zb_bodies_add(zb_bodies *b, const zb_body_desc *d) {
  if (body_count(b) >= ZB_BODIES_MAX || d->shape < 0 || d->shape >= ZB_SHAPES || d->material < 0 ||
      d->material >= b->materials->count) {
    return 0;
  }
  const zb_material_info *mi = &b->materials->info[d->material];
  if (mi->phase != ZB_PHASE_SOLID && mi->phase != ZB_PHASE_POWDER) {
    return 0;
  }
  body bd;
  memset(&bd, 0, sizeof bd);
  bd.shape = d->shape;
  bd.material = d->material;
  bd.radius = d->radius;
  bd.half_length = d->half_length;
  memcpy(bd.half, d->half, sizeof bd.half);
  bool ok = d->shape == ZB_SHAPE_BOX ? size_ok(d->half[0]) && size_ok(d->half[1]) && size_ok(d->half[2])
                                     : size_ok(d->radius) && (d->shape == ZB_SHAPE_SPHERE || size_ok(d->half_length));
  if (!ok || !mass_properties(&bd, mi->density)) {
    return 0;
  }
  bd.density = mi->density;
  bd.friction = mi->friction;
  bd.x = zb_planet_to_cart(P(b), &d->pos);
  bd.v = from_enu(b, bd.x, d->velocity);
  bd.w = from_enu(b, bd.x, d->spin);
  zb_dvec3 e, n, u;
  frame_at(b, bd.x, &e, &n, &u);
  if (!vec_zero(d->axis_x) && !vec_zero(d->axis_z)) {
    zb_dvec3 ax = zb_dv_norm(from_enu(b, bd.x, d->axis_x)), az = from_enu(b, bd.x, d->axis_z);
    az = zb_dv_norm(zb_dv_sub(az, zb_dv_scale(ax, zb_dv_dot(az, ax))));
    quat_from_axes(ax, zb_dv_cross(az, ax), az, bd.q);
  } else {
    quat_from_axes(e, n, u, bd.q);
  }
  bd.asleep = d->asleep;
  bd.chunk = d->pos.chunk;
  bd.id = b->next_id++;
  return zb_array_push(&b->bodies, &bd) ? bd.id : 0;
}

static int find(const zb_bodies *b, uint32_t id) {
  int lo = 0, hi = body_count(b) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t m = B(b, mid)->id;
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

bool zb_bodies_remove(zb_bodies *b, uint32_t id) {
  int i = find(b, id);
  if (i < 0) {
    return false;
  }
  body *all = (body *)b->bodies.data;
  memmove(&all[i], &all[i + 1], sizeof(body) * (b->bodies.len - (size_t)i - 1));
  b->bodies.len--;
  return true;
}

static void wake(body *bd) {
  bd->asleep = false;
  bd->still = 0.0;
}

bool zb_bodies_push(zb_bodies *b, uint32_t id, const double impulse[3]) {
  int i = find(b, id);
  if (i < 0) {
    return false;
  }
  body *bd = B(b, i);
  wake(bd);
  bd->v = zb_dv_add(bd->v, zb_dv_scale(from_enu(b, bd->x, impulse), bd->inv_mass));
  return true;
}

bool zb_bodies_get(const zb_bodies *b, uint32_t id, zb_body_state *out) {
  int i = find(b, id);
  memset(out, 0, sizeof *out);
  if (i < 0) {
    return false;
  }
  const body *bd = B(b, i);
  out->shape = bd->shape;
  out->material = bd->material;
  out->radius = bd->radius;
  out->half_length = bd->half_length;
  memcpy(out->half, bd->half, sizeof out->half);
  out->pos = zb_planet_from_cart(P(b), bd->x);
  to_enu(b, bd->x, bd->v, out->velocity);
  to_enu(b, bd->x, bd->w, out->spin);
  out->speed = zb_dv_len(bd->v);
  out->mass = bd->mass;
  out->submerged = bd->submerged;
  out->asleep = bd->asleep;
  out->frozen = bd->frozen;
  out->grounded = bd->grounded;
  to_enu(b, bd->x, axis_of(bd->q, 0), out->axis_x);
  to_enu(b, bd->x, axis_of(bd->q, 1), out->axis_y);
  to_enu(b, bd->x, axis_of(bd->q, 2), out->axis_z);
  return true;
}

/* ---- Contacts ---- */

static void add_contact(zb_bodies *b, int ia, int ib, zb_dvec3 p, zb_dvec3 n, double depth) {
  contact c;
  memset(&c, 0, sizeof c);
  c.a = ia;
  c.b = ib;
  c.p = p;
  c.n = n;
  c.depth = depth;
  zb_array_push(&b->contacts, &c);
}

/* The ground under one point of a body, with the normal found at the body. */
static void ground_point(zb_bodies *b, int i, zb_dvec3 p, double r, zb_dvec3 n) {
  zb_planet_pos pos;
  zb_dvec3 g;
  if (!ground_below(b, p, &pos, &g)) {
    return;
  }
  double d = zb_dv_dot(zb_dv_sub(p, g), n);
  if (d < r) {
    add_contact(b, i, -1, zb_dv_sub(p, zb_dv_scale(n, r)), n, r - d);
  }
}

static void ground_contacts(zb_bodies *b, int i) {
  body *bd = B(b, i);
  zb_dvec3 g, n;
  zb_planet_pos at;
  size_t before = b->contacts.len;
  if (!ground_plane(b, bd->x, &g, &n, &at)) {
    return;
  }
  /* Far above the ground: nothing to find. */
  if (zb_dv_dot(zb_dv_sub(bd->x, g), n) > bd->bound + 0.5) {
    return;
  }
  switch (bd->shape) {
    case ZB_SHAPE_SPHERE: ground_point(b, i, bd->x, bd->radius, n); break;
    case ZB_SHAPE_CAPSULE: {
      zb_dvec3 az = zb_dv_scale(axis_of(bd->q, 2), bd->half_length);
      ground_point(b, i, zb_dv_add(bd->x, az), bd->radius, n);
      ground_point(b, i, zb_dv_sub(bd->x, az), bd->radius, n);
      break;
    }
    default: {
      zb_dvec3 ax[3];
      for (int k = 0; k < 3; ++k) {
        ax[k] = zb_dv_scale(axis_of(bd->q, k), bd->half[k]);
      }
      for (int c = 0; c < 8; ++c) {
        zb_dvec3 p = bd->x;
        for (int k = 0; k < 3; ++k) {
          p = zb_dv_add(p, zb_dv_scale(ax[k], (c >> k) & 1 ? 1.0 : -1.0));
        }
        ground_point(b, i, p, 0.0, n);
      }
      break;
    }
  }
  double mu = sqrt(bd->friction * ground_friction(b, &at));
  for (size_t k = before; k < b->contacts.len; ++k) {
    ((contact *)b->contacts.data)[k].friction = mu;
  }
}

/* The closest points of two segments [p1, q1] and [p2, q2]. */
static void closest_segments(zb_dvec3 p1, zb_dvec3 q1, zb_dvec3 p2, zb_dvec3 q2, zb_dvec3 *c1, zb_dvec3 *c2) {
  zb_dvec3 d1 = zb_dv_sub(q1, p1), d2 = zb_dv_sub(q2, p2), r = zb_dv_sub(p1, p2);
  double a = zb_dv_dot(d1, d1), e = zb_dv_dot(d2, d2), f = zb_dv_dot(d2, r), s = 0.0, t = 0.0;
  if (a <= 1e-12 && e <= 1e-12) {
    s = t = 0.0;
  } else if (a <= 1e-12) {
    t = fmin(fmax(f / e, 0.0), 1.0);
  } else {
    double c = zb_dv_dot(d1, r);
    if (e <= 1e-12) {
      s = fmin(fmax(-c / a, 0.0), 1.0);
    } else {
      double bb = zb_dv_dot(d1, d2), den = a * e - bb * bb;
      s = den > 1e-12 ? fmin(fmax((bb * f - c * e) / den, 0.0), 1.0) : 0.0;
      t = (bb * s + f) / e;
      if (t < 0.0) {
        t = 0.0;
        s = fmin(fmax(-c / a, 0.0), 1.0);
      } else if (t > 1.0) {
        t = 1.0;
        s = fmin(fmax((bb - c) / a, 0.0), 1.0);
      }
    }
  }
  *c1 = zb_dv_add(p1, zb_dv_scale(d1, s));
  *c2 = zb_dv_add(p2, zb_dv_scale(d2, t));
}

static zb_dvec3 closest_on_segment(zb_dvec3 p, zb_dvec3 s0, zb_dvec3 s1) {
  zb_dvec3 d = zb_dv_sub(s1, s0);
  double l = zb_dv_dot(d, d), t = l > 1e-12 ? zb_dv_dot(zb_dv_sub(p, s0), d) / l : 0.0;
  return zb_dv_add(s0, zb_dv_scale(d, fmin(fmax(t, 0.0), 1.0)));
}

/* A sphere or capsule as a segment and a radius. */
static void segment_of(const body *bd, zb_dvec3 *s0, zb_dvec3 *s1) {
  zb_dvec3 az = zb_dv_scale(axis_of(bd->q, 2), bd->shape == ZB_SHAPE_CAPSULE ? bd->half_length : 0.0);
  *s0 = zb_dv_add(bd->x, az);
  *s1 = zb_dv_sub(bd->x, az);
}

/* Spheres at p (body a, radius ra) and q (body b, radius rb). */
static bool sphere_pair(zb_bodies *b, int ia, int ib, zb_dvec3 p, double ra, zb_dvec3 q, double rb) {
  zb_dvec3 d = zb_dv_sub(p, q);
  double l = zb_dv_len(d);
  if (l >= ra + rb) {
    return false;
  }
  zb_dvec3 n = l > 1e-9 ? zb_dv_scale(d, 1.0 / l) : zb_dv_norm(B(b, ia)->x);
  add_contact(b, ia, ib, zb_dv_sub(p, zb_dv_scale(n, ra)), n, ra + rb - l);
  return true;
}

static void round_pair(zb_bodies *b, int ia, int ib) {
  const body *x = B(b, ia), *y = B(b, ib);
  zb_dvec3 a0, a1, b0, b1, c1, c2;
  segment_of(x, &a0, &a1);
  segment_of(y, &b0, &b1);
  closest_segments(a0, a1, b0, b1, &c1, &c2);
  sphere_pair(b, ia, ib, c1, x->radius, c2, y->radius);
  if (x->shape == ZB_SHAPE_CAPSULE && y->shape == ZB_SHAPE_CAPSULE &&
      fabs(zb_dv_dot(axis_of(x->q, 2), axis_of(y->q, 2))) > 0.95) {
    /* Side by side: also hold the ends, so they lie still. */
    zb_dvec3 ends[2] = {a0, a1};
    for (int k = 0; k < 2; ++k) {
      zb_dvec3 q = closest_on_segment(ends[k], b0, b1);
      if (zb_dv_len(zb_dv_sub(q, c2)) > 0.25 * y->radius) {
        sphere_pair(b, ia, ib, ends[k], x->radius, q, y->radius);
      }
    }
  }
}

/* A sphere at s (radius r, body is) against box ib. */
static void sphere_box(zb_bodies *b, int is, zb_dvec3 s, double r, int ib, bool flip) {
  const body *bx = B(b, ib);
  zb_dvec3 ax[3], rel = zb_dv_sub(s, bx->x);
  double l[3], c[3];
  bool inside = true;
  for (int k = 0; k < 3; ++k) {
    ax[k] = axis_of(bx->q, k);
    l[k] = zb_dv_dot(rel, ax[k]);
    c[k] = fmin(fmax(l[k], -bx->half[k]), bx->half[k]);
    inside = inside && c[k] == l[k];
  }
  zb_dvec3 n, p;
  double depth;
  if (inside) {
    int best = 0;
    for (int k = 1; k < 3; ++k) {
      best = bx->half[k] - fabs(l[k]) < bx->half[best] - fabs(l[best]) ? k : best;
    }
    n = zb_dv_scale(ax[best], l[best] < 0.0 ? -1.0 : 1.0);
    depth = r + bx->half[best] - fabs(l[best]);
    p = zb_dv_sub(s, zb_dv_scale(n, r));
  } else {
    zb_dvec3 cp = bx->x;
    for (int k = 0; k < 3; ++k) {
      cp = zb_dv_add(cp, zb_dv_scale(ax[k], c[k]));
    }
    zb_dvec3 d = zb_dv_sub(s, cp);
    double dist = zb_dv_len(d);
    if (dist >= r) {
      return;
    }
    n = zb_dv_scale(d, 1.0 / dist);
    depth = r - dist;
    p = cp;
  }
  if (flip) {
    add_contact(b, ib, is, p, zb_dv_scale(n, -1.0), depth);
  } else {
    add_contact(b, is, ib, p, n, depth);
  }
}

static void round_box(zb_bodies *b, int ir, int ib, bool flip) {
  const body *r = B(b, ir), *bx = B(b, ib);
  zb_dvec3 s0, s1;
  segment_of(r, &s0, &s1);
  sphere_box(b, ir, s0, r->radius, ib, flip);
  if (r->shape == ZB_SHAPE_CAPSULE) {
    sphere_box(b, ir, s1, r->radius, ib, flip);
    zb_dvec3 mid = closest_on_segment(bx->x, s0, s1);
    if (zb_dv_len(zb_dv_sub(mid, s0)) > r->radius && zb_dv_len(zb_dv_sub(mid, s1)) > r->radius) {
      sphere_box(b, ir, mid, r->radius, ib, flip);
    }
  }
}

/* Overlap of two boxes along a unit axis (separating axis test). */
static double overlap_on(const body *x, const body *y, zb_dvec3 axis) {
  double rx = 0.0, ry = 0.0;
  for (int k = 0; k < 3; ++k) {
    rx += x->half[k] * fabs(zb_dv_dot(axis_of(x->q, k), axis));
    ry += y->half[k] * fabs(zb_dv_dot(axis_of(y->q, k), axis));
  }
  return rx + ry - fabs(zb_dv_dot(zb_dv_sub(x->x, y->x), axis));
}

/* The corners of box inc below the face of box ref that faces along n (and
 * within its rectangle) become contacts. */
static int clip_corners(zb_bodies *b, int iinc, int iref, int face, zb_dvec3 n) {
  const body *inc = B(b, iinc), *ref = B(b, iref);
  double extent = ref->half[face];
  zb_dvec3 u[2] = {axis_of(ref->q, (face + 1) % 3), axis_of(ref->q, (face + 2) % 3)};
  double hu[2] = {ref->half[(face + 1) % 3], ref->half[(face + 2) % 3]};
  zb_dvec3 ax[3];
  for (int k = 0; k < 3; ++k) {
    ax[k] = zb_dv_scale(axis_of(inc->q, k), inc->half[k]);
  }
  int found = 0;
  for (int c = 0; c < 8; ++c) {
    zb_dvec3 p = inc->x;
    for (int k = 0; k < 3; ++k) {
      p = zb_dv_add(p, zb_dv_scale(ax[k], (c >> k) & 1 ? 1.0 : -1.0));
    }
    zb_dvec3 rel = zb_dv_sub(p, ref->x);
    double s = zb_dv_dot(rel, n) - extent;
    if (s < 0.0 && fabs(zb_dv_dot(rel, u[0])) <= hu[0] + 0.02 && fabs(zb_dv_dot(rel, u[1])) <= hu[1] + 0.02) {
      add_contact(b, iinc, iref, p, n, -s);
      found++;
    }
  }
  return found;
}

static void box_pair(zb_bodies *b, int ia, int ib) {
  const body *x = B(b, ia), *y = B(b, ib);
  zb_dvec3 best_axis = zb_dv(0, 0, 0);
  double best = INFINITY;
  int kind = -1;
  for (int i = 0; i < 15; ++i) {
    zb_dvec3 axis;
    if (i < 3) {
      axis = axis_of(x->q, i);
    } else if (i < 6) {
      axis = axis_of(y->q, i - 3);
    } else {
      axis = zb_dv_cross(axis_of(x->q, (i - 6) / 3), axis_of(y->q, (i - 6) % 3));
      double len = zb_dv_len(axis);
      if (len < 1e-3) {
        continue; /* parallel edges: their faces decide */
      }
      axis = zb_dv_scale(axis, 1.0 / len);
    }
    double o = overlap_on(x, y, axis);
    if (o <= 0.0) {
      return;
    }
    /* Faces win over edges unless an edge is clearly shallower. */
    if (i < 6 ? o < best : o < 0.95 * best) {
      best = o;
      best_axis = axis;
      kind = i;
    }
  }
  if (zb_dv_dot(best_axis, zb_dv_sub(x->x, y->x)) < 0.0) {
    best_axis = zb_dv_scale(best_axis, -1.0); /* from y towards x */
  }
  int found = 0;
  if (kind >= 3 && kind < 6) {
    found = clip_corners(b, ia, ib, kind - 3, best_axis); /* x rests on a face of y */
  } else if (kind < 3) {
    found = clip_corners(b, ib, ia, kind, zb_dv_scale(best_axis, -1.0));
  }
  if (found == 0) {
    /* Edge against edge: one contact between the centres. */
    add_contact(b, ia, ib, zb_dv_scale(zb_dv_add(x->x, y->x), 0.5), best_axis, best);
  }
}

static void narrow(zb_bodies *b, int ia, int ib) {
  const body *x = B(b, ia), *y = B(b, ib);
  if (zb_dv_len(zb_dv_sub(x->x, y->x)) >= x->bound + y->bound) {
    return;
  }
  bool bx = x->shape == ZB_SHAPE_BOX, by = y->shape == ZB_SHAPE_BOX;
  if (bx && by) {
    box_pair(b, ia, ib);
  } else if (bx) {
    round_box(b, ib, ia, true);
  } else if (by) {
    round_box(b, ia, ib, false);
  } else {
    round_pair(b, ia, ib);
  }
}

/* ---- The broadphase grid ---- */

static int64_t cell_of(double v) { return (int64_t)floor(v / CELL); }

static uint64_t cell_key(int64_t x, int64_t y, int64_t z) {
  int64_t k[3] = {x, y, z};
  return zb_hash64(k, sizeof k, 0x63656c6c);
}

static int by_key(const void *pa, const void *pb) {
  const cell_entry *a = pa, *b = pb;
  if (a->key != b->key) {
    return a->key < b->key ? -1 : 1;
  }
  return (a->index > b->index) - (a->index < b->index);
}

static int by_pair(const void *pa, const void *pb) {
  const pair *a = pa, *b = pb;
  if (a->a != b->a) {
    return a->a < b->a ? -1 : 1;
  }
  return (a->b > b->b) - (a->b < b->b);
}

static bool active_pair(const body *x, const body *y) {
  if (x->frozen && y->frozen) {
    return false;
  }
  return !(x->asleep || x->frozen) || !(y->asleep || y->frozen);
}

static void push_pair(zb_bodies *b, int i, int j) {
  pair p = {i < j ? i : j, i < j ? j : i};
  if (i != j && active_pair(B(b, i), B(b, j))) {
    zb_array_push(&b->pairs, &p);
  }
}

static void find_pairs(zb_bodies *b) {
  zb_array_clear(&b->cells);
  zb_array_clear(&b->pairs);
  int n = body_count(b);
  for (int i = 0; i < n; ++i) {
    const body *bd = B(b, i);
    if (bd->bound > LARGE) {
      for (int j = 0; j < n; ++j) {
        push_pair(b, i, j);
      }
      continue;
    }
    int64_t lo[3], hi[3];
    double c[3] = {bd->x.x, bd->x.y, bd->x.z};
    for (int k = 0; k < 3; ++k) {
      lo[k] = cell_of(c[k] - bd->bound);
      hi[k] = cell_of(c[k] + bd->bound);
    }
    for (int64_t x = lo[0]; x <= hi[0]; ++x) {
      for (int64_t y = lo[1]; y <= hi[1]; ++y) {
        for (int64_t z = lo[2]; z <= hi[2]; ++z) {
          cell_entry e = {cell_key(x, y, z), i};
          zb_array_push(&b->cells, &e);
        }
      }
    }
  }
  cell_entry *cells = (cell_entry *)b->cells.data;
  if (b->cells.len > 1) {
    qsort(cells, b->cells.len, sizeof *cells, by_key);
  }
  for (size_t s = 0; s < b->cells.len;) {
    size_t e = s + 1;
    while (e < b->cells.len && cells[e].key == cells[s].key) {
      e++;
    }
    for (size_t i = s; i < e; ++i) {
      for (size_t j = i + 1; j < e; ++j) {
        push_pair(b, cells[i].index, cells[j].index);
      }
    }
    s = e;
  }
  pair *pairs = (pair *)b->pairs.data;
  if (b->pairs.len > 1) {
    qsort(pairs, b->pairs.len, sizeof *pairs, by_pair);
  }
  size_t out = 0;
  for (size_t i = 0; i < b->pairs.len; ++i) {
    if (out == 0 || pairs[i].a != pairs[out - 1].a || pairs[i].b != pairs[out - 1].b) {
      pairs[out++] = pairs[i];
    }
  }
  b->pairs.len = out;
}

/* ---- The solver ---- */

static zb_dvec3 point_velocity(const body *bd, zb_dvec3 r) {
  if (bd->asleep || bd->frozen) {
    return zb_dv(0, 0, 0);
  }
  return zb_dv_add(bd->v, zb_dv_cross(bd->w, r));
}

static double effective(const body *x, const body *y, zb_dvec3 ra, zb_dvec3 rb, zb_dvec3 n) {
  double k = inv_mass_of(x) + zb_dv_dot(n, zb_dv_cross(inv_inertia_mul(x, zb_dv_cross(ra, n)), ra));
  if (y) {
    k += inv_mass_of(y) + zb_dv_dot(n, zb_dv_cross(inv_inertia_mul(y, zb_dv_cross(rb, n)), rb));
  }
  return k;
}

static void apply(body *x, body *y, zb_dvec3 ra, zb_dvec3 rb, zb_dvec3 imp) {
  if (!x->asleep && !x->frozen) {
    x->v = zb_dv_add(x->v, zb_dv_scale(imp, x->inv_mass));
    x->w = zb_dv_add(x->w, inv_inertia_mul(x, zb_dv_cross(ra, imp)));
  }
  if (y && !y->asleep && !y->frozen) {
    y->v = zb_dv_sub(y->v, zb_dv_scale(imp, y->inv_mass));
    y->w = zb_dv_sub(y->w, inv_inertia_mul(y, zb_dv_cross(rb, imp)));
  }
}

static zb_dvec3 relative_velocity(const body *x, const body *y, const contact *c) {
  zb_dvec3 v = point_velocity(x, c->ra);
  return y ? zb_dv_sub(v, point_velocity(y, c->rb)) : v;
}

static void prepare(zb_bodies *b, contact *c) {
  body *x = B(b, c->a), *y = c->b >= 0 ? B(b, c->b) : NULL;
  c->ra = zb_dv_sub(c->p, x->x);
  c->rb = y ? zb_dv_sub(c->p, y->x) : zb_dv(0, 0, 0);
  if (y) {
    c->friction = sqrt(x->friction * y->friction);
  }
  zb_dvec3 vr = relative_velocity(x, y, c);
  double vn = zb_dv_dot(vr, c->n);
  c->target = vn < -BOUNCE_SPEED ? -RESTITUTION * vn : 0.0;
  c->bias = fmin(BAUMGARTE / DT * fmax(c->depth - SLOP, 0.0), MAX_BIAS);
  zb_dvec3 vt = zb_dv_sub(vr, zb_dv_scale(c->n, vn));
  double lt = zb_dv_len(vt);
  if (lt > 1e-6) {
    c->t1 = zb_dv_scale(vt, 1.0 / lt);
  } else {
    zb_dvec3 any = fabs(c->n.x) < 0.6 ? zb_dv(1, 0, 0) : zb_dv(0, 1, 0);
    c->t1 = zb_dv_norm(zb_dv_cross(c->n, any));
  }
  c->t2 = zb_dv_cross(c->n, c->t1);
  c->kn = effective(x, y, c->ra, c->rb, c->n);
  c->kt1 = effective(x, y, c->ra, c->rb, c->t1);
  c->kt2 = effective(x, y, c->ra, c->rb, c->t2);
  c->ln = c->lt1 = c->lt2 = 0.0;
}

static void solve(zb_bodies *b, contact *c) {
  body *x = B(b, c->a), *y = c->b >= 0 ? B(b, c->b) : NULL;
  if (c->kn <= 0.0) {
    return;
  }
  double vn = zb_dv_dot(relative_velocity(x, y, c), c->n);
  double l = fmax(c->ln + (fmax(c->target, c->bias) - vn) / c->kn, 0.0);
  apply(x, y, c->ra, c->rb, zb_dv_scale(c->n, l - c->ln));
  c->ln = l;
  double limit = c->friction * c->ln;
  zb_dvec3 t[2] = {c->t1, c->t2};
  double *acc[2] = {&c->lt1, &c->lt2}, k[2] = {c->kt1, c->kt2};
  for (int i = 0; i < 2; ++i) {
    double vt = zb_dv_dot(relative_velocity(x, y, c), t[i]);
    double nl = fmin(fmax(*acc[i] - vt / k[i], -limit), limit);
    apply(x, y, c->ra, c->rb, zb_dv_scale(t[i], nl - *acc[i]));
    *acc[i] = nl;
  }
}

/* ---- Water ---- */

static double sphere_cap_share(double h, double r) {
  h = fmin(fmax(h, 0.0), 2.0 * r);
  return h * h * (3.0 * r - h) / (4.0 * r * r * r);
}

/* The body's lowest and highest point along up, from its centre. */
static double vertical_extent(const body *bd, zb_dvec3 up) {
  switch (bd->shape) {
    case ZB_SHAPE_SPHERE: return bd->radius;
    case ZB_SHAPE_CAPSULE: return fabs(zb_dv_dot(axis_of(bd->q, 2), up)) * bd->half_length + bd->radius;
    default: {
      double e = 0.0;
      for (int k = 0; k < 3; ++k) {
        e += bd->half[k] * fabs(zb_dv_dot(axis_of(bd->q, k), up));
      }
      return e;
    }
  }
}

/* The water at a body: the share of it under water and the flow (m/s). */
static double water_at(const zb_bodies *b, const body *bd, const zb_planet_pos *pos, zb_dvec3 *flow) {
  *flow = zb_dv(0, 0, 0);
  if (!b->water) {
    return 0.0;
  }
  zb_water_cell c;
  zb_surface_water_cell_at(b->water, pos, &c);
  if (!c.loaded || c.depth <= WET) {
    return 0.0;
  }
  zb_dvec3 up = zb_dv_norm(bd->x);
  double e = vertical_extent(bd, up), below = c.level - (pos->alt - e);
  if (below <= 0.0) {
    return 0.0;
  }
  if (c.vx != 0.0 || c.vy != 0.0) {
    zb_planet_pos px = *pos, py = *pos;
    px.lx += 1.0f;
    py.ly += 1.0f;
    zb_dvec3 o = zb_planet_to_cart(P(b), pos);
    zb_dvec3 ax = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(P(b), &px), o));
    zb_dvec3 ay = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(P(b), &py), o));
    *flow = zb_dv_add(zb_dv_scale(ax, c.vx), zb_dv_scale(ay, c.vy));
  }
  return bd->shape == ZB_SHAPE_SPHERE ? sphere_cap_share(below, bd->radius) : fmin(below / (2.0 * e), 1.0);
}

/* Whether a body is in the active tier. */
static bool active(const zb_bodies *b, const zb_planet_pos *pos) {
  if (!loaded(b, pos->chunk)) {
    return false;
  }
  if (!b->water) {
    return true;
  }
  zb_water_cell c;
  zb_surface_water_cell_at(b->water, pos, &c);
  return c.loaded;
}

/* ---- Stepping ---- */

static void forces(zb_bodies *b, body *bd, const zb_planet_pos *pos) {
  zb_dvec3 up = zb_dv_norm(bd->x), flow;
  double share = water_at(b, bd, pos, &flow);
  bd->submerged = share;
  double lift = GRAVITY * (WATER_DENSITY / bd->density) * share;
  bd->v = zb_dv_add(bd->v, zb_dv_scale(up, (lift - GRAVITY) * DT));
  if (share > 0.0) {
    double k = fmin(WATER_DRAG * share * DT, 1.0);
    bd->v = zb_dv_add(bd->v, zb_dv_scale(zb_dv_sub(flow, bd->v), k));
    bd->w = zb_dv_scale(bd->w, 1.0 - k);
  }
  double air = 1.0 / (1.0 + AIR_DAMPING * DT);
  bd->v = zb_dv_scale(bd->v, air);
  bd->w = zb_dv_scale(bd->w, air);
}

static void integrate(body *bd) {
  bd->x = zb_dv_add(bd->x, zb_dv_scale(bd->v, DT));
  double *q = bd->q, h = 0.5 * DT;
  double wx = bd->w.x, wy = bd->w.y, wz = bd->w.z;
  double dq[4] = {-wx * q[1] - wy * q[2] - wz * q[3], wx * q[0] + wy * q[3] - wz * q[2],
                  wy * q[0] + wz * q[1] - wx * q[3], wz * q[0] + wx * q[2] - wy * q[1]};
  for (int k = 0; k < 4; ++k) {
    q[k] += h * dq[k];
  }
  normalize_q(q);
}

static void rest(body *bd) {
  bool slow = zb_dv_dot(bd->v, bd->v) < SLEEP_SPEED * SLEEP_SPEED && zb_dv_dot(bd->w, bd->w) < SLEEP_SPIN * SLEEP_SPIN;
  if (slow && (bd->touched || bd->submerged > 0.0)) {
    bd->still += DT;
    if (bd->still >= SLEEP_AFTER) {
      bd->asleep = true;
      bd->v = bd->w = zb_dv(0, 0, 0);
    }
  } else {
    bd->still = 0.0;
  }
}

/* Sleeping bodies wake when the water around them changes. */
static void check_sleeper(zb_bodies *b, body *bd, const zb_planet_pos *pos) {
  if (++bd->water_check < ZB_BODIES_HZ / 2) {
    return;
  }
  bd->water_check = 0;
  zb_dvec3 flow;
  double share = water_at(b, bd, pos, &flow);
  if (fabs(share - bd->submerged) > 0.02 || zb_dv_len(flow) > 0.3) {
    wake(bd);
  }
}

static void step_projectiles(zb_bodies *b);

static bool moving(const body *bd) {
  return !bd->asleep && !bd->frozen &&
         (zb_dv_dot(bd->v, bd->v) > 4.0 * SLEEP_SPEED * SLEEP_SPEED ||
          zb_dv_dot(bd->w, bd->w) > 4.0 * SLEEP_SPIN * SLEEP_SPIN);
}

static void step(zb_bodies *b) {
  int n = body_count(b);
  zb_array_clear(&b->contacts);
  for (int i = 0; i < n; ++i) {
    body *bd = B(b, i);
    zb_planet_pos pos = zb_planet_from_cart(P(b), bd->x);
    bd->chunk = pos.chunk;
    bd->frozen = !active(b, &pos);
    bd->grounded = bd->touched = false;
    bd->lively = moving(bd);
    if (bd->frozen) {
      continue;
    }
    if (bd->asleep) {
      check_sleeper(b, bd, &pos);
    }
    if (!bd->asleep) {
      forces(b, bd, &pos);
      ground_contacts(b, i);
    }
  }
  find_pairs(b);
  const pair *pairs = (pair *)b->pairs.data;
  for (size_t k = 0; k < b->pairs.len; ++k) {
    size_t before = b->contacts.len;
    narrow(b, pairs[k].a, pairs[k].b);
    if (b->contacts.len > before) {
      /* A real knock wakes a sleeper; something settling on it does not. */
      body *x = B(b, pairs[k].a), *y = B(b, pairs[k].b);
      if (x->asleep && y->lively) {
        wake(x);
      }
      if (y->asleep && x->lively) {
        wake(y);
      }
    }
  }
  contact *cs = (contact *)b->contacts.data;
  size_t nc = b->contacts.len;
  for (size_t k = 0; k < nc; ++k) {
    prepare(b, &cs[k]);
    B(b, cs[k].a)->touched = true;
    B(b, cs[k].a)->grounded |= cs[k].b < 0;
    if (cs[k].b >= 0) {
      B(b, cs[k].b)->touched = true;
    }
  }
  for (int it = 0; it < ITERATIONS; ++it) {
    for (size_t k = 0; k < nc; ++k) {
      solve(b, &cs[k]);
    }
  }
  b->last_contacts = (int)nc;
  for (int i = 0; i < n; ++i) {
    body *bd = B(b, i);
    if (bd->frozen || bd->asleep) {
      continue;
    }
    if (bd->grounded && bd->shape != ZB_SHAPE_BOX) {
      /* Rolling resistance: the spin slows by a fixed amount each step. */
      double spin = zb_dv_len(bd->w), slow = ROLLING * GRAVITY / bd->radius * DT;
      bd->w = spin > slow ? zb_dv_scale(bd->w, 1.0 - slow / spin) : zb_dv(0, 0, 0);
    }
    integrate(bd);
    rest(bd);
  }
  step_projectiles(b);
  b->steps++;
}

void zb_bodies_update(zb_bodies *b, double seconds) {
  b->carry += seconds;
  for (int k = 0; k < 10 && b->carry >= DT - 1e-9; ++k) {
    step(b);
    b->carry -= DT;
  }
  if (b->carry > DT) {
    b->carry = 0.0; /* too far behind: drop the rest */
  }
}

void zb_bodies_ground_changed(zb_bodies *b, zb_chunk c) {
  for (int i = 0; i < body_count(b); ++i) {
    body *bd = B(b, i);
    if (bd->chunk.face == c.face && bd->chunk.x == c.x && bd->chunk.y == c.y) {
      wake(bd);
    }
  }
}

/* ---- Sweeps and projectiles ---- */

/* The distance from p to a body's surface (negative inside) and its gradient. */
static double body_distance(const body *bd, zb_dvec3 p, zb_dvec3 *grad) {
  if (bd->shape != ZB_SHAPE_BOX) {
    zb_dvec3 s0, s1;
    segment_of(bd, &s0, &s1);
    zb_dvec3 d = zb_dv_sub(p, closest_on_segment(p, s0, s1));
    double l = zb_dv_len(d);
    *grad = l > 1e-9 ? zb_dv_scale(d, 1.0 / l) : zb_dv_norm(bd->x);
    return l - bd->radius;
  }
  zb_dvec3 rel = zb_dv_sub(p, bd->x), out = zb_dv(0, 0, 0), ax[3];
  double inside = -INFINITY, outside2 = 0.0;
  int best = 0;
  for (int k = 0; k < 3; ++k) {
    ax[k] = axis_of(bd->q, k);
    double l = zb_dv_dot(rel, ax[k]), d = fabs(l) - bd->half[k];
    if (d > 0.0) {
      out = zb_dv_add(out, zb_dv_scale(ax[k], l < 0.0 ? -d : d));
      outside2 += d * d;
    }
    if (d > inside) {
      inside = d;
      best = k;
    }
  }
  if (outside2 > 0.0) {
    double l = sqrt(outside2);
    *grad = zb_dv_scale(out, 1.0 / l);
    return l;
  }
  *grad = zb_dv_scale(ax[best], zb_dv_dot(rel, ax[best]) < 0.0 ? -1.0 : 1.0);
  return inside;
}

typedef struct sweep_probe {
  double ground; /* clearance to the ground's plane (INFINITY: no ground) */
  double body;   /* clearance to the nearest body */
  int nearest;
  zb_dvec3 ground_n, body_n;
} sweep_probe;

static sweep_probe probe(const zb_bodies *b, zb_dvec3 p, double r, const int *cand, int nc) {
  sweep_probe s = {INFINITY, INFINITY, -1, zb_dv(0, 0, 0), zb_dv(0, 0, 0)};
  zb_dvec3 g, n;
  if (ground_plane(b, p, &g, &n, NULL)) {
    s.ground = zb_dv_dot(zb_dv_sub(p, g), n) - r;
    s.ground_n = n;
  }
  for (int k = 0; k < nc; ++k) {
    zb_dvec3 grad;
    double d = body_distance(B(b, cand[k]), p, &grad) - r;
    if (d < s.body) {
      s.body = d;
      s.nearest = cand[k];
      s.body_n = grad;
    }
  }
  return s;
}

static bool touching(const sweep_probe *s) { return s->ground <= 0.0 || s->body <= 0.0; }

static zb_hit sweep_cart(const zb_bodies *b, zb_dvec3 from, zb_dvec3 to, double r) {
  zb_hit h;
  memset(&h, 0, sizeof h);
  zb_dvec3 d = zb_dv_sub(to, from);
  double len = zb_dv_len(d);
  zb_dvec3 dir = len > 0.0 ? zb_dv_scale(d, 1.0 / len) : zb_dv(0, 0, 0);
  int cand[64], nc = 0;
  for (int i = 0; i < body_count(b) && nc < 64; ++i) {
    const body *bd = B(b, i);
    zb_dvec3 c = len > 0.0 ? closest_on_segment(bd->x, from, to) : from;
    if (zb_dv_len(zb_dv_sub(bd->x, c)) <= bd->bound + r) {
      cand[nc++] = i;
    }
  }
  double t = 0.0, prev = 0.0;
  sweep_probe s = probe(b, from, r, cand, nc);
  while (!touching(&s)) {
    if (t >= len) {
      return h;
    }
    double ground_step = s.ground == INFINITY ? len : fmax(MARCH, 0.5 * s.ground);
    double step = fmax(fmin(ground_step, s.body), 0.01);
    prev = t;
    t = fmin(t + step, len);
    s = probe(b, zb_dv_add(from, zb_dv_scale(dir, t)), r, cand, nc);
  }
  if (t > 0.0) { /* close in on the moment of contact */
    double lo = prev, hi = t;
    for (int k = 0; k < 16; ++k) {
      double mid = 0.5 * (lo + hi);
      sweep_probe m = probe(b, zb_dv_add(from, zb_dv_scale(dir, mid)), r, cand, nc);
      if (touching(&m)) {
        hi = mid;
        s = m;
      } else {
        lo = mid;
      }
    }
    t = hi;
  }
  zb_dvec3 at = zb_dv_add(from, zb_dv_scale(dir, t));
  h.hit = true;
  h.distance = t;
  h.pos = zb_planet_from_cart(P(b), at);
  bool on_body = s.body <= 0.0 && (s.ground > 0.0 || s.body <= s.ground);
  h.body = on_body ? B(b, s.nearest)->id : 0;
  to_enu(b, at, on_body ? s.body_n : s.ground_n, h.normal);
  return h;
}

zb_hit zb_bodies_sweep(const zb_bodies *b, const zb_planet_pos *from, const zb_planet_pos *to, double radius) {
  return sweep_cart(b, zb_planet_to_cart(P(b), from), zb_planet_to_cart(P(b), to), fmax(radius, 0.0));
}

static projectile *PR(const zb_bodies *b, int i) { return &((projectile *)b->projectiles.data)[i]; }

uint32_t zb_bodies_launch(zb_bodies *b, const zb_planet_pos *pos, const double velocity[3], double radius,
                          double mass) {
  if (b->projectiles.len >= ZB_PROJECTILES_MAX || !(radius >= 0.001 && radius <= 0.5) ||
      !(mass >= 0.001 && mass <= 100.0)) {
    return 0;
  }
  projectile p;
  memset(&p, 0, sizeof p);
  p.x = zb_planet_to_cart(P(b), pos);
  p.v = from_enu(b, p.x, velocity);
  p.radius = radius;
  p.mass = mass;
  p.flying = true;
  p.id = b->next_projectile++;
  return zb_array_push(&b->projectiles, &p) ? p.id : 0;
}

static void strike(zb_bodies *b, projectile *p, const zb_hit *h) {
  int i = find(b, h->body);
  if (i < 0) {
    return;
  }
  body *bd = B(b, i);
  wake(bd);
  zb_dvec3 n = from_enu(b, p->x, h->normal), at = zb_dv_sub(p->x, zb_dv_scale(n, p->radius));
  zb_dvec3 imp = zb_dv_scale(p->v, p->mass);
  bd->v = zb_dv_add(bd->v, zb_dv_scale(imp, bd->inv_mass));
  bd->w = zb_dv_add(bd->w, inv_inertia_mul(bd, zb_dv_cross(zb_dv_sub(at, bd->x), imp)));
}

static void step_projectiles(zb_bodies *b) {
  size_t out = 0;
  for (size_t k = 0; k < b->projectiles.len; ++k) {
    projectile *p = PR(b, (int)k);
    if (p->flying) {
      zb_planet_pos pos = zb_planet_from_cart(P(b), p->x);
      p->frozen = !active(b, &pos);
      if (!p->frozen) {
        zb_dvec3 up = zb_dv_norm(p->x);
        p->v = zb_dv_scale(zb_dv_add(p->v, zb_dv_scale(up, -GRAVITY * DT)), 1.0 / (1.0 + PROJECTILE_DRAG * DT));
        zb_dvec3 to = zb_dv_add(p->x, zb_dv_scale(p->v, DT));
        zb_hit h = sweep_cart(b, p->x, to, p->radius);
        if (h.hit) {
          p->x = zb_planet_to_cart(P(b), &h.pos);
          p->hit = h;
          strike(b, p, &h);
          p->flying = false;
          p->v = zb_dv(0, 0, 0);
        } else {
          p->x = to;
        }
      }
    } else {
      p->stopped_for += DT;
    }
    if (p->flying || p->stopped_for < PROJECTILE_KEEP) {
      *PR(b, (int)out++) = *p;
    }
  }
  b->projectiles.len = out;
}

bool zb_bodies_projectile(const zb_bodies *b, uint32_t id, zb_projectile_state *out) {
  memset(out, 0, sizeof *out);
  for (size_t k = 0; k < b->projectiles.len; ++k) {
    const projectile *p = PR(b, (int)k);
    if (p->id == id) {
      out->exists = true;
      out->flying = p->flying;
      out->pos = zb_planet_from_cart(P(b), p->x);
      to_enu(b, p->x, p->v, out->velocity);
      out->hit = p->hit;
      return true;
    }
  }
  return false;
}

/* ---- Queries ---- */

void zb_bodies_get_stats(const zb_bodies *b, zb_bodies_stats *out) {
  memset(out, 0, sizeof *out);
  out->count = body_count(b);
  for (int i = 0; i < out->count; ++i) {
    const body *bd = B(b, i);
    out->frozen += bd->frozen ? 1 : 0;
    if (!bd->frozen && !bd->asleep) {
      out->awake++;
      double rot = 0.0;
      for (int k = 0; k < 3; ++k) {
        double wk = zb_dv_dot(bd->w, axis_of(bd->q, k));
        rot += wk * wk / bd->inv_inertia[k];
      }
      out->kinetic += 0.5 * bd->mass * zb_dv_dot(bd->v, bd->v) + 0.5 * rot;
    }
  }
  out->contacts = b->last_contacts;
  for (size_t k = 0; k < b->projectiles.len; ++k) {
    out->projectiles += PR(b, (int)k)->flying ? 1 : 0;
  }
}

uint64_t zb_bodies_hash(const zb_bodies *b) {
  uint64_t h = zb_hash64(&b->steps, sizeof b->steps, 0x626f6479);
  for (int i = 0; i < body_count(b); ++i) {
    const body *bd = B(b, i);
    h = zb_hash64(&bd->id, sizeof bd->id, h);
    h = zb_hash64(&bd->x, sizeof bd->x, h);
    h = zb_hash64(&bd->v, sizeof bd->v, h);
    h = zb_hash64(&bd->w, sizeof bd->w, h);
    h = zb_hash64(bd->q, sizeof bd->q, h);
  }
  for (size_t k = 0; k < b->projectiles.len; ++k) {
    const projectile *p = PR(b, (int)k);
    h = zb_hash64(&p->x, sizeof p->x, h);
    h = zb_hash64(&p->v, sizeof p->v, h);
  }
  return h;
}

void zb_bodies_each(const zb_bodies *b, zb_body_fn fn, void *ctx) {
  for (int i = 0; i < body_count(b); ++i) {
    const body *bd = B(b, i);
    zb_body_pose p;
    p.id = bd->id;
    p.shape = bd->shape;
    p.material = bd->material;
    p.radius = bd->radius;
    p.half_length = bd->half_length;
    memcpy(p.half, bd->half, sizeof p.half);
    p.centre = bd->x;
    for (int k = 0; k < 3; ++k) {
      p.axis[k] = axis_of(bd->q, k);
    }
    p.asleep = bd->asleep;
    fn(ctx, &p);
  }
}

int zb_bodies_near(const zb_bodies *b, const zb_planet_pos *pos, double radius, uint32_t *ids, int max) {
  zb_dvec3 c = zb_planet_to_cart(P(b), pos);
  int n = 0;
  for (int i = 0; i < body_count(b) && n < max; ++i) {
    if (zb_dv_len(zb_dv_sub(B(b, i)->x, c)) <= radius) {
      ids[n++] = B(b, i)->id;
    }
  }
  return n;
}

const char *zb_shape_name(zb_shape s) {
  static const char *names[ZB_SHAPES] = {"sphere", "capsule", "box"};
  return s >= 0 && s < ZB_SHAPES ? names[s] : "?";
}
