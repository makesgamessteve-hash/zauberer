/* SPDX-License-Identifier: MIT */
#include "app/body_view.h"

#include "world/dvec.h"

#include <math.h>
#include <string.h>

#define TUBE_SIDES 10

typedef struct part_mesh {
  uint32_t mesh, count;
} part_mesh;

struct zb_body_view {
  const zb_allocator *a;
  zb_render *render;
  const zb_materials *materials;
  part_mesh meshes[ZB_PARTS][ZB_MATERIALS_MAX];
};

zb_body_view *zb_body_view_create(const zb_allocator *a, zb_render *r, const zb_materials *materials) {
  zb_body_view *v = zb_calloc(a, sizeof *v);
  if (v) {
    v->a = a;
    v->render = r;
    v->materials = materials;
  }
  return v;
}

void zb_body_view_destroy(zb_body_view *v) {
  if (!v) {
    return;
  }
  for (int p = 0; p < ZB_PARTS; ++p) {
    for (int m = 0; m < ZB_MATERIALS_MAX; ++m) {
      if (v->meshes[p][m].mesh) {
        zb_render_release_mesh(v->render, v->meshes[p][m].mesh);
      }
    }
  }
  zb_free(v->a, v);
}

/* ---- Meshes ---- */

typedef struct builder {
  zb_vertex *v;
  uint32_t *i;
  size_t nv, ni;
  float color[3];
} builder;

/* A flat-shaded triangle (counter-clockwise seen from outside). */
static void tri(builder *b, zb_vec3 p0, zb_vec3 p1, zb_vec3 p2) {
  zb_vec3 n = zb_v3_normalize(zb_v3_cross(zb_v3_sub(p1, p0), zb_v3_sub(p2, p0)));
  zb_vec3 p[3] = {p0, p1, p2};
  for (int k = 0; k < 3; ++k) {
    zb_vertex *x = &b->v[b->nv];
    memset(x, 0, sizeof *x);
    x->pos[0] = p[k].x;
    x->pos[1] = p[k].y;
    x->pos[2] = p[k].z;
    x->normal[0] = n.x;
    x->normal[1] = n.y;
    x->normal[2] = n.z;
    /* A little variation between facets, as hewn stone or wood. */
    float shade = 0.92f + 0.08f * (float)((b->nv / 3 * 7) % 5) / 4.0f;
    for (int c = 0; c < 3; ++c) {
      x->color[c] = b->color[c] * shade;
    }
    x->color[3] = 1.0f;
    b->i[b->ni++] = (uint32_t)b->nv++;
  }
}

static void quad(builder *b, zb_vec3 p0, zb_vec3 p1, zb_vec3 p2, zb_vec3 p3) {
  tri(b, p0, p1, p2);
  tri(b, p0, p2, p3);
}

static zb_vec3 on_sphere(zb_vec3 p) { return zb_v3_normalize(p); }

