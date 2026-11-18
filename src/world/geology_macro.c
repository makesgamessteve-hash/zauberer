/* SPDX-License-Identifier: MIT */
/* Deep-time geology, macro grid: the final lattice state amplified onto a
 * cube-sphere grid of about 6 km cells with feature-aware detail, then eroded
 * by stream power and hillslope diffusion, with water routed over a
 * priority-flood (depressions become lakes). */
#include "world/geology_internal.h"

#include <string.h>

#include "world/dvec.h"
#include "world/heap.h"
#include "world/noise.h"

#define STREAM_POWER 0.0025 /* per pass, per sqrt(km^2) of drainage, per km of distance */
#define DIFFUSION 0.12      /* hillslope smoothing per pass */
#define FLOOD_STEP 0.01f    /* metres added along filled flats so water keeps flowing */
#define LAKE_INFILL 0.4     /* share of a lake's depth filled by sediment per pass */

static void fit_sea_level(zb_geology *g);

static size_t cell_count(const zb_geology *g) { return g->grid.cells; }

zb_dvec3 zb_geo_macro_dir(const zb_geology *g, int32_t cell) { return zb_cubegrid_dir(&g->grid, cell); }

int32_t zb_geo_macro_nbr(const zb_geology *g, int32_t cell, int k) { return zb_cubegrid_nbr(&g->grid, cell, k); }

