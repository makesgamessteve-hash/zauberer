/* SPDX-License-Identifier: MIT */
#include "app/terrain_view.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/array.h"
#include "core/dmath.h"
#include "core/log.h"
#include "core/luon.h"

#define WATER_GRID 5      /* water surface vertices per chunk edge */
#define UNTEXTURED 255.0f /* colour alpha marker: no atlas layer */

typedef struct cache_entry {
  zb_chunk chunk;
  uint32_t mesh, count, version, origin_version;
  int lod;
  bool transformed;
  zb_origin frame; /* the chunk's tangent frame */
  zb_mat4 transform;
  zb_vec3 center;
  float radius;
  uint64_t seen;
} cache_entry;

typedef struct build_request {
  zb_chunk chunk;
  const zb_chunk_terrain *ct;
  uint32_t version;
  int lod;
  float distance;
} build_request;

struct zb_terrain_view {
  const zb_allocator *a;
  zb_render *r;
  zb_terrain_view_config cfg;
  zb_vec3 colors[ZB_MAT_COUNT];
  int layers[ZB_MAT_COUNT];
  zb_array entries; /* cache_entry */
  zb_map index;     /* chunk key -> entry index */
  zb_array requests;
  uint64_t frame;
  zb_terrain_view_stats stats;
  /* per-frame visit state */
  zb_world *world;
  zb_vec3 eye;
};

void zb_terrain_view_default_config(zb_terrain_view_config *c) {
  c->lod_distance[0] = 600.0f;
  c->lod_distance[1] = 1600.0f;
  c->lod_distance[2] = 4000.0f;
  c->lod_distance[3] = 8000.0f;
  c->fine_builds = 4;
  c->coarse_builds = 48;
}

/* Fallback material colours (palette ramps: green, sand, stone, snow, brown). */
static const zb_vec3 DEFAULT_COLORS[ZB_MAT_COUNT] = {
    {0.0f, 0.0f, 0.0f},    {0.36f, 0.47f, 0.22f}, {0.77f, 0.68f, 0.48f}, {0.45f, 0.43f, 0.41f},
    {0.58f, 0.56f, 0.52f}, {0.92f, 0.94f, 0.96f}, {0.75f, 0.86f, 0.93f}, {0.33f, 0.25f, 0.18f}};
static const zb_vec3 WATER_COLOR = {0.13f, 0.32f, 0.47f};
/* Atlas layer standing in for each material until vegetation arrives (M2). */
static const char *const MATERIAL_LAYER[ZB_MAT_COUNT] = {NULL, "grass", "sand", "rock", "gravel", "snow", "ice", "mud"};

/* Layer indices and average colours from the atlas description and image. */
static void load_atlas(zb_terrain_view *tv, zb_vfs *vfs, const char *atlas) {
  char *text = NULL;
  size_t len = 0;
  if (!vfs || !atlas || zb_vfs_read(vfs, atlas, tv->a, &text, &len) != ZB_OK) {
    return;
  }
  zb_luon_doc doc;
  zb_luon_error err;
  if (zb_luon_parse(&doc, tv->a, text, len, &err) != ZB_OK) {
    zb_warn("%s:%d: %s", atlas, err.line, err.message);
    zb_free(tv->a, text);
    return;
  }
  zb_free(tv->a, text);
  const char *texture = zb_value_str(zb_table_get(&doc.root, "texture"), NULL);
  const zb_value *names = zb_table_get(&doc.root, "layers");
  int count = (int)zb_table_len(names);
  char *png = NULL;
  zb_image img = {0, 0, NULL};
  if (texture && count > 0 && zb_vfs_read(vfs, texture, tv->a, &png, &len) == ZB_OK &&
      zb_image_decode_png(tv->a, png, len, &img) == ZB_OK && img.height >= count && img.width > 0) {
    int size = img.height / count;
    for (int m = 1; m < ZB_MAT_COUNT; ++m) {
      for (int l = 0; l < count; ++l) {
        if (strcmp(zb_value_str(zb_table_index(names, (size_t)l + 1), ""), MATERIAL_LAYER[m]) != 0) {
          continue;
        }
        double sum[3] = {0, 0, 0};
        for (int y = l * size; y < (l + 1) * size; ++y) {
          for (int x = 0; x < img.width; ++x) {
            const unsigned char *px = &img.rgba[((size_t)y * (size_t)img.width + (size_t)x) * 4];
            for (int k = 0; k < 3; ++k) {
              sum[k] += px[k];
            }
          }
        }
        double n = (double)size * img.width * 255.0;
        tv->colors[m] = zb_v3((float)(sum[0] / n), (float)(sum[1] / n), (float)(sum[2] / n));
        tv->layers[m] = l;
      }
    }
    zb_render_set_terrain_atlas(tv->r, zb_render_load_texture(tv->r, texture), count);
  } else {
    zb_warn("terrain atlas %s could not be read; terrain is drawn untextured", atlas);
  }
  zb_image_free(tv->a, &img);
  zb_free(tv->a, png);
  zb_luon_free(&doc);
}

