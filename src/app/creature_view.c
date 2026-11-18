/* SPDX-License-Identifier: MIT */
#include "app/creature_view.h"

#include "world/dvec.h"

#include <math.h>
#include <string.h>

typedef struct part_mesh {
  uint32_t mesh, count;
} part_mesh;

struct zb_creature_view {
  const zb_allocator *a;
  zb_render *render;
  part_mesh meshes[ZB_PARTS][ZB_SPECIES_MAX];
};

zb_creature_view *zb_creature_view_create(const zb_allocator *a, zb_render *r) {
  zb_creature_view *v = zb_calloc(a, sizeof *v);
  if (v) {
    v->a = a;
    v->render = r;
  }
  return v;
}

void zb_creature_view_destroy(zb_creature_view *v) {
  if (!v) {
    return;
  }
  for (int p = 0; p < ZB_PARTS; ++p) {
    for (int s = 0; s < ZB_SPECIES_MAX; ++s) {
      if (v->meshes[p][s].mesh) {
        zb_render_release_mesh(v->render, v->meshes[p][s].mesh);
      }
    }
  }
  zb_free(v->a, v);
}

static const part_mesh *mesh_for(zb_creature_view *v, zb_body_part part, int species, const float color[3]) {
  part_mesh *pm = &v->meshes[part][species];
  if (!pm->mesh) {
    zb_mesh_data md;
    if (zb_body_part_mesh(v->a, part, color, &md) != ZB_OK) {
      return NULL;
    }
    pm->mesh = zb_render_upload_mesh(v->render, &md);
    pm->count = (uint32_t)md.index_count;
    zb_mesh_free(v->a, &md);
  }
  return pm->mesh ? pm : NULL;
}

typedef struct draw_ctx {
  zb_creature_view *v;
  const zb_creatures *c;
  const zb_origin *origin;
  const zb_planet *planet;
  zb_vec3 eye;
  float range;
  double time;
  int drawn;
} draw_ctx;

static zb_vec3 local_dir(const zb_origin *o, zb_dvec3 d) {
  return zb_v3((float)zb_dv_dot(d, o->east), (float)zb_dv_dot(d, o->up), (float)-zb_dv_dot(d, o->north));
}

/* A part with its own axes x (side), y (forward), z (up) scaled, at centre. */
static void part(draw_ctx *dc, zb_body_part p, int species, const float color[3], zb_vec3 centre, zb_vec3 ax,
                 zb_vec3 ay, zb_vec3 az, float sx, float sy, float sz) {
  const part_mesh *pm = mesh_for(dc->v, p, species, color);
  if (!pm) {
    return;
  }
  zb_mat4 m = zb_m4_identity();
  const zb_vec3 axes[3] = {ax, ay, az};
  const float scale[3] = {sx, sy, sz};
  for (int k = 0; k < 3; ++k) {
    m.m[k * 4 + 0] = axes[k].x * scale[k];
    m.m[k * 4 + 1] = axes[k].y * scale[k];
    m.m[k * 4 + 2] = axes[k].z * scale[k];
  }
  m.m[12] = centre.x;
  m.m[13] = centre.y;
  m.m[14] = centre.z;
  zb_render_draw_mesh(dc->v->render, pm->mesh, pm->count, m, zb_v3(0, 0, 0), 1.75f);
}

static zb_vec3 at(zb_vec3 c, zb_vec3 a, float ka, zb_vec3 b, float kb, zb_vec3 d, float kd) {
  return zb_v3_add(c, zb_v3_add(zb_v3_scale(a, ka), zb_v3_add(zb_v3_scale(b, kb), zb_v3_scale(d, kd))));
}