zb_result zb_geo_macro_alloc(zb_geology *g) {
  int res = g->opt.macro_res;
  g->macro.res = res;
  if (zb_cubegrid_init(&g->grid, g->a, res, g->opt.radius / 1000.0) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  size_t cells = cell_count(g);
  g->macro.height = zb_alloc(g->a, sizeof(float) * cells);
  g->macro.lake = zb_calloc(g->a, sizeof(float) * cells);
  g->macro.drainage = zb_alloc(g->a, sizeof(float) * cells);
  g->macro.receiver = zb_alloc(g->a, sizeof(int32_t) * cells);
  g->macro.point = zb_alloc(g->a, sizeof(int32_t) * cells);
  g->macro.area = g->grid.area;
  if (!g->macro.height || !g->macro.lake || !g->macro.drainage || !g->macro.receiver || !g->macro.point) {
    zb_geo_macro_free(g);
    return ZB_ERR_NOMEM;
  }
  g->macro_rows_done = 0;
  return ZB_OK;
}

void zb_geo_macro_free(zb_geology *g) {
  zb_free(g->a, g->macro.height);
  zb_free(g->a, g->macro.lake);
  zb_free(g->a, g->macro.drainage);
  zb_free(g->a, g->macro.receiver);
  zb_free(g->a, g->macro.point);
  zb_cubegrid_free(&g->grid);
  memset(&g->macro, 0, sizeof g->macro);
}

static float cell_area(const zb_geology *g, int32_t cell) { return zb_cubegrid_area(&g->grid, cell); }

/* ---- Amplification ------------------------------------------------------------------ */

static double youth(float orogeny) {
  if (orogeny < 0.0f) {
    return 0.35;
  }
  return orogeny < 80.0f ? 1.0 : (orogeny < 250.0f ? 0.7 : 0.45);
}

static float amplify_cell(zb_geology *g, zb_dvec3 d, int j) {
  const zb_lattice *l = &g->lat;
  double reach = 1.6 * l->spacing, wsum = 0.0, hsum = 0.0;
  for (int k = -1; k < ZB_LATTICE_K; ++k) {
    int m = k < 0 ? j : zb_lattice_nbr(l, j, k);
    if (m < 0) {
      continue;
    }
    double t = 1.0 - zb_dv_angle(d, l->p[m]) / reach;
    if (t > 0.0) {
      double w = t * t;
      wsum += w;
      hsum += w * (double)g->elevation[m];
    }
  }
  double h = wsum > 0.0 ? hsum / wsum : (double)g->elevation[j];
  double scale = g->opt.radius / 1000.0; /* km per unit of direction */
  uint64_t s = g->seed;
  /* Mountains: ridged detail, sharper where the range is young. */
  double mount = (h - 200.0) / 2000.0;
  mount = mount < 0.0 ? 0.0 : (mount > 1.0 ? 1.0 : mount);
  if (mount > 0.0) {
    double f = scale / 120.0;
    double r = zb_ridged3(zb_hash_combine(s, 11), d.x * f, d.y * f, d.z * f, 5);
    h += (r - 0.45) * 1800.0 * mount * youth(g->cur.orogeny[j]);
  }
  /* Broad relief and ragged coasts. */
  double f2 = scale / 400.0, f3 = scale / 60.0;
  h += zb_fbm3(zb_hash_combine(s, 12), d.x * f2, d.y * f2, d.z * f2, 4, 0.5) * (h > 0.0 ? 180.0 : 300.0);
  double near_coast = h / 400.0;
  h += zb_fbm3(zb_hash_combine(s, 13), d.x * f3, d.y * f3, d.z * f3, 4, 0.55) * 320.0 *
       zb_dexp(-(near_coast < 0.0 ? -near_coast : near_coast));
  /* Volcanic cones on arcs and hotspots that stand above the sea. */
  if ((g->cur.flags[j] & (ZB_GEO_ACTIVE_VOLCANO | ZB_GEO_HOTSPOT)) && g->elevation[j] > -500.0f) {
    double t = 1.0 - zb_dv_angle(d, l->p[j]) / (0.5 * l->spacing);
    if (t > 0.0) {
      h += 1200.0 * t * t;
    }
  }
  return (float)h;
}

bool zb_geo_macro_amplify(zb_geology *g, int rows) {
  int res = g->macro.res, total = 6 * res;
  int hint = -1;
  for (int r = 0; r < rows && g->macro_rows_done < total; ++r, ++g->macro_rows_done) {
    int face = g->macro_rows_done / res, y = g->macro_rows_done % res;
    for (int x = 0; x < res; ++x) {
      int32_t cell = (int32_t)((face * res + y) * res + x);
      zb_dvec3 d = zb_geo_macro_dir(g, cell);
      int j = zb_lattice_nearest(&g->lat, d, hint);
      hint = j;
      g->macro.point[cell] = j;
      g->macro.height[cell] = amplify_cell(g, d, j);
    }
  }
  if (g->macro_rows_done < total) {
    return false;
  }
  fit_sea_level(g); /* erosion works against the present sea */
  return true;
}

/* ---- Routing and erosion -------------------------------------------------------------- */

typedef struct flood {
  float *filled;
  int32_t *order;
  uint8_t *diag;
  uint8_t *ocean; /* 1 for cells of seas large enough to be the ocean */
  size_t count;
} flood;

/* Marks the ocean: connected cells at or below sea level, in seas of at least
 * 1/1000 of the planet (smaller basins below sea level are dry depressions). */
static void mark_ocean(zb_geology *g, flood *f) {
  size_t cells = cell_count(g), min_size = cells / 1000 + 1;
  float *h = g->macro.height;
  int32_t *queue = f->order; /* scratch until the flood fills it */
  memset(f->ocean, 0, cells);
  for (size_t s = 0; s < cells; ++s) {
    if (h[s] > 0.0f || f->ocean[s]) {
      continue;
    }
    size_t head = 0, tail = 0;
    queue[tail++] = (int32_t)s;
    f->ocean[s] = 2; /* visited */
    while (head < tail) {
      int32_t c = queue[head++];
      for (int k = 0; k < 4; ++k) {
        int32_t nb = zb_geo_macro_nbr(g, c, k);
        if (nb >= 0 && !f->ocean[nb] && h[nb] <= 0.0f) {
          f->ocean[nb] = 2;
          queue[tail++] = nb;
        }
      }
    }
    uint8_t mark = tail >= min_size ? 1 : 3; /* 3: visited, not ocean */
    for (size_t q = 0; q < tail; ++q) {
      f->ocean[queue[q]] = mark;
    }
  }
  for (size_t c = 0; c < cells; ++c) {
    f->ocean[c] = f->ocean[c] == 1;
  }
}

/* Priority-flood from the sea: receivers point downhill towards the sea (over
 * filled depressions), order lists cells outlet first. */
static bool priority_flood(zb_geology *g, flood *f) {
  size_t cells = cell_count(g);
  float *h = g->macro.height;
  int32_t *recv = g->macro.receiver;
  zb_heap heap;
  zb_heap_init(&heap, g->a);
  if (!zb_heap_reserve(&heap, cells / 4 + 64)) {
    return false;
  }
  for (size_t c = 0; c < cells; ++c) {
    recv[c] = -2; /* unvisited */
  }
  mark_ocean(g, f);
  int32_t lowest = 0;
  bool any_sea = false;
  for (size_t c = 0; c < cells; ++c) {
    if (f->ocean[c]) {
      any_sea = true;
      recv[c] = -1;
      f->filled[c] = h[c];
      zb_heap_push(&heap, (double)h[c], (int32_t)c, 0);
    } else if (h[c] < h[lowest]) {
      lowest = (int32_t)c;
    }
  }
  if (!any_sea) {
    recv[lowest] = -1;
    f->filled[lowest] = h[lowest];
    zb_heap_push(&heap, (double)h[lowest], lowest, 0);
  }
  f->count = 0;
  zb_heap_item it;
  while (zb_heap_pop(&heap, &it)) {
    f->order[f->count++] = it.id;
    for (int k = 0; k < 8; ++k) {
      int32_t nb = zb_geo_macro_nbr(g, it.id, k);
      if (nb < 0 || recv[nb] != -2) {
        continue;
      }
      recv[nb] = it.id;
      f->diag[nb] = k >= 4;
      float base = f->filled[it.id] + FLOOD_STEP;
      f->filled[nb] = h[nb] > base ? h[nb] : base;
      zb_heap_push(&heap, (double)f->filled[nb], nb, 0);
    }
  }
  bool ok = !heap.failed;
  zb_heap_free(&heap);
  return ok;
}

static void accumulate(zb_geology *g, const flood *f) {
  float *dr = g->macro.drainage;
  for (size_t i = 0; i < f->count; ++i) {
    dr[f->order[i]] = cell_area(g, f->order[i]);
  }
  for (size_t i = f->count; i-- > 0;) {
    int32_t c = f->order[i], r = g->macro.receiver[c];
    if (r >= 0) {
      dr[r] += dr[c];
    }
  }
}

static bool flood_alloc(zb_geology *g, flood *f) {
  size_t cells = cell_count(g);
  f->filled = zb_alloc(g->a, sizeof(float) * cells);
  f->order = zb_alloc(g->a, sizeof(int32_t) * cells);
  f->diag = zb_alloc(g->a, cells);
  f->ocean = zb_alloc(g->a, cells);
  f->count = 0;
  return f->filled && f->order && f->diag && f->ocean;
}

static void flood_free(zb_geology *g, flood *f) {
  zb_free(g->a, f->filled);
  zb_free(g->a, f->order);
  zb_free(g->a, f->diag);
  zb_free(g->a, f->ocean);
}

static void fit_sea_level(zb_geology *g) {
  size_t cells = cell_count(g);
  float *weights = zb_alloc(g->a, sizeof(float) * cells);
  if (!weights) {
    return;
  }
  for (size_t c = 0; c < cells; ++c) {
    weights[c] = cell_area(g, (int32_t)c);
  }
  float sea = zb_geo_level_for_fraction(g->hist, g->macro.height, weights, cells, g->opt.land_fraction);
  zb_free(g->a, weights);
  for (size_t c = 0; c < cells; ++c) {
    g->macro.height[c] -= sea;
  }
}

void zb_geo_macro_erode(zb_geology *g) {
  flood f;
  if (!flood_alloc(g, &f) || !priority_flood(g, &f)) {
    flood_free(g, &f);
    return;
  }
  accumulate(g, &f);
  float *h = g->macro.height;
  /* Stream power, implicit (Braun and Willett), outlets first. */
  for (size_t i = 0; i < f.count; ++i) {
    int32_t c = f.order[i], r = g->macro.receiver[c];
    if (f.ocean[c]) {
      continue; /* the sea does not incise */
    }
    if (f.filled[c] > h[c] + FLOOD_STEP * 2.0f) { /* lakes silt up */
      h[c] += (float)LAKE_INFILL * (f.filled[c] - h[c]);
      continue;
    }
    if (r < 0) {
      continue;
    }
    /* Rivers grade to the level they flow into: a lake's surface, not its floor. */
    float base = f.filled[r] > h[r] + FLOOD_STEP * 2.0f ? f.filled[r] : h[r];
    if (h[c] <= base) {
      continue;
    }
    double dist = sqrt((double)cell_area(g, c)) * (f.diag[c] ? 1.41421356 : 1.0);
    double k = STREAM_POWER * sqrt((double)g->macro.drainage[c]) / dist;
    double nh = ((double)h[c] + k * (double)base) / (1.0 + k);
    h[c] = (float)nh;
  }
  /* Hillslope diffusion on land. */
  size_t cells = cell_count(g);
  float *out = f.filled; /* reused as scratch */
  for (size_t c = 0; c < cells; ++c) {
    out[c] = h[c];
    if (f.ocean[c]) {
      continue;
    }
    double sum = 0.0;
    int count = 0;
    for (int k = 0; k < 4; ++k) {
      int32_t nb = zb_geo_macro_nbr(g, (int32_t)c, k);
      if (nb >= 0) {
        sum += (double)h[nb];
        count++;
      }
    }
    if (count > 0) {
      double nh = (double)h[c] + DIFFUSION * (sum / count - (double)h[c]);
      out[c] = (float)(h[c] > 0.0f && nh < 0.5 ? 0.5 : nh); /* land stays land */
    }
  }
  memcpy(h, out, sizeof(float) * cells);
  flood_free(g, &f);
}

void zb_geo_macro_route(zb_geology *g) {
  fit_sea_level(g);
  flood f;
  size_t cells = cell_count(g);
  if (!flood_alloc(g, &f) || !priority_flood(g, &f)) {
    flood_free(g, &f);
    return;
  }
  accumulate(g, &f);
  for (size_t c = 0; c < cells; ++c) {
    float depth = f.filled[c] - g->macro.height[c];
    g->macro.lake[c] = !f.ocean[c] && depth > 1.0f ? depth : 0.0f;
  }
  flood_free(g, &f);
}

/* ---- Sampling ------------------------------------------------------------------------ */

double zb_geology_height(const zb_geology *g, zb_dvec3 dir) {
  const zb_geology_macro *m = zb_geology_get_macro(g);
  if (!m) {
    return (double)g->elevation[zb_lattice_nearest(&g->lat, dir, -1)];
  }
  return zb_cubegrid_sample(&g->grid, m->height, dir);
}

const zb_cubegrid *zb_geology_macro_grid(const zb_geology *g) { return zb_geology_get_macro(g) ? &g->grid : NULL; }
