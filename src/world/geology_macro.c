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

static const int DX[8] = {1, -1, 0, 0, 1, -1, 1, -1};
static const int DY[8] = {0, 0, 1, -1, 1, 1, -1, -1};

static void fit_sea_level(zb_geology *g);

static size_t cell_count(const zb_geology *g) { return (size_t)6 * (size_t)g->macro.res * (size_t)g->macro.res; }

static double cell_uv(int res, double i) { return -1.0 + (i + 0.5) * 2.0 / (double)res; }

zb_dvec3 zb_geo_macro_dir(const zb_geology *g, int32_t cell) {
  int res = g->macro.res;
  int face = cell / (res * res), y = (cell / res) % res, x = cell % res;
  return zb_planet_face_dir(face, cell_uv(res, (double)x), cell_uv(res, (double)y));
}

static int32_t cell_at(int res, int face, double u, double v) {
  int x = (int)((u + 1.0) * 0.5 * (double)res), y = (int)((v + 1.0) * 0.5 * (double)res);
  x = x < 0 ? 0 : (x >= res ? res - 1 : x);
  y = y < 0 ? 0 : (y >= res ? res - 1 : y);
  return (int32_t)((face * res + y) * res + x);
}

/* Perimeter slot of an edge cell, -1 for interior cells. */
static int edge_slot(int res, int face, int x, int y) {
  int side;
  int t;
  if (y == 0) {
    side = 0;
    t = x;
  } else if (y == res - 1) {
    side = 1;
    t = x;
  } else if (x == 0) {
    side = 2;
    t = y;
  } else if (x == res - 1) {
    side = 3;
    t = y;
  } else {
    return -1;
  }
  return (face * 4 + side) * res + t;
}

static int32_t compute_nbr(int res, int face, int x, int y, int k) {
  int nx = x + DX[k], ny = y + DY[k];
  bool out_x = nx < 0 || nx >= res, out_y = ny < 0 || ny >= res;
  if (!out_x && !out_y) {
    return (int32_t)((face * res + ny) * res + nx);
  }
  if (out_x && out_y) {
    return -1; /* the missing diagonal at a cube corner */
  }
  zb_dvec3 d = zb_planet_face_dir(face, cell_uv(res, (double)nx), cell_uv(res, (double)ny));
  int f2 = zb_planet_dir_face(d);
  double u, v;
  zb_planet_dir_uv(f2, d, &u, &v);
  return cell_at(res, f2, u, v);
}

int32_t zb_geo_macro_nbr(const zb_geology *g, int32_t cell, int k) {
  int res = g->macro.res;
  int face = cell / (res * res), y = (cell / res) % res, x = cell % res;
  int slot = edge_slot(res, face, x, y);
  if (slot < 0) {
    return cell + DX[k] + DY[k] * res;
  }
  return g->edge_nbr[slot * 8 + k];
}

zb_result zb_geo_macro_alloc(zb_geology *g) {
  int res = g->opt.macro_res;
  g->macro.res = res;
  size_t cells = cell_count(g), rr = (size_t)res * (size_t)res;
  g->macro.height = zb_alloc(g->a, sizeof(float) * cells);
  g->macro.lake = zb_calloc(g->a, sizeof(float) * cells);
  g->macro.drainage = zb_alloc(g->a, sizeof(float) * cells);
  g->macro.receiver = zb_alloc(g->a, sizeof(int32_t) * cells);
  g->macro.point = zb_alloc(g->a, sizeof(int32_t) * cells);
  g->macro.area = zb_alloc(g->a, sizeof(float) * rr);
  g->edge_nbr = zb_alloc(g->a, sizeof(int32_t) * (size_t)24 * (size_t)res * 8);
  if (!g->macro.height || !g->macro.lake || !g->macro.drainage || !g->macro.receiver || !g->macro.point ||
      !g->macro.area || !g->edge_nbr) {
    zb_geo_macro_free(g);
    return ZB_ERR_NOMEM;
  }
  /* Cell areas from the solid angle of the equi-angular mapping. */
  double r_km = g->opt.radius / 1000.0, du = 2.0 / (double)res, q = ZB_GEO_PI / 4.0;
  for (int y = 0; y < res; ++y) {
    double b = zb_dtan(cell_uv(res, (double)y) * q);
    for (int x = 0; x < res; ++x) {
      double a = zb_dtan(cell_uv(res, (double)x) * q);
      double s = 1.0 + a * a + b * b;
      double omega = q * q * (1.0 + a * a) * (1.0 + b * b) / (s * sqrt(s)) * du * du;
      g->macro.area[y * res + x] = (float)(omega * r_km * r_km);
    }
  }
  for (int face = 0; face < 6; ++face) {
    for (int y = 0; y < res; ++y) {
      for (int x = 0; x < res; ++x) {
        int slot = edge_slot(res, face, x, y);
        if (slot < 0) {
          x = res - 2; /* skip the interior of the row */
          continue;
        }
        for (int k = 0; k < 8; ++k) {
          g->edge_nbr[slot * 8 + k] = compute_nbr(res, face, x, y, k);
        }
      }
    }
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
  zb_free(g->a, g->macro.area);
  zb_free(g->a, g->edge_nbr);
  memset(&g->macro, 0, sizeof g->macro);
  g->edge_nbr = NULL;
}

static float cell_area(const zb_geology *g, int32_t cell) {
  int res = g->macro.res;
  return g->macro.area[cell % (res * res)];
}

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
  int res = m->res;
  int face = zb_planet_dir_face(dir);
  double u, v;
  zb_planet_dir_uv(face, dir, &u, &v);
  double fx = (u + 1.0) * 0.5 * (double)res - 0.5, fy = (v + 1.0) * 0.5 * (double)res - 0.5;
  fx = fx < 0.0 ? 0.0 : (fx > (double)(res - 1) ? (double)(res - 1) : fx);
  fy = fy < 0.0 ? 0.0 : (fy > (double)(res - 1) ? (double)(res - 1) : fy);
  int x0 = (int)fx, y0 = (int)fy;
  int x1 = x0 + 1 < res ? x0 + 1 : x0, y1 = y0 + 1 < res ? y0 + 1 : y0;
  double tx = fx - (double)x0, ty = fy - (double)y0;
  const float *h = m->height + (size_t)face * (size_t)res * (size_t)res;
  double a = (double)h[y0 * res + x0] * (1.0 - tx) + (double)h[y0 * res + x1] * tx;
  double b = (double)h[y1 * res + x0] * (1.0 - tx) + (double)h[y1 * res + x1] * tx;
  return a * (1.0 - ty) + b * ty;
}
