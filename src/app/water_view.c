/* SPDX-License-Identifier: MIT */
#include "app/water_view.h"

#include <string.h>

#define N ZB_ACTIVE_RES
#define MIN_DEPTH 0.02 /* metres: thinner films are not drawn */
#define REBUILD 0.5    /* seconds between rebuilds of a chunk's mesh */
#define MAX_ENTRIES ZB_ACTIVE_MAX

typedef struct entry {
  zb_chunk chunk;
  uint32_t version;
  double built_at;
  uint32_t mesh, count;
  zb_origin frame;
  zb_vec3 center;
  float radius;
  bool seen;
} entry;

struct zb_water_view {
  const zb_allocator *a;
  zb_render *r;
  entry e[MAX_ENTRIES];
  int n;
};

zb_water_view *zb_water_view_create(const zb_allocator *a, zb_render *r) {
  zb_water_view *v = zb_calloc(a, sizeof *v);
  if (v) {
    v->a = a;
    v->r = r;
  }
  return v;
}

void zb_water_view_destroy(zb_water_view *v) {
  if (!v) {
    return;
  }
  for (int i = 0; i < v->n; ++i) {
    zb_render_release_mesh(v->r, v->e[i].mesh);
  }
  zb_free(v->a, v);
}

zb_result zb_standing_water_mesh(const zb_allocator *a, const zb_planet *p, const zb_water_chunk_view *c,
                                 zb_mesh_data *out) {
  memset(out, 0, sizeof *out);
  size_t wet = 0;
  for (int i = 0; i < N * N; ++i) {
    wet += c->depth[i] > MIN_DEPTH ? 1 : 0;
  }
  if (wet == 0) {
    return ZB_OK;
  }
  out->vertices = zb_alloc(a, wet * 4 * sizeof(zb_vertex));
  out->indices = zb_alloc(a, wet * 6 * sizeof(uint32_t));
  if (!out->vertices || !out->indices) {
    zb_mesh_free(a, out);
    return ZB_ERR_NOMEM;
  }
  zb_origin frame;
  zb_origin_init(&frame, p, c->chunk);
  zb_planet_pos centre = {c->chunk, 128.0f, 128.0f, 0.0f}, above = {c->chunk, 128.0f, 128.0f, 1.0f};
  zb_vec3 up =
      zb_v3_normalize(zb_v3_sub(zb_origin_to_local(&frame, p, &above), zb_origin_to_local(&frame, p, &centre)));
  const float step = 256.0f / N;
  size_t q = 0;
  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      int i = y * N + x;
      double d = c->depth[i];
      if (d <= MIN_DEPTH) {
        continue;
      }
      float level = (float)(c->ground[i] + d);
      float alpha = (float)(0.35 + d);
      alpha = alpha > 0.85f ? 0.85f : alpha;
      static const int corner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
      for (int k = 0; k < 4; ++k) {
        zb_planet_pos wp = {c->chunk, (float)(x + corner[k][0]) * step, (float)(y + corner[k][1]) * step, level};
        zb_vec3 l = zb_origin_to_local(&frame, p, &wp);
        zb_vertex *vx = &out->vertices[q * 4 + (size_t)k];
        memcpy(vx->pos, &l, sizeof vx->pos);
        memcpy(vx->normal, &up, sizeof vx->normal);
        const float tint[4] = {0.14f, 0.33f, 0.40f, alpha};
        memcpy(vx->color, tint, sizeof vx->color);
        vx->uv[0] = wp.lx;
        vx->uv[1] = wp.ly;
      }
      uint32_t b = (uint32_t)(q * 4);
      uint32_t tri[6] = {b, b + 2, b + 1, b, b + 3, b + 2};
      memcpy(&out->indices[q * 6], tri, sizeof tri);
      q++;
    }
  }
  out->vertex_count = wet * 4;
  out->index_count = wet * 6;
  zb_mesh_compute_bounds(out);
  return ZB_OK;
}

static entry *find(zb_water_view *v, zb_chunk c) {
  for (int i = 0; i < v->n; ++i) {
    if (zb_chunk_equal(v->e[i].chunk, c)) {
      return &v->e[i];
    }
  }
  if (v->n == MAX_ENTRIES) {
    return NULL;
  }
  entry *e = &v->e[v->n++];
  memset(e, 0, sizeof *e);
  e->chunk = c;
  e->version = UINT32_MAX;
  e->built_at = -1e9;
  return e;
}

static void rebuild(zb_water_view *v, entry *e, const zb_water_chunk_view *c, const zb_planet *p, double now) {
  zb_mesh_data md;
  e->built_at = now;
  e->version = c->version;
  if (zb_standing_water_mesh(v->a, p, c, &md) != ZB_OK) {
    return;
  }
  zb_render_release_mesh(v->r, e->mesh);
  e->mesh = md.index_count ? zb_render_upload_mesh(v->r, &md) : 0;
  e->count = e->mesh ? (uint32_t)md.index_count : 0;
  zb_origin_init(&e->frame, p, c->chunk);
  e->center = md.center;
  e->radius = md.radius;
  zb_mesh_free(v->a, &md);
}

int zb_water_view_draw(zb_water_view *v, const zb_surface_water *w, const zb_world *world, double now) {
  const zb_planet *p = zb_world_planet(world);
  for (int i = 0; i < v->n; ++i) {
    v->e[i].seen = false;
  }
  int drawn = 0;
  for (int k = 0; w && k < zb_surface_water_chunk_count(w); ++k) {
    zb_water_chunk_view c;
    zb_surface_water_chunk(w, k, &c);
    entry *e = find(v, c.chunk);
    if (!e) {
      continue;
    }
    e->seen = true;
    if (e->version != c.version && now - e->built_at >= REBUILD) {
      rebuild(v, e, &c, p, now);
    }
    if (e->mesh) {
      zb_render_draw_water(v->r, e->mesh, e->count, zb_origin_transform(&e->frame, zb_world_origin(world)), e->center,
                           e->radius);
      drawn++;
    }
  }
  /* Chunks that left the simulation lose their meshes. */
  for (int i = 0; i < v->n;) {
    if (v->e[i].seen) {
      ++i;
      continue;
    }
    zb_render_release_mesh(v->r, v->e[i].mesh);
    v->e[i] = v->e[--v->n];
  }
  return drawn;
}