zb_terrain_view *zb_terrain_view_create(const zb_allocator *a, zb_render *r, zb_vfs *vfs, const char *atlas,
                                        const zb_terrain_view_config *cfg) {
  zb_terrain_view *tv = zb_calloc(a, sizeof *tv);
  if (!tv) {
    return NULL;
  }
  tv->a = a;
  tv->r = r;
  if (cfg) {
    tv->cfg = *cfg;
  } else {
    zb_terrain_view_default_config(&tv->cfg);
  }
  memcpy(tv->colors, DEFAULT_COLORS, sizeof tv->colors);
  for (int m = 0; m < ZB_MAT_COUNT; ++m) {
    tv->layers[m] = -1;
  }
  zb_array_init(&tv->entries, a, sizeof(cache_entry));
  zb_array_init(&tv->requests, a, sizeof(build_request));
  zb_map_init(&tv->index, a);
  load_atlas(tv, vfs, atlas);
  return tv;
}

void zb_terrain_view_destroy(zb_terrain_view *tv) {
  if (!tv) {
    return;
  }
  for (size_t i = 0; i < tv->entries.len; ++i) {
    zb_render_release_mesh(tv->r, ((cache_entry *)zb_array_at(&tv->entries, i))->mesh);
  }
  zb_array_free(&tv->entries);
  zb_array_free(&tv->requests);
  zb_map_free(&tv->index);
  zb_free(tv->a, tv);
}

int zb_terrain_view_lod(const zb_terrain_view *tv, float distance) {
  int lod = 0;
  while (lod < ZB_TERRAIN_LODS - 1 && distance >= tv->cfg.lod_distance[lod]) {
    lod++;
  }
  return lod;
}

/* ---- Mesh building --------------------------------------------------------------- */

static zb_vec3 v3sub(zb_vec3 a, zb_vec3 b) { return zb_v3_sub(a, b); }

