/* SPDX-License-Identifier: MIT */
#include "world/planet.h"

#include <math.h>
#include <stdio.h>

#include "core/dmath.h"

typedef struct face_axes {
  zb_dvec3 n, u, v; /* u x v = n */
} face_axes;

static const face_axes FACES[ZB_PLANET_FACES] = {
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},  /* +X */
    {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}, /* -X */
    {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},  /* +Y */
    {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}, /* -Y */
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  /* +Z */
    {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}, /* -Z */
};

#define QUARTER_PI (ZB_DPI / 4.0)

static zb_dvec3 dv(double x, double y, double z) {
  zb_dvec3 r = {x, y, z};
  return r;
}
static zb_dvec3 dv_add(zb_dvec3 a, zb_dvec3 b) { return dv(a.x + b.x, a.y + b.y, a.z + b.z); }
static zb_dvec3 dv_sub(zb_dvec3 a, zb_dvec3 b) { return dv(a.x - b.x, a.y - b.y, a.z - b.z); }
static zb_dvec3 dv_scale(zb_dvec3 a, double s) { return dv(a.x * s, a.y * s, a.z * s); }
static double dv_dot(zb_dvec3 a, zb_dvec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static zb_dvec3 dv_cross(zb_dvec3 a, zb_dvec3 b) {
  return dv(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static double dv_len(zb_dvec3 a) { return sqrt(dv_dot(a, a)); }
static zb_dvec3 dv_norm(zb_dvec3 a) { return dv_scale(a, 1.0 / dv_len(a)); }

bool zb_planet_init(zb_planet *p, double radius, char *err, size_t err_len) {
  if (!(radius >= ZB_PLANET_MIN_RADIUS && radius <= ZB_PLANET_MAX_RADIUS)) {
    if (err && err_len) {
      snprintf(err, err_len, "planet radius %.0f m outside the allowed range %.0f..%.0f m", radius,
               ZB_PLANET_MIN_RADIUS, ZB_PLANET_MAX_RADIUS);
    }
    return false;
  }
  p->radius = radius;
  p->n = (int32_t)floor(radius * (ZB_DPI / 2.0) / ZB_CHUNK_SIZE + 0.5);
  p->chunk_uv = 2.0 / (double)p->n;
  return true;
}

bool zb_chunk_valid(const zb_planet *p, zb_chunk c) {
  return c.face >= 0 && c.face < ZB_PLANET_FACES && c.x >= 0 && c.x < p->n && c.y >= 0 && c.y < p->n;
}

bool zb_chunk_equal(zb_chunk a, zb_chunk b) { return a.face == b.face && a.x == b.x && a.y == b.y; }

uint64_t zb_chunk_key(zb_chunk c) {
  return (uint64_t)(uint32_t)c.face << 60 | (uint64_t)((uint32_t)c.x & 0x3fffffffu) << 30 |
         (uint64_t)((uint32_t)c.y & 0x3fffffffu);
}

zb_chunk zb_chunk_from_key(uint64_t key) {
  zb_chunk c = {(int32_t)(key >> 60), (int32_t)((key >> 30) & 0x3fffffffu), (int32_t)(key & 0x3fffffffu)};
  return c;
}

void zb_planet_face_axes(int face, zb_dvec3 *n, zb_dvec3 *u, zb_dvec3 *v) {
  *n = FACES[face].n;
  *u = FACES[face].u;
  *v = FACES[face].v;
}

zb_dvec3 zb_planet_face_dir(int face, double u, double v) {
  const face_axes *f = &FACES[face];
  double a = zb_dtan(u * QUARTER_PI);
  double b = zb_dtan(v * QUARTER_PI);
  return dv_norm(dv_add(f->n, dv_add(dv_scale(f->u, a), dv_scale(f->v, b))));
}

int zb_planet_dir_face(zb_dvec3 d) {
  double ax = fabs(d.x), ay = fabs(d.y), az = fabs(d.z);
  if (ax >= ay && ax >= az) {
    return d.x >= 0.0 ? ZB_FACE_PX : ZB_FACE_NX;
  }
  if (ay >= az) {
    return d.y >= 0.0 ? ZB_FACE_PY : ZB_FACE_NY;
  }
  return d.z >= 0.0 ? ZB_FACE_PZ : ZB_FACE_NZ;
}

void zb_planet_dir_uv(int face, zb_dvec3 d, double *u, double *v) {
  const face_axes *f = &FACES[face];
  double dn = dv_dot(d, f->n);
  *u = zb_datan2(dv_dot(d, f->u), dn) / QUARTER_PI;
  *v = zb_datan2(dv_dot(d, f->v), dn) / QUARTER_PI;
}

/* Face coordinate (-1..1) to chunk index and nominal offset. */
static void uv_to_cell(const zb_planet *p, double u, int32_t *cell, float *offset) {
  double t = (u + 1.0) * 0.5 * (double)p->n;
  double c = floor(t);
  if (c < 0.0) {
    c = 0.0;
  } else if (c > (double)(p->n - 1)) {
    c = (double)(p->n - 1);
  }
  double o = (t - c) * ZB_CHUNK_SIZE;
  if (o < 0.0) {
    o = 0.0;
  } else if (o > ZB_CHUNK_SIZE) {
    o = ZB_CHUNK_SIZE;
  }
  *cell = (int32_t)c;
  *offset = (float)o;
}

static double cell_to_uv(const zb_planet *p, int32_t cell, double offset) {
  return -1.0 + ((double)cell + offset / ZB_CHUNK_SIZE) * p->chunk_uv;
}

static zb_dvec3 pos_dir(const zb_planet *p, const zb_planet_pos *pos) {
  return zb_planet_face_dir(pos->chunk.face, cell_to_uv(p, pos->chunk.x, pos->lx),
                            cell_to_uv(p, pos->chunk.y, pos->ly));
}

zb_dvec3 zb_planet_to_cart(const zb_planet *p, const zb_planet_pos *pos) {
  return dv_scale(pos_dir(p, pos), p->radius + (double)pos->alt);
}

zb_planet_pos zb_planet_from_cart(const zb_planet *p, zb_dvec3 c) {
  zb_planet_pos pos;
  double len = dv_len(c);
  zb_dvec3 d = len > 0.0 ? dv_scale(c, 1.0 / len) : dv(1, 0, 0);
  int face = zb_planet_dir_face(d);
  double u, v;
  zb_planet_dir_uv(face, d, &u, &v);
  pos.chunk.face = face;
  uv_to_cell(p, u, &pos.chunk.x, &pos.lx);
  uv_to_cell(p, v, &pos.chunk.y, &pos.ly);
  pos.alt = (float)(len - p->radius);
  return pos;
}

zb_planet_pos zb_planet_chunk_center(zb_chunk c) {
  zb_planet_pos pos = {c, (float)(ZB_CHUNK_SIZE / 2), (float)(ZB_CHUNK_SIZE / 2), 0.0f};
  return pos;
}

zb_dvec3 zb_planet_chunk_dir(const zb_planet *p, zb_chunk c) {
  zb_planet_pos pos = zb_planet_chunk_center(c);
  return pos_dir(p, &pos);
}

/* The chunk across one face edge from an edge chunk. axis 0 crosses in u,
 * axis 1 in v; dir is the crossing direction (+1 or -1). The edge point is
 * mapped to the adjacent face, where the coordinate along the shared edge is
 * the same and the other one is +-1. */
static zb_chunk cross_edge(const zb_planet *p, zb_chunk c, int axis, int dir) {
  const face_axes *f = &FACES[c.face];
  double along = cell_to_uv(p, axis == 0 ? c.y : c.x, ZB_CHUNK_SIZE / 2);
  double b = zb_dtan(along * QUARTER_PI);
  zb_dvec3 out_axis = axis == 0 ? f->u : f->v;
  zb_dvec3 along_axis = axis == 0 ? f->v : f->u;
  zb_dvec3 edge = dv_add(f->n, dv_add(dv_scale(out_axis, (double)dir), dv_scale(along_axis, b)));
  zb_dvec3 target = dv_scale(out_axis, (double)dir); /* normal of the adjacent face */
  zb_chunk r = {zb_planet_dir_face(target), 0, 0};
  double u, v;
  zb_planet_dir_uv(r.face, edge, &u, &v);
  float unused;
  /* The coordinate that is +-1 on the new face snaps to its edge row/column. */
  if (fabs(u) > fabs(v)) {
    r.x = u > 0.0 ? p->n - 1 : 0;
    uv_to_cell(p, v, &r.y, &unused);
  } else {
    r.y = v > 0.0 ? p->n - 1 : 0;
    uv_to_cell(p, u, &r.x, &unused);
  }
  return r;
}

bool zb_planet_neighbor(const zb_planet *p, zb_chunk c, int dx, int dy, zb_chunk *out) {
  int32_t nx = c.x + dx, ny = c.y + dy;
  bool out_x = nx < 0 || nx >= p->n;
  bool out_y = ny < 0 || ny >= p->n;
  if (out_x && out_y) {
    return false; /* the missing diagonal at a cube corner */
  }
  if (!out_x && !out_y) {
    zb_chunk r = {c.face, nx, ny};
    *out = r;
    return true;
  }
  if (out_x) {
    zb_chunk step = {c.face, c.x, ny};
    *out = cross_edge(p, step, 0, dx);
  } else {
    zb_chunk step = {c.face, nx, c.y};
    *out = cross_edge(p, step, 1, dy);
  }
  return true;
}

int zb_planet_neighbors(const zb_planet *p, zb_chunk c, zb_chunk out[8]) {
  int count = 0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      zb_chunk nb;
      if ((dx == 0 && dy == 0) || !zb_planet_neighbor(p, c, dx, dy, &nb)) {
        continue;
      }
      bool dup = zb_chunk_equal(nb, c);
      for (int i = 0; i < count && !dup; ++i) {
        dup = zb_chunk_equal(out[i], nb);
      }
      if (!dup) {
        out[count++] = nb;
      }
    }
  }
  return count;
}

void zb_planet_frame(zb_dvec3 dir, zb_dvec3 *east, zb_dvec3 *north, zb_dvec3 *up) {
  zb_dvec3 u = dv_norm(dir);
  zb_dvec3 e = dv_cross(dv(0, 0, 1), u);
  double len = dv_len(e);
  e = len > 1e-12 ? dv_scale(e, 1.0 / len) : dv(0, 1, 0);
  *up = u;
  *east = e;
  *north = dv_cross(u, e);
}

zb_planet_pos zb_planet_move(const zb_planet *p, const zb_planet_pos *pos, double heading, double forward, double right,
                             double up, double *new_heading) {
  zb_dvec3 c = zb_planet_to_cart(p, pos);
  zb_dvec3 e, n, u;
  zb_planet_frame(c, &e, &n, &u);
  double ch = zb_dcos(heading), sh = zb_dsin(heading);
  zb_dvec3 fwd = dv_add(dv_scale(n, ch), dv_scale(e, sh));
  zb_dvec3 rgt = dv_sub(dv_scale(e, ch), dv_scale(n, sh));
  zb_dvec3 moved = dv_add(c, dv_add(dv_scale(fwd, forward), dv_scale(rgt, right)));
  zb_planet_pos out = zb_planet_from_cart(p, moved);
  out.alt = (float)((double)out.alt + up);
  /* Carry the forward direction into the new tangent plane. */
  zb_dvec3 e2, n2, u2;
  zb_planet_frame(moved, &e2, &n2, &u2);
  zb_dvec3 f2 = dv_sub(fwd, dv_scale(u2, dv_dot(fwd, u2)));
  *new_heading = zb_datan2(dv_dot(f2, e2), dv_dot(f2, n2));
  return out;
}

static double angle_between(zb_dvec3 a, zb_dvec3 b) { return zb_datan2(dv_len(dv_cross(a, b)), dv_dot(a, b)); }

double zb_planet_distance(const zb_planet *p, const zb_planet_pos *a, const zb_planet_pos *b) {
  return p->radius * angle_between(pos_dir(p, a), pos_dir(p, b));
}

void zb_planet_chunk_size(const zb_planet *p, zb_chunk c, double *width, double *height) {
  double u0 = cell_to_uv(p, c.x, 0), u1 = cell_to_uv(p, c.x, ZB_CHUNK_SIZE);
  double v0 = cell_to_uv(p, c.y, 0), v1 = cell_to_uv(p, c.y, ZB_CHUNK_SIZE);
  double um = (u0 + u1) * 0.5, vm = (v0 + v1) * 0.5;
  *width = p->radius * angle_between(zb_planet_face_dir(c.face, u0, vm), zb_planet_face_dir(c.face, u1, vm));
  *height = p->radius * angle_between(zb_planet_face_dir(c.face, um, v0), zb_planet_face_dir(c.face, um, v1));
}

/* ---- Floating origin ------------------------------------------------------ */

void zb_origin_init(zb_origin *o, const zb_planet *p, zb_chunk anchor) {
  zb_dvec3 d = zb_planet_chunk_dir(p, anchor);
  o->chunk = anchor;
  o->center = dv_scale(d, p->radius);
  zb_planet_frame(d, &o->east, &o->north, &o->up);
}

zb_vec3 zb_origin_from_cart(const zb_origin *o, zb_dvec3 c) {
  zb_dvec3 r = dv_sub(c, o->center);
  return zb_v3((float)dv_dot(r, o->east), (float)dv_dot(r, o->up), (float)-dv_dot(r, o->north));
}

zb_dvec3 zb_origin_to_cart(const zb_origin *o, zb_vec3 l) {
  zb_dvec3 r = dv_add(dv_scale(o->east, (double)l.x), dv_scale(o->up, (double)l.y));
  return dv_add(o->center, dv_add(r, dv_scale(o->north, -(double)l.z)));
}

zb_vec3 zb_origin_to_local(const zb_origin *o, const zb_planet *p, const zb_planet_pos *pos) {
  return zb_origin_from_cart(o, zb_planet_to_cart(p, pos));
}

zb_planet_pos zb_origin_to_pos(const zb_origin *o, const zb_planet *p, zb_vec3 local) {
  return zb_planet_from_cart(p, zb_origin_to_cart(o, local));
}

bool zb_origin_should_rebase(zb_vec3 local, float distance) { return zb_v3_dot(local, local) > distance * distance; }

/* Local axis i (x = east, y = up, z = south) of an origin in planet space. */
static zb_dvec3 local_axis(const zb_origin *o, int i) {
  return i == 0 ? o->east : (i == 1 ? o->up : dv_scale(o->north, -1.0));
}

zb_mat4 zb_origin_transform(const zb_origin *from, const zb_origin *to) {
  zb_mat4 m = zb_m4_identity();
  for (int col = 0; col < 3; ++col) {
    zb_dvec3 a = local_axis(from, col);
    for (int row = 0; row < 3; ++row) {
      m.m[col * 4 + row] = (float)dv_dot(local_axis(to, row), a);
    }
  }
  zb_dvec3 t = dv_sub(from->center, to->center);
  for (int row = 0; row < 3; ++row) {
    m.m[12 + row] = (float)dv_dot(local_axis(to, row), t);
  }
  return m;
}

zb_mat4 zb_origin_rebase(zb_origin *o, const zb_planet *p, zb_chunk anchor) {
  zb_origin old = *o;
  zb_origin_init(o, p, anchor);
  return zb_origin_transform(&old, o);
}