static void draw_one(void *ctx, const zb_creature_state *s, double phase) {
  draw_ctx *dc = ctx;
  zb_vec3 c = zb_origin_to_local(dc->origin, dc->planet, &s->pos);
  if (zb_v3_length(zb_v3_sub(c, dc->eye)) > dc->range) {
    return;
  }
  const zb_animal *a = zb_creatures_species(dc->c, s->species);
  zb_dvec3 e, n, u;
  zb_planet_frame(zb_dv_norm(zb_planet_to_cart(dc->planet, &s->pos)), &e, &n, &u);
  zb_dvec3 fwd = zb_dv_add(zb_dv_scale(e, sin(s->heading)), zb_dv_scale(n, cos(s->heading)));
  zb_vec3 f = local_dir(dc->origin, fwd), up = local_dir(dc->origin, u);
  zb_vec3 side = zb_v3_normalize(zb_v3_cross(f, up));
  if (!s->alive) { /* lies on its side */
    zb_vec3 t = up;
    up = side;
    side = zb_v3_scale(t, -1.0f);
  }
  float len = (float)a->length, r = len * 0.18f;
  const float *col = a->color;
  if (a->flies) {
    zb_vec3 body = c;
    part(dc, ZB_PART_BALL, s->species, col, body, side, f, up, r * 1.2f, len * 0.35f, r);
    part(dc, ZB_PART_BALL, s->species, col, at(body, f, len * 0.38f, up, r * 0.3f, side, 0), side, f, up, r * 0.6f,
         r * 0.6f, r * 0.6f);
    float flap = s->alive ? (float)sin(dc->time * 8.0 + s->id) * 0.6f : 0.0f;
    for (int k = -1; k <= 1; k += 2) {
      zb_vec3 wing_side = zb_v3_normalize(zb_v3_add(zb_v3_scale(side, (float)k), zb_v3_scale(up, flap)));
      zb_vec3 wing_up = zb_v3_normalize(zb_v3_cross(wing_side, f));
      part(dc, ZB_PART_BLOCK, s->species, col, zb_v3_add(body, zb_v3_scale(wing_side, len * 0.45f)), wing_side, f,
           wing_up, len * 0.4f, len * 0.15f, len * 0.02f);
    }
  } else {
    float leg = len * 0.25f;
    zb_vec3 body = zb_v3_add(c, zb_v3_scale(up, leg * 2.0f + r));
    part(dc, ZB_PART_TUBE, s->species, col, body, side, up, f, r, r, len * 0.3f);
    for (int k = -1; k <= 1; k += 2) {
      part(dc, ZB_PART_BALL, s->species, col, zb_v3_add(body, zb_v3_scale(f, (float)k * len * 0.3f)), side, f, up, r, r,
           r);
    }
    part(dc, ZB_PART_BALL, s->species, col, at(body, f, len * 0.48f, up, r * 0.8f, side, 0), side, f, up, r * 0.8f,
         r * 1.1f, r * 0.8f);
    float swing = (float)sin(phase * 2.0 * 3.14159265) * 0.5f;
    for (int i = 0; i < 4; ++i) {
      float fore = i < 2 ? 1.0f : -1.0f, lr = i % 2 ? 1.0f : -1.0f, sw = swing * (i == 0 || i == 3 ? 1.0f : -1.0f);
      zb_vec3 leg_axis = zb_v3_normalize(zb_v3_add(up, zb_v3_scale(f, sw)));
      zb_vec3 hip = at(body, f, fore * len * 0.25f, side, lr * r * 0.7f, up, -r * 0.5f);
      part(dc, ZB_PART_TUBE, s->species, col, zb_v3_sub(hip, zb_v3_scale(leg_axis, leg)), side, f, leg_axis,
           len * 0.05f, len * 0.05f, leg);
    }
  }
  dc->drawn++;
}

int zb_creature_view_draw(zb_creature_view *v, const zb_creatures *c, const zb_world *world, zb_vec3 eye, float range,
                          double time) {
  if (!v || !c) {
    return 0;
  }
  draw_ctx dc = {v, c, zb_world_origin(world), zb_world_planet(world), eye, range, time, 0};
  zb_creatures_each(c, draw_one, &dc);
  return dc.drawn;
}