zb_result zb_terrain_mesh_build(const zb_allocator *a, const zb_planet *p, const zb_chunk_terrain *ct, int lod,
                                const zb_vec3 colors[ZB_MAT_COUNT], const int layers[ZB_MAT_COUNT], zb_mesh_data *out) {
  memset(out, 0, sizeof *out);
  if (ct->stage < ZB_STAGE_COARSE || lod < 0 || lod >= ZB_TERRAIN_LODS) {
    return ZB_ERR_INVALID;
  }
  bool fine = lod <= 1 && ct->height != NULL;
  if (lod <= 1 && !fine) {
    lod = 2; /* no heightfield yet: the coarse grid */
  }
  int step = 4 << lod;
  int n = 256 / step + 1;
  bool water = ct->min_height < 0.5f;
  size_t grid = (size_t)n * (size_t)n;
  size_t vcount = grid + 4 * (size_t)n + (water ? WATER_GRID * WATER_GRID : 0);
  size_t icount = (size_t)(n - 1) * (size_t)(n - 1) * 6 + 4 * (size_t)(n - 1) * 12 +
                  (water ? (WATER_GRID - 1) * (WATER_GRID - 1) * 6 : 0);
  out->vertices = zb_alloc(a, vcount * sizeof(zb_vertex));
  out->indices = zb_alloc(a, icount * sizeof(uint32_t));
  if (!out->vertices || !out->indices) {
    zb_mesh_free(a, out);
    return ZB_ERR_NOMEM;
  }
  zb_origin frame;
  zb_origin_init(&frame, p, ct->chunk);
  zb_dvec3 fn, fu, fv;
  zb_planet_face_axes(ct->chunk.face, &fn, &fu, &fv);
  /* Positions in the chunk's frame. */
  zb_vertex *v = out->vertices;
  for (int j = 0; j < n; ++j) {
    double b =
        zb_dtan((-1.0 + ((double)ct->chunk.y + (double)(j * step) / ZB_CHUNK_SIZE) * p->chunk_uv) * (ZB_DPI / 4.0));
    for (int i = 0; i < n; ++i) {
      double ta =
          zb_dtan((-1.0 + ((double)ct->chunk.x + (double)(i * step) / ZB_CHUNK_SIZE) * p->chunk_uv) * (ZB_DPI / 4.0));
      double x = fn.x + ta * fu.x + b * fv.x, y = fn.y + ta * fu.y + b * fv.y, z = fn.z + ta * fu.z + b * fv.z;
      double inv = 1.0 / sqrt(x * x + y * y + z * z);
      float h = fine ? ct->height[(size_t)(j * step) * ZB_HEIGHT_RES + (size_t)(i * step)]
                     : zb_chunk_coarse_height(ct, i * step / 16, j * step / 16);
      double r = (p->radius + (double)h) * inv;
      zb_dvec3 c = {x * r, y * r, z * r};
      zb_vec3 l = zb_origin_from_cart(&frame, c);
      zb_vertex *vx = &v[(size_t)j * (size_t)n + (size_t)i];
      memcpy(vx->pos, &l, sizeof vx->pos);
      int mat;
      if (fine && ct->material) {
        int cx = i * step > 255 ? 255 : i * step, cy = j * step > 255 ? 255 : j * step;
        mat = ct->material[cy * ZB_SURFACE_RES + cx];
      } else {
        mat = ct->coarse_material[(j * step / 16) * ZB_COARSE_RES + i * step / 16];
      }
      mat = mat > ZB_MAT_NONE && mat < ZB_MAT_COUNT ? mat : ZB_MAT_SOIL;
      vx->color[0] = colors[mat].x;
      vx->color[1] = colors[mat].y;
      vx->color[2] = colors[mat].z;
      vx->color[3] = (lod == 0 && layers[mat] >= 0 ? (float)layers[mat] : UNTEXTURED) / 255.0f;
      vx->uv[0] = (float)(i * step) / 8.0f;
      vx->uv[1] = (float)(j * step) / 8.0f;
    }
  }
  /* Normals from neighbouring positions, pointing up. */
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      size_t row = (size_t)j * (size_t)n, nn = (size_t)n;
      const zb_vertex *xl = &v[row + (size_t)(i > 0 ? i - 1 : i)];
      const zb_vertex *xr = &v[row + (size_t)(i < n - 1 ? i + 1 : i)];
      const zb_vertex *yl = &v[(size_t)(j > 0 ? j - 1 : j) * nn + (size_t)i];
      const zb_vertex *yr = &v[(size_t)(j < n - 1 ? j + 1 : j) * nn + (size_t)i];
      zb_vec3 du = v3sub(zb_v3(xr->pos[0], xr->pos[1], xr->pos[2]), zb_v3(xl->pos[0], xl->pos[1], xl->pos[2]));
      zb_vec3 dv = v3sub(zb_v3(yr->pos[0], yr->pos[1], yr->pos[2]), zb_v3(yl->pos[0], yl->pos[1], yl->pos[2]));
      zb_vec3 nm = zb_v3_normalize(zb_v3_cross(du, dv));
      if (nm.y < 0.0f) {
        nm = zb_v3_scale(nm, -1.0f);
      }
      memcpy(v[row + (size_t)i].normal, &nm, sizeof nm);
    }
  }
  /* Winding: counter-clockwise seen from above. */
  const zb_vertex *a0 = &v[0], *a1 = &v[1], *a2 = &v[(size_t)n + 1u];
  zb_vec3 e1 = v3sub(zb_v3(a1->pos[0], a1->pos[1], a1->pos[2]), zb_v3(a0->pos[0], a0->pos[1], a0->pos[2]));
  zb_vec3 e2 = v3sub(zb_v3(a2->pos[0], a2->pos[1], a2->pos[2]), zb_v3(a0->pos[0], a0->pos[1], a0->pos[2]));
  bool flip = zb_v3_cross(e1, e2).y < 0.0f;
  uint32_t *ix = out->indices;
  size_t k = 0;
  for (int j = 0; j < n - 1; ++j) {
    for (int i = 0; i < n - 1; ++i) {
      uint32_t q0 = (uint32_t)(j * n + i), q1 = q0 + 1, q2 = q0 + (uint32_t)n + 1, q3 = q0 + (uint32_t)n;
      uint32_t tri[6] = {q0, q1, q2, q0, q2, q3};
      if (flip) {
        tri[1] = q2;
        tri[2] = q1;
        tri[4] = q3;
        tri[5] = q2;
      }
      memcpy(&ix[k], tri, sizeof tri);
      k += 6;
    }
  }
  /* Skirts: every edge vertex again, hanging down; quads in both windings. */
  float depth = 2.0f * (float)step + 8.0f;
  size_t vk = grid;
  for (int e = 0; e < 4; ++e) {
    size_t first = vk;
    for (int t = 0; t < n; ++t) {
      int i = e == 0 ? t : (e == 1 ? n - 1 : (e == 2 ? n - 1 - t : 0));
      int j = e == 0 ? 0 : (e == 1 ? t : (e == 2 ? n - 1 : n - 1 - t));
      v[vk] = v[(size_t)j * (size_t)n + (size_t)i];
      v[vk].pos[1] -= depth;
      vk++;
    }
    for (int t = 0; t < n - 1; ++t) {
      int i0 = e == 0 ? t : (e == 1 ? n - 1 : (e == 2 ? n - 1 - t : 0));
      int j0 = e == 0 ? 0 : (e == 1 ? t : (e == 2 ? n - 1 : n - 1 - t));
      int i1 = e == 0 ? t + 1 : (e == 1 ? n - 1 : (e == 2 ? n - 2 - t : 0));
      int j1 = e == 0 ? 0 : (e == 1 ? t + 1 : (e == 2 ? n - 1 : n - 2 - t));
      uint32_t top0 = (uint32_t)(j0 * n + i0), top1 = (uint32_t)(j1 * n + i1);
      uint32_t bot0 = (uint32_t)(first + (size_t)t), bot1 = bot0 + 1;
      uint32_t quad[12] = {top0, bot0, bot1, top0, bot1, top1, top0, bot1, bot0, top0, top1, bot1};
      memcpy(&ix[k], quad, sizeof quad);
      k += 12;
    }
  }
  /* Water at sea level over chunks that reach below it. */
  if (water) {
    size_t first = vk;
    for (int j = 0; j < WATER_GRID; ++j) {
      for (int i = 0; i < WATER_GRID; ++i) {
        zb_planet_pos wp = {ct->chunk, (float)(i * 64), (float)(j * 64), 0.0f};
        zb_vec3 l = zb_origin_to_local(&frame, p, &wp);
        zb_vertex *vx = &v[vk++];
        memset(vx, 0, sizeof *vx);
        memcpy(vx->pos, &l, sizeof vx->pos);
        vx->normal[1] = 1.0f;
        vx->color[0] = WATER_COLOR.x;
        vx->color[1] = WATER_COLOR.y;
        vx->color[2] = WATER_COLOR.z;
        vx->color[3] = UNTEXTURED / 255.0f;
      }
    }
    for (int j = 0; j < WATER_GRID - 1; ++j) {
      for (int i = 0; i < WATER_GRID - 1; ++i) {
        uint32_t q0 = (uint32_t)(first + (size_t)(j * WATER_GRID + i)), q1 = q0 + 1;
        uint32_t q2 = q0 + WATER_GRID + 1, q3 = q0 + WATER_GRID;
        uint32_t tri[6] = {q0, q1, q2, q0, q2, q3};
        if (flip) {
          tri[1] = q2;
          tri[2] = q1;
          tri[4] = q3;
          tri[5] = q2;
        }
        memcpy(&ix[k], tri, sizeof tri);
        k += 6;
      }
    }
  }
  out->vertex_count = vk;
  out->index_count = k;
  zb_mesh_compute_bounds(out);
  return ZB_OK;
}