/* An icosahedron split once: 80 facets. */
static void ball(builder *b) {
  const float t = 1.6180339887f;
  zb_vec3 v[12] = {{-1, t, 0},  {1, t, 0},  {-1, -t, 0}, {1, -t, 0}, {0, -1, t},  {0, 1, t},
                   {0, -1, -t}, {0, 1, -t}, {t, 0, -1},  {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
  static const int f[20][3] = {{0, 11, 5},  {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
                               {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},   {3, 2, 6}, {3, 6, 8},
                               {3, 8, 9},   {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},   {9, 8, 1}};
  for (int k = 0; k < 20; ++k) {
    zb_vec3 a = on_sphere(v[f[k][0]]), c = on_sphere(v[f[k][1]]), d = on_sphere(v[f[k][2]]);
    zb_vec3 ac = on_sphere(zb_v3_add(a, c)), cd = on_sphere(zb_v3_add(c, d)), da = on_sphere(zb_v3_add(d, a));
    tri(b, a, ac, da);
    tri(b, ac, c, cd);
    tri(b, da, cd, d);
    tri(b, ac, cd, da);
  }
}

static void block(builder *b) {
  for (int axis = 0; axis < 3; ++axis) {
    for (int s = -1; s <= 1; s += 2) {
      float c[3][4];
      int u = (axis + 1) % 3, w = (axis + 2) % 3;
      const float us[4] = {-1, 1, 1, -1}, ws[4] = {-1, -1, 1, 1};
      zb_vec3 p[4];
      for (int k = 0; k < 4; ++k) {
        c[axis][k] = (float)s;
        c[u][k] = us[k] * (float)s;
        c[w][k] = ws[k];
        p[k] = zb_v3(c[0][k], c[1][k], c[2][k]);
      }
      quad(b, p[0], p[1], p[2], p[3]);
    }
  }
}

static void tube(builder *b) {
  for (int k = 0; k < TUBE_SIDES; ++k) {
    float a0 = 6.2831853f * (float)k / TUBE_SIDES, a1 = 6.2831853f * (float)(k + 1) / TUBE_SIDES;
    zb_vec3 p0 = {cosf(a0), sinf(a0), -1.0f}, p1 = {cosf(a1), sinf(a1), -1.0f};
    zb_vec3 q0 = {p0.x, p0.y, 1.0f}, q1 = {p1.x, p1.y, 1.0f};
    quad(b, p0, p1, q1, q0);
  }
}

zb_result zb_body_part_mesh(const zb_allocator *a, zb_body_part part, const float color[3], zb_mesh_data *out) {
  memset(out, 0, sizeof *out);
  size_t tris = part == ZB_PART_BALL ? 80 : (part == ZB_PART_BLOCK ? 12 : 2 * TUBE_SIDES);
  builder b = {zb_alloc(a, sizeof(zb_vertex) * tris * 3), zb_alloc(a, sizeof(uint32_t) * tris * 3), 0, 0, {0}};
  if (!b.v || !b.i) {
    zb_free(a, b.v);
    zb_free(a, b.i);
    return ZB_ERR_NOMEM;
  }
  memcpy(b.color, color, sizeof b.color);
  switch (part) {
    case ZB_PART_BALL: ball(&b); break;
    case ZB_PART_BLOCK: block(&b); break;
    default: tube(&b); break;
  }
  out->vertices = b.v;
  out->vertex_count = b.nv;
  out->indices = b.i;
  out->index_count = b.ni;
  zb_mesh_compute_bounds(out);
  return ZB_OK;
}

static const part_mesh *mesh_for(zb_body_view *v, zb_body_part part, int material) {
  part_mesh *pm = &v->meshes[part][material];
  if (!pm->mesh) {
    zb_mesh_data md;
    if (zb_body_part_mesh(v->a, part, v->materials->info[material].color, &md) != ZB_OK) {
      return NULL;
    }
    pm->mesh = zb_render_upload_mesh(v->render, &md);
    pm->count = (uint32_t)md.index_count;
    zb_mesh_free(v->a, &md);
  }
  return pm->mesh ? pm : NULL;
}

/* ---- Drawing ---- */

typedef struct draw_ctx {
  zb_body_view *v;
  const zb_origin *origin;
  zb_vec3 eye;
  float range;
  int drawn;
} draw_ctx;

static zb_vec3 local_dir(const zb_origin *o, zb_dvec3 d) {
  return zb_v3((float)zb_dv_dot(d, o->east), (float)zb_dv_dot(d, o->up), (float)-zb_dv_dot(d, o->north));
}

/* A transform placing a part: its x, y, z scaled and rotated onto the
 * body's axes, at centre. */
static zb_mat4 place(zb_vec3 centre, const zb_vec3 axis[3], const float scale[3]) {
  zb_mat4 m = zb_m4_identity();
  for (int k = 0; k < 3; ++k) {
    m.m[k * 4 + 0] = axis[k].x * scale[k];
    m.m[k * 4 + 1] = axis[k].y * scale[k];
    m.m[k * 4 + 2] = axis[k].z * scale[k];
  }
  m.m[12] = centre.x;
  m.m[13] = centre.y;
  m.m[14] = centre.z;
  return m;
}

static void draw_part(draw_ctx *dc, zb_body_part part, int material, zb_mat4 m) {
  const part_mesh *pm = mesh_for(dc->v, part, material);
  if (pm) {
    zb_render_draw_mesh(dc->v->render, pm->mesh, pm->count, m, zb_v3(0, 0, 0), part == ZB_PART_BALL ? 1.0f : 1.75f);
  }
}

static void draw_body(void *ctx, const zb_body_pose *p) {
  draw_ctx *dc = ctx;
  zb_vec3 c = zb_origin_from_cart(dc->origin, p->centre);
  if (zb_v3_length(zb_v3_sub(c, dc->eye)) > dc->range) {
    return;
  }
  zb_vec3 axis[3];
  for (int k = 0; k < 3; ++k) {
    axis[k] = local_dir(dc->origin, p->axis[k]);
  }
  float r = (float)p->radius;
  switch (p->shape) {
    case ZB_SHAPE_SPHERE: {
      float s[3] = {r, r, r};
      draw_part(dc, ZB_PART_BALL, p->material, place(c, axis, s));
      break;
    }
    case ZB_SHAPE_BOX: {
      float s[3] = {(float)p->half[0], (float)p->half[1], (float)p->half[2]};
      draw_part(dc, ZB_PART_BLOCK, p->material, place(c, axis, s));
      break;
    }
    default: {
      float h = (float)p->half_length, s[3] = {r, r, h}, e[3] = {r, r, r};
      draw_part(dc, ZB_PART_TUBE, p->material, place(c, axis, s));
      for (int k = -1; k <= 1; k += 2) {
        zb_vec3 end = zb_v3_add(c, zb_v3_scale(axis[2], h * (float)k));
        draw_part(dc, ZB_PART_BALL, p->material, place(end, axis, e));
      }
      break;
    }
  }
  dc->drawn++;
}

int zb_body_view_draw(zb_body_view *v, const zb_bodies *b, const zb_world *world, zb_vec3 eye, float range) {
  if (!v || !b) {
    return 0;
  }
  draw_ctx dc = {v, zb_world_origin(world), eye, range, 0};
  zb_bodies_each(b, draw_body, &dc);
  return dc.drawn;
}