/* ---- Per frame ------------------------------------------------------------------------ */

static cache_entry *find_entry(zb_terrain_view *tv, zb_chunk c) {
  uint64_t idx;
  return zb_map_get(&tv->index, zb_chunk_key(c), &idx) ? zb_array_at(&tv->entries, (size_t)idx) : NULL;
}

/* Visits a loaded chunk: marks its mesh as seen or asks for a (re)build. */
static void visit(void *ctx, const zb_chunk_terrain *ct, uint32_t version) {
  zb_terrain_view *tv = ctx;
  const zb_planet *p = zb_world_planet(tv->world);
  zb_dvec3 d = zb_planet_chunk_dir(p, ct->chunk);
  zb_dvec3 c = {d.x * p->radius, d.y * p->radius, d.z * p->radius};
  zb_vec3 l = zb_origin_from_cart(zb_world_origin(tv->world), c);
  float dist = zb_v3_length(zb_v3_sub(l, tv->eye));
  int lod = zb_terrain_view_lod(tv, dist);
  if (lod <= 1 && ct->stage < ZB_STAGE_TERRAIN) {
    lod = 2;
  }
  cache_entry *e = find_entry(tv, ct->chunk);
  if (e) {
    e->seen = tv->frame;
  }
  if (!e || e->lod != lod || e->version != version) {
    build_request req = {ct->chunk, ct, version, lod, dist};
    zb_array_push(&tv->requests, &req);
  }
}

static int cmp_request(const void *a, const void *b) {
  const build_request *x = a, *y = b;
  if (x->distance != y->distance) {
    return x->distance < y->distance ? -1 : 1;
  }
  uint64_t kx = zb_chunk_key(x->chunk), ky = zb_chunk_key(y->chunk);
  return kx < ky ? -1 : (kx > ky);
}

static void build(zb_terrain_view *tv, const build_request *req) {
  zb_mesh_data md;
  if (zb_terrain_mesh_build(tv->a, zb_world_planet(tv->world), req->ct, req->lod, tv->colors, tv->layers, &md) !=
      ZB_OK) {
    return;
  }
  uint32_t mesh = zb_render_upload_mesh(tv->r, &md);
  uint32_t count = (uint32_t)md.index_count;
  zb_vec3 center = md.center;
  float radius = md.radius;
  zb_mesh_free(tv->a, &md);
  if (!mesh) {
    return;
  }
  cache_entry *e = find_entry(tv, req->chunk);
  if (!e) {
    cache_entry fresh;
    memset(&fresh, 0, sizeof fresh);
    fresh.chunk = req->chunk;
    zb_origin_init(&fresh.frame, zb_world_planet(tv->world), req->chunk);
    e = zb_array_push(&tv->entries, &fresh);
    if (!e || zb_map_put(&tv->index, zb_chunk_key(req->chunk), tv->entries.len - 1) != ZB_OK) {
      if (e) {
        zb_array_pop(&tv->entries);
      }
      zb_render_release_mesh(tv->r, mesh);
      return;
    }
  }
  zb_render_release_mesh(tv->r, e->mesh);
  e->mesh = mesh;
  e->count = count;
  e->lod = req->lod;
  e->version = req->version;
  e->center = center;
  e->radius = radius;
  e->seen = tv->frame;
  tv->stats.built++;
}

void zb_terrain_view_draw(zb_terrain_view *tv, zb_world *w, zb_vec3 eye) {
  tv->frame++;
  tv->world = w;
  tv->eye = eye;
  memset(&tv->stats, 0, sizeof tv->stats);
  zb_array_clear(&tv->requests);
  zb_stream_each(zb_world_stream(w), visit, tv);
  /* Nearest first, within the per-frame budgets. */
  if (tv->requests.len > 1) {
    qsort(tv->requests.data, tv->requests.len, sizeof(build_request), cmp_request);
  }
  int fine = tv->cfg.fine_builds, coarse = tv->cfg.coarse_builds;
  for (size_t i = 0; i < tv->requests.len; ++i) {
    const build_request *req = zb_array_at(&tv->requests, i);
    int *budget = req->lod <= 1 ? &fine : &coarse;
    if (*budget > 0) {
      (*budget)--;
      build(tv, req);
    }
  }
  /* Draw what was seen; forget meshes of chunks no longer loaded. */
  const zb_origin *origin = zb_world_origin(w);
  uint32_t origin_version = zb_world_origin_version(w);
  for (size_t i = 0; i < tv->entries.len;) {
    cache_entry *e = zb_array_at(&tv->entries, i);
    if (e->seen != tv->frame) {
      zb_render_release_mesh(tv->r, e->mesh);
      zb_map_remove(&tv->index, zb_chunk_key(e->chunk));
      size_t last = tv->entries.len - 1;
      if (i != last) {
        cache_entry *moved = zb_array_at(&tv->entries, last);
        zb_map_put(&tv->index, zb_chunk_key(moved->chunk), i);
      }
      zb_array_remove_swap(&tv->entries, i);
      continue;
    }
    if (!e->transformed || e->origin_version != origin_version) {
      e->transform = zb_origin_transform(&e->frame, origin);
      e->origin_version = origin_version;
      e->transformed = true;
    }
    zb_render_draw_terrain(tv->r, e->mesh, e->count, e->transform, e->center, e->radius);
    tv->stats.drawn++;
    tv->stats.triangles += e->count / 3;
    tv->stats.per_lod[e->lod]++;
    ++i;
  }
  tv->stats.meshes = tv->entries.len;
}

void zb_terrain_view_get_stats(const zb_terrain_view *tv, zb_terrain_view_stats *out) { *out = tv->stats; }
