/* SPDX-License-Identifier: MIT */
#include "world/climate.h"

#include <stdio.h>
#include <string.h>

#include "core/dmath.h"
#include "core/hash.h"
#include "world/dvec.h"
#include "world/geology.h"
#include "world/heap.h"

#define PI 3.141592653589793238
#define SOLAR_CONSTANT 1361.0
#define LAPSE 0.0065         /* deg C per metre */
#define MOISTURE_PASSES 128  /* cells moisture travels per month */
#define RIVER_DISCHARGE 8.0f /* m^3/s: smaller flows are creeks below the grid */
#define GREAT_RIVER 2000.0f  /* m^3/s */
#define SECONDS_PER_YEAR 31557600.0

struct zb_climate {
  const zb_allocator *a;
  zb_climate_input in;
  const zb_cubegrid *macro;
  zb_cubegrid grid; /* climate grid */
  int ratio;        /* macro cells per climate cell edge */
  zb_climate_phase phase;
  int month;
  /* Climate grid. */
  zb_dvec3 *dir;
  float *height; /* mean land height */
  float *land;   /* land share */
  uint8_t *ocean;
  float *coast_km; /* distance to the ocean (land) */
  float *current;  /* sea-surface anomaly from currents (ocean) or its echo on land */
  float *temp;     /* sea-level temperature per month: ZB_MONTHS * cells */
  float *precip;   /* mm per month: ZB_MONTHS * cells */
  float *annual_t, *annual_p, *warmest, *dry_share;
  /* Macro grid. */
  uint8_t *biome;
  uint16_t *flags;
  float *discharge;
  uint8_t *order;
  float *groundwater;
  uint8_t *province;
  zb_province *provinces;
  int province_count;
  int magical_patches;
  /* Scratch. */
  int32_t *upwind;
  float *q, *q2, *rate, *evap;
};

static const char *const BIOME_NAMES[ZB_BIOME_COUNT] = {"none",
                                                        "deep ocean",
                                                        "continental shelf",
                                                        "coral reef",
                                                        "kelp forest",
                                                        "sea ice",
                                                        "lake",
                                                        "salt lake",
                                                        "river",
                                                        "beach",
                                                        "dunes",
                                                        "cliffs",
                                                        "mangroves",
                                                        "salt marsh",
                                                        "estuary",
                                                        "rainforest",
                                                        "seasonal forest",
                                                        "cloud forest",
                                                        "savanna",
                                                        "sand desert",
                                                        "rocky desert",
                                                        "salt flat",
                                                        "xeric shrubland",
                                                        "mediterranean scrub",
                                                        "steppe",
                                                        "deciduous forest",
                                                        "mixed forest",
                                                        "temperate rainforest",
                                                        "taiga",
                                                        "tundra",
                                                        "polar desert",
                                                        "ice sheet",
                                                        "montane forest",
                                                        "alpine meadow",
                                                        "scree and cliffs",
                                                        "glacier",
                                                        "volcanic slopes",
                                                        "marsh",
                                                        "swamp forest",
                                                        "peat bog",
                                                        "floodplain",
                                                        "ashlands",
                                                        "stormspires",
                                                        "glasswood",
                                                        "wildheart",
                                                        "drowned vale",
                                                        "deepstone",
                                                        "the hollows",
                                                        "mirefen"};

const char *zb_biome_name(zb_biome b) { return (int)b >= 0 && b < ZB_BIOME_COUNT ? BIOME_NAMES[b] : "?"; }

zb_biome_group zb_biome_group_of(zb_biome b) {
  if (b <= ZB_BIOME_RIVER) {
    return ZB_GROUP_AQUATIC;
  }
  if (b <= ZB_BIOME_ESTUARY) {
    return ZB_GROUP_COASTAL;
  }
  if (b <= ZB_BIOME_SAVANNA) {
    return ZB_GROUP_TROPICAL;
  }
  if (b <= ZB_BIOME_XERIC_SHRUBLAND) {
    return ZB_GROUP_ARID;
  }
  if (b <= ZB_BIOME_TEMPERATE_RAINFOREST) {
    return ZB_GROUP_TEMPERATE;
  }
  if (b <= ZB_BIOME_ICE_SHEET) {
    return ZB_GROUP_COLD;
  }
  if (b <= ZB_BIOME_VOLCANIC_SLOPES) {
    return ZB_GROUP_MOUNTAIN;
  }
  if (b <= ZB_BIOME_FLOODPLAIN) {
    return ZB_GROUP_WETLAND;
  }
  return ZB_GROUP_MAGICAL;
}

double zb_insolation(double latitude, double declination) {
  double t = -zb_dtan(latitude) * zb_dtan(declination);
  double h0 = t >= 1.0 ? 0.0 : (t <= -1.0 ? PI : zb_dacos(t)); /* polar night / midnight sun */
  return SOLAR_CONSTANT / PI *
         (h0 * zb_dsin(latitude) * zb_dsin(declination) + zb_dcos(latitude) * zb_dcos(declination) * zb_dsin(h0));
}

zb_biome zb_biome_classify(double t, double p, double warmest, double dry_half_share, double rocky) {
  if (t < -10.0) {
    return p < 250.0 ? ZB_BIOME_POLAR_DESERT : ZB_BIOME_ICE_SHEET;
  }
  if (warmest < 10.0) {
    return ZB_BIOME_TUNDRA;
  }
  if (t < 3.0) {
    return p < 250.0 ? ZB_BIOME_TUNDRA : ZB_BIOME_TAIGA;
  }
  if (p < 250.0) {
    return rocky > 0.5 ? ZB_BIOME_ROCKY_DESERT : ZB_BIOME_SAND_DESERT;
  }
  if (t < 20.0) {
    if (p < 500.0) {
      return t > 15.0 ? ZB_BIOME_XERIC_SHRUBLAND : ZB_BIOME_STEPPE;
    }
    if (t > 11.0 && p < 1000.0 && dry_half_share < 0.25) {
      return ZB_BIOME_MEDITERRANEAN; /* dry summers */
    }
    if (p > 2500.0) {
      return ZB_BIOME_TEMPERATE_RAINFOREST;
    }
    return t > 8.0 ? ZB_BIOME_DECIDUOUS_FOREST : ZB_BIOME_MIXED_FOREST;
  }
  if (p < 600.0) {
    return ZB_BIOME_XERIC_SHRUBLAND;
  }
  if (p < 1200.0) {
    return ZB_BIOME_SAVANNA;
  }
  if (p < 2000.0 || dry_half_share < 0.2) {
    return ZB_BIOME_SEASONAL_FOREST;
  }
  return ZB_BIOME_RAINFOREST;
}

/* ---- Climate grid ------------------------------------------------------------------ */

static int32_t climate_cell_of_macro(const zb_climate *c, int32_t m) {
  int res = c->macro->res, rc = c->grid.res;
  int face = m / (res * res), y = (m / res) % res, x = m % res;
  int cx = x * rc / res, cy = y * rc / res;
  return (int32_t)((face * rc + cy) * rc + cx);
}

static bool macro_ocean(const zb_climate *c, size_t m) {
  return c->in.height[m] <= 0.0f && (!c->in.lake || c->in.lake[m] <= 0.0f);
}

static void downsample(zb_climate *c) {
  size_t cells = c->grid.cells;
  float *count = c->q; /* scratch */
  memset(c->height, 0, sizeof(float) * cells);
  memset(c->land, 0, sizeof(float) * cells);
  memset(count, 0, sizeof(float) * cells);
  for (size_t m = 0; m < c->macro->cells; ++m) {
    int32_t k = climate_cell_of_macro(c, (int32_t)m);
    count[k] += 1.0f;
    if (!macro_ocean(c, m)) {
      c->land[k] += 1.0f;
      c->height[k] += c->in.height[m] > 0.0f ? c->in.height[m] : 0.0f;
    }
  }
  for (size_t k = 0; k < cells; ++k) {
    c->height[k] = c->land[k] > 0.0f ? c->height[k] / c->land[k] : 0.0f;
    c->land[k] = count[k] > 0.0f ? c->land[k] / count[k] : 0.0f;
    c->ocean[k] = c->land[k] < 0.5f;
    c->dir[k] = zb_cubegrid_dir(&c->grid, (int32_t)k);
  }
}

static void local_frame(zb_dvec3 d, zb_dvec3 *east, zb_dvec3 *north) {
  zb_dvec3 e = zb_dv_cross(zb_dv(0, 0, 1), d);
  if (zb_dv_len(e) < 1e-9) {
    e = zb_dv(1, 0, 0); /* at a pole any east will do */
  }
  *east = zb_dv_norm(e);
  *north = zb_dv_cross(d, *east);
}

/* Ocean currents: cold along the eastern sides of subtropical oceans (west
 * coasts), warm along their western sides; the reverse in subpolar gyres. */
static void ocean_currents(zb_climate *c) {
  double step = 1.0 / (double)c->grid.res; /* about a cell's angle */
  double reach_km = 700.0, km_per_step = step * 1.5708 * c->in.radius / 1000.0;
  int steps = (int)(reach_km / km_per_step) + 1;
  for (size_t k = 0; k < c->grid.cells; ++k) {
    c->current[k] = 0.0f;
    if (!c->ocean[k]) {
      continue;
    }
    zb_dvec3 d = c->dir[k], e, n;
    local_frame(d, &e, &n);
    double lat = zb_dasin(d.z) * 180.0 / PI, alat = lat < 0.0 ? -lat : lat;
    double land_east = -1.0, land_west = -1.0;
    for (int s = 1; s <= steps && (land_east < 0.0 || land_west < 0.0); ++s) {
      for (int side = 0; side < 2; ++side) {
        if ((side == 0 && land_east >= 0.0) || (side == 1 && land_west >= 0.0)) {
          continue;
        }
        zb_dvec3 p = zb_dv_norm(zb_dv_add(d, zb_dv_scale(e, (side == 0 ? 1.0 : -1.0) * step * 1.5708 * s)));
        if (!c->ocean[zb_cubegrid_cell_at(&c->grid, p)]) {
          *(side == 0 ? &land_east : &land_west) = (double)s * km_per_step;
        }
      }
    }
    double near_east = land_east >= 0.0 ? 1.0 - land_east / reach_km : 0.0;
    double near_west = land_west >= 0.0 ? 1.0 - land_west / reach_km : 0.0;
    if (alat > 8.0 && alat < 40.0) {
      c->current[k] = (float)(-6.0 * near_east + 3.0 * near_west);
    } else if (alat > 45.0 && alat < 68.0) {
      c->current[k] = (float)(4.0 * near_east - 3.0 * near_west);
    }
  }
}

/* Distance from the ocean over land, carrying the coastal current anomaly inland. */
static void coast_distance(zb_climate *c) {
  size_t cells = c->grid.cells;
  int32_t *queue = c->upwind; /* scratch */
  float cell_km = (float)sqrt((double)zb_cubegrid_area(&c->grid, 0));
  size_t head = 0, tail = 0;
  for (size_t k = 0; k < cells; ++k) {
    c->coast_km[k] = c->ocean[k] ? 0.0f : -1.0f;
    if (c->ocean[k]) {
      queue[tail++] = (int32_t)k;
    }
  }
  while (head < tail) {
    int32_t k = queue[head++];
    for (int j = 0; j < 4; ++j) {
      int32_t nb = zb_cubegrid_nbr(&c->grid, k, j);
      if (nb >= 0 && c->coast_km[nb] < 0.0f) {
        c->coast_km[nb] = c->coast_km[k] + cell_km;
        c->current[nb] = c->current[k] * 0.8f; /* the sea's warmth or chill fades inland */
        queue[tail++] = nb;
      }
    }
  }
  for (size_t k = 0; k < cells; ++k) {
    if (c->coast_km[k] < 0.0f) {
      c->coast_km[k] = 5000.0f; /* a world without ocean */
    }
  }
}

static double declination(const zb_climate *c, int month) {
  double tilt = c->in.axial_tilt * PI / 180.0;
  return tilt * zb_dsin(2.0 * PI * ((double)month + 0.5 - 2.7) / 12.0);
}

/* Prevailing wind (the direction the air moves) from the three-cell model,
 * shifted with the thermal equator. */
static zb_dvec3 wind(zb_dvec3 d, double shift) {
  zb_dvec3 e, n;
  local_frame(d, &e, &n);
  double lat = zb_dasin(d.z) * 180.0 / PI - shift;
  double s = lat < 0.0 ? -1.0 : 1.0, a = lat * s;
  if (a < 30.0) {
    return zb_dv_add(zb_dv_scale(e, -0.9), zb_dv_scale(n, -0.4 * s)); /* trade winds */
  }
  if (a < 60.0) {
    return zb_dv_add(zb_dv_scale(e, 0.9), zb_dv_scale(n, 0.4 * s)); /* westerlies */
  }
  return zb_dv_add(zb_dv_scale(e, -0.8), zb_dv_scale(n, -0.4 * s)); /* polar easterlies */
}

static void compute_month(zb_climate *c, int month) {
  size_t cells = c->grid.cells;
  double dec = declination(c, month), shift = 0.5 * dec * 180.0 / PI;
  double q_now[ZB_MONTHS];
  float *t = c->temp + (size_t)month * cells, *p = c->precip + (size_t)month * cells;
  for (size_t k = 0; k < cells; ++k) {
    zb_dvec3 d = c->dir[k];
    double lat = zb_dasin(d.z);
    for (int m = 0; m < ZB_MONTHS; ++m) {
      q_now[m] = zb_insolation(lat, declination(c, m));
    }
    double qa = 0.0;
    for (int m = 0; m < ZB_MONTHS; ++m) {
      qa += q_now[m] / ZB_MONTHS;
    }
    int m1 = (month + 11) % 12, m2 = (month + 10) % 12;
    /* Radiative balance plus the heat winds and currents carry poleward. */
    double base = -48.0 + 0.18 * qa + 8.0 * d.z * d.z + c->in.temperature_offset;
    if (c->ocean[k]) { /* the sea lags two months and swings little */
      double lag = 0.5 * (q_now[m1] + q_now[m2]);
      t[k] = (float)(base + 0.15 * 0.18 * (lag - qa) + (double)c->current[k]);
    } else {
      double cont = (double)c->coast_km[k] / 1200.0;
      cont = cont > 1.0 ? 1.0 : cont;
      double lag = 0.5 * (q_now[month] + q_now[m1]);
      double swing = (0.35 + 0.35 * cont) * (1.0 - 0.5 * d.z * d.z); /* snow and ice blunt polar summers */
      t[k] = (float)(base + swing * 0.18 * (lag - qa) + (double)c->current[k]);
    }
  }
  /* Upwind neighbour of every cell for this month's winds. */
  for (size_t k = 0; k < cells; ++k) {
    zb_dvec3 w = wind(c->dir[k], shift);
    int32_t best = -1;
    double best_dot = -1e9;
    for (int j = 0; j < ZB_CUBEGRID_NBRS; ++j) {
      int32_t nb = zb_cubegrid_nbr(&c->grid, (int32_t)k, j);
      if (nb < 0) {
        continue;
      }
      zb_dvec3 tdir = zb_dv_norm(zb_dv_sub(c->dir[nb], c->dir[k]));
      double dot = -zb_dv_dot(tdir, w);
      if (dot > best_dot) {
        best_dot = dot;
        best = nb;
      }
    }
    c->upwind[k] = best;
  }
  /* Evaporation and rain-out rates. */
  for (size_t k = 0; k < cells; ++k) {
    double temp = (double)t[k] - (c->ocean[k] ? 0.0 : LAPSE * (double)c->height[k]);
    double f = (temp + 5.0) / 30.0;
    f = f < 0.15 ? 0.15 : (f > 1.2 ? 1.2 : f); /* even cold seas give some vapour */
    c->evap[k] = c->ocean[k] ? (float)(150.0 * f) : 0.0f;
    double lat = zb_dasin(c->dir[k].z) * 180.0 / PI - shift, alat = lat < 0.0 ? -lat : lat;
    double r = (c->ocean[k] ? 0.08 : 0.06) + 0.08 * (temp > 15.0 ? (temp > 30.0 ? 1.0 : (temp - 15.0) / 15.0) : 0.0);
    if (alat < 8.0) {
      r *= 2.0; /* the intertropical convergence */
    } else if (alat > 15.0 && alat < 32.0) {
      r *= 0.25; /* subtropical highs */
    } else if (alat > 70.0) {
      r *= 0.6;
    }
    int32_t up = c->upwind[k];
    if (!c->ocean[k] && up >= 0) {
      double rise = (double)c->height[k] - (double)(c->ocean[up] ? 0.0f : c->height[up]);
      if (rise > 0.0) {
        r += 0.4 * rise / 1000.0; /* orographic lift */
      } else {
        double sink = 1.0 + rise / 1500.0; /* descending air dries */
        r *= sink < 0.3 ? 0.3 : sink;
      }
      if (c->height[k] > 3000.0f) {
        r += 0.1;
      }
    }
    c->rate[k] = (float)(r < 0.01 ? 0.01 : (r > 0.95 ? 0.95 : r));
    c->q[k] = c->evap[k] / 0.1f;
    p[k] = 0.0f;
  }
  for (int pass = 0; pass < MOISTURE_PASSES; ++pass) {
    for (size_t k = 0; k < cells; ++k) {
      int32_t up = c->upwind[k];
      float in = up >= 0 ? c->q[up] : c->q[k];
      float rain = in * c->rate[k];
      p[k] = rain;
      float recycled = c->ocean[k] ? c->evap[k] : 0.4f * rain; /* plants and soil give water back */
      c->q2[k] = in - rain + recycled;
    }
    float *tmp = c->q;
    c->q = c->q2;
    c->q2 = tmp;
  }
}

static void annual_summary(zb_climate *c) {
  size_t cells = c->grid.cells;
  for (size_t k = 0; k < cells; ++k) {
    double ts = 0.0, ps = 0.0, warm = -1e9;
    float months[ZB_MONTHS];
    for (int m = 0; m < ZB_MONTHS; ++m) {
      double tm = (double)c->temp[(size_t)m * cells + k];
      ts += tm;
      warm = tm > warm ? tm : warm;
      months[m] = c->precip[(size_t)m * cells + k];
      ps += (double)months[m];
    }
    /* Share of the rain in the driest six months (insertion sort of 12). */
    for (int i = 1; i < ZB_MONTHS; ++i) {
      float v = months[i];
      int j = i - 1;
      while (j >= 0 && months[j] > v) {
        months[j + 1] = months[j];
        --j;
      }
      months[j + 1] = v;
    }
    double dry = 0.0;
    for (int m = 0; m < 6; ++m) {
      dry += (double)months[m];
    }
    c->annual_t[k] = (float)(ts / ZB_MONTHS);
    c->annual_p[k] = (float)ps;
    c->warmest[k] = (float)warm;
    c->dry_share[k] = (float)(ps > 0.0 ? dry / ps : 0.5);
  }
}

/* Bilinear sample of a climate field at a macro cell (same face, no trigonometry). */
static double sample_at_macro(const zb_climate *c, const float *field, int32_t m) {
  int res = c->macro->res, rc = c->grid.res;
  int face = m / (res * res), y = (m / res) % res, x = m % res;
  double fx = ((double)x + 0.5) * (double)rc / (double)res - 0.5;
  double fy = ((double)y + 0.5) * (double)rc / (double)res - 0.5;
  fx = fx < 0.0 ? 0.0 : (fx > (double)(rc - 1) ? (double)(rc - 1) : fx);
  fy = fy < 0.0 ? 0.0 : (fy > (double)(rc - 1) ? (double)(rc - 1) : fy);
  int x0 = (int)fx, y0 = (int)fy;
  int x1 = x0 + 1 < rc ? x0 + 1 : x0, y1 = y0 + 1 < rc ? y0 + 1 : y0;
  double tx = fx - (double)x0, ty = fy - (double)y0;
  const float *h = field + (size_t)face * (size_t)rc * (size_t)rc;
  double a = (double)h[y0 * rc + x0] * (1.0 - tx) + (double)h[y0 * rc + x1] * tx;
  double b = (double)h[y1 * rc + x0] * (1.0 - tx) + (double)h[y1 * rc + x1] * tx;
  return a * (1.0 - ty) + b * ty;
}

/* ---- Hydrology on the macro grid ---------------------------------------------------- */

static double cell_temperature(const zb_climate *c, int32_t m, const float *field) {
  double h = (double)c->in.height[m];
  return sample_at_macro(c, field, m) - LAPSE * (h > 0.0 ? h : 0.0);
}

static double potential_evaporation(double t) {
  double pet = 55.0 * (t + 5.0);
  return pet < 0.0 ? 0.0 : (pet > 1800.0 ? 1800.0 : pet);
}

static void hydrology(zb_climate *c) {
  const zb_cubegrid *g = c->macro;
  size_t cells = g->cells;
  int32_t *indeg = (int32_t *)c->groundwater; /* scratch until groundwater is written */
  int32_t *queue = c->upwind;                 /* grown to macro size in create */
  memset(c->flags, 0, sizeof(uint16_t) * cells);
  memset(c->order, 0, cells);
  memset(indeg, 0, sizeof(int32_t) * cells);
  for (size_t m = 0; m < cells; ++m) {
    c->discharge[m] = 0.0f;
    if (macro_ocean(c, m)) {
      c->flags[m] |= ZB_CELL_OCEAN;
    } else if (c->in.lake && c->in.lake[m] > 0.0f) {
      c->flags[m] |= ZB_CELL_LAKE;
    }
    if (c->in.geo_flags && (c->in.geo_flags[m] & (1u << 10))) { /* ZB_GEO_ACTIVE_VOLCANO */
      c->flags[m] |= ZB_CELL_VOLCANO;
    }
    int32_t r = c->in.receiver ? c->in.receiver[m] : -1;
    if (r >= 0 && !macro_ocean(c, m)) {
      indeg[r]++;
    }
  }
  /* Kahn's order from the headwaters downstream. */
  size_t head = 0, tail = 0;
  uint8_t *river_donors = c->biome; /* scratch until biomes are written */
  memset(river_donors, 0, cells);
  for (size_t m = 0; m < cells; ++m) {
    if (indeg[m] == 0 && !macro_ocean(c, m)) {
      queue[tail++] = (int32_t)m;
    }
  }
  while (head < tail) {
    int32_t m = queue[head++];
    double t = cell_temperature(c, m, c->annual_t), p = sample_at_macro(c, c->annual_p, m);
    double pet = potential_evaporation(t);
    double area = (double)zb_cubegrid_area(g, m);
    double runoff = p > 0.0 ? p * p / (p + pet) : 0.0; /* mm per year */
    double q = (double)c->discharge[m] + runoff * area * 1000.0 / SECONDS_PER_YEAR;
    if (c->flags[m] & ZB_CELL_LAKE) {
      double evap = pet * area * 1000.0 / SECONDS_PER_YEAR;
      if (q > 0.0 && q <= evap) {
        c->flags[m] |= ZB_CELL_SALT_LAKE; /* the lake loses all its water to the sky */
      }
      q = q > evap ? q - evap : 0.0;
    }
    c->discharge[m] = (float)q;
    if (q >= RIVER_DISCHARGE && !(c->flags[m] & ZB_CELL_LAKE)) {
      c->flags[m] |= ZB_CELL_RIVER;
      /* Strahler order: river donors of the highest order meet. */
      int top = c->order[m], meet = river_donors[m];
      c->order[m] = (uint8_t)(top == 0 ? 1 : (meet >= 2 ? top + 1 : top));
    }
    int32_t r = c->in.receiver ? c->in.receiver[m] : -1;
    if (r < 0) {
      continue;
    }
    c->discharge[r] += (float)q;
    if (c->flags[m] & ZB_CELL_RIVER) {
      if (c->order[m] > c->order[r]) {
        c->order[r] = c->order[m];
        river_donors[r] = 1;
      } else if (c->order[m] == c->order[r]) {
        river_donors[r]++;
      }
    }
    if (macro_ocean(c, (size_t)r)) {
      if (c->flags[m] & ZB_CELL_RIVER) {
        c->flags[m] |= ZB_CELL_RIVER_MOUTH;
      }
      continue;
    }
    if (--indeg[r] == 0) {
      queue[tail++] = r;
    }
  }
  /* Ocean cells only collect: they hold no order. */
  for (size_t m = 0; m < cells; ++m) {
    if (c->flags[m] & ZB_CELL_OCEAN) {
      c->order[m] = 0;
    }
  }
}

static void ice_and_groundwater(zb_climate *c) {
  const zb_cubegrid *g = c->macro;
  size_t cells = g->cells;
  for (size_t m = 0; m < cells; ++m) {
    int32_t mi = (int32_t)m;
    double t = cell_temperature(c, mi, c->annual_t), warm = cell_temperature(c, mi, c->warmest);
    double p = sample_at_macro(c, c->annual_p, mi);
    if (c->flags[m] & ZB_CELL_OCEAN) {
      if (t < -3.0) {
        c->flags[m] |= ZB_CELL_SEA_ICE;
      }
      c->groundwater[m] = 0.0f;
      continue;
    }
    if (t < -10.0 || (warm < 0.0 && p > 150.0)) {
      c->flags[m] |= ZB_CELL_GLACIER;
    }
    for (int k = 0; k < 4; ++k) {
      int32_t nb = zb_cubegrid_nbr(g, mi, k);
      if (nb >= 0 && (c->flags[nb] & ZB_CELL_OCEAN)) {
        c->flags[m] |= ZB_CELL_COAST;
      }
    }
    double depth = 2.0 + 250.0 * zb_dexp(-p / 250.0);
    double h = (double)c->in.height[m];
    depth += h > 0.0 ? h / 50.0 : 0.0;
    if (c->flags[m] & (ZB_CELL_RIVER | ZB_CELL_LAKE)) {
      depth = 1.0;
    } else {
      for (int k = 0; k < 8; ++k) { /* beside a river the water table is shallow */
        int32_t nb = zb_cubegrid_nbr(g, mi, k);
        if (nb >= 0 && (c->flags[nb] & (ZB_CELL_RIVER | ZB_CELL_LAKE))) {
          depth *= 0.25;
          break;
        }
      }
    }
    c->groundwater[m] = (float)depth;
    if (p < 250.0 && depth < 25.0) {
      c->flags[m] |= ZB_CELL_OASIS;
    }
  }
}

/* ---- Biomes ------------------------------------------------------------------------- */

static zb_biome ocean_biome(const zb_climate *c, int32_t m) {
  double depth = -(double)c->in.height[m];
  double sst = sample_at_macro(c, c->annual_t, m);
  if (c->flags[m] & ZB_CELL_SEA_ICE) {
    return ZB_BIOME_SEA_ICE;
  }
  if (depth > 200.0) {
    return ZB_BIOME_DEEP_OCEAN;
  }
  if (depth < 60.0) {
    uint64_t h = zb_rng_at(c->in.seed, 0x7265656600ull, (uint64_t)m);
    if (sst > 22.0 && (h & 3u) != 0u) {
      return ZB_BIOME_CORAL_REEF;
    }
    if (sst > 4.0 && sst < 16.0 && (h & 3u) == 0u) {
      return ZB_BIOME_KELP_FOREST;
    }
  }
  return ZB_BIOME_SHELF;
}

static zb_biome land_biome(const zb_climate *c, int32_t m) {
  const zb_cubegrid *g = c->macro;
  uint16_t f = c->flags[m];
  double h = (double)c->in.height[m];
  double t = cell_temperature(c, m, c->annual_t), warm = cell_temperature(c, m, c->warmest);
  double p = sample_at_macro(c, c->annual_p, m), dry = sample_at_macro(c, c->dry_share, m);
  if (f & ZB_CELL_LAKE) {
    return (f & ZB_CELL_SALT_LAKE) ? ZB_BIOME_SALT_LAKE : ZB_BIOME_LAKE;
  }
  if (f & ZB_CELL_GLACIER) {
    return h > 1500.0 && t > -20.0 ? ZB_BIOME_GLACIER : ZB_BIOME_ICE_SHEET;
  }
  if ((f & ZB_CELL_RIVER) && c->discharge[m] > GREAT_RIVER) {
    return ZB_BIOME_RIVER; /* the great rivers are wide enough to be their own biome */
  }
  /* Steepness: the largest drop to a neighbour. */
  double drop = 0.0;
  for (int k = 0; k < 4; ++k) {
    int32_t nb = zb_cubegrid_nbr(g, m, k);
    if (nb >= 0) {
      double d = h - (double)c->in.height[nb];
      drop = d > drop ? d : drop;
    }
  }
  uint64_t hash = zb_rng_at(c->in.seed, 0x62696f6d65ull, (uint64_t)m);
  double rocky = (double)(hash & 0xffff) / 65536.0;
  rocky = h > 600.0 ? 0.6 + 0.4 * rocky : 0.7 * rocky; /* sand seas in lowland basins, rock on plateaus */
  if (f & ZB_CELL_COAST) {
    if ((f & ZB_CELL_RIVER_MOUTH) && c->order[m] >= 3) {
      return ZB_BIOME_ESTUARY;
    }
    if (t > 20.0 && p > 1200.0 && h < 30.0) {
      return ZB_BIOME_MANGROVE;
    }
    if (h > 300.0 || drop > 250.0) {
      return ZB_BIOME_CLIFFS;
    }
    if (p < 300.0) {
      return ZB_BIOME_DUNES;
    }
    if (t > 3.0 && t < 20.0 && p > 600.0 && h < 20.0) {
      return ZB_BIOME_SALT_MARSH;
    }
    if (h < 40.0) {
      return ZB_BIOME_BEACH;
    }
  }
  if ((f & ZB_CELL_VOLCANO) && h > 800.0) {
    return ZB_BIOME_VOLCANIC_SLOPES;
  }
  /* Mountains. */
  if (h > 1200.0) {
    if (t > 15.0 && h < 3200.0 && p > 1800.0) {
      return ZB_BIOME_CLOUD_FOREST;
    }
    if (warm < 10.0 || t < 0.0) {
      return p > 300.0 && drop < 400.0 ? ZB_BIOME_ALPINE_MEADOW : ZB_BIOME_SCREE;
    }
    if (h > 1800.0 && t > 0.0 && t < 15.0 && p > 700.0) {
      return ZB_BIOME_MONTANE_FOREST;
    }
  }
  /* Arid basins with salt. */
  if (p < 300.0 && c->in.minerals && (c->in.minerals[m] & (1u << 5)) && h < 1500.0) { /* ZB_MIN_SALT */
    return ZB_BIOME_SALT_FLAT;
  }
  /* Wetlands on flat, well-watered lowland. */
  if (h < 250.0 && drop < 40.0) {
    bool wet_river = (f & ZB_CELL_RIVER) && c->order[m] >= 4;
    if (t > 20.0 && p > 1800.0 && (rocky < 0.4 || wet_river)) {
      return ZB_BIOME_SWAMP_FOREST;
    }
    if (wet_river && t > 3.0) {
      return ZB_BIOME_FLOODPLAIN;
    }
    if (t < 5.0 && t > -5.0 && p > 450.0 && rocky < 0.25) {
      return ZB_BIOME_PEAT_BOG;
    }
    if (t >= 5.0 && p > 900.0 && rocky < 0.12) {
      return ZB_BIOME_MARSH;
    }
  }
  return zb_biome_classify(t, p, warm, dry, rocky);
}

/* Magical overlays around the strongest nexuses, chosen by their setting. */
static void magical_biomes(zb_climate *c) {
  const zb_cubegrid *g = c->macro;
  if (c->in.nexus_count <= 0) {
    return;
  }
  double r_km = c->in.radius / 1000.0;
  for (int k = 0; k < c->in.nexus_count; ++k) {
    float s = c->in.nexus_strength ? c->in.nexus_strength[k] : 1.0f;
    if (s < 0.75f) {
      continue; /* only extreme mana makes magical land */
    }
    zb_dvec3 centre = c->in.nexus[k];
    int32_t mc = zb_cubegrid_cell_at(g, centre);
    if (c->flags[mc] & ZB_CELL_OCEAN) {
      continue;
    }
    zb_biome around = (zb_biome)c->biome[mc];
    double h = (double)c->in.height[mc];
    uint8_t kind = c->in.nexus_kind ? c->in.nexus_kind[k] : 0;
    zb_biome magic;
    if ((c->flags[mc] & ZB_CELL_VOLCANO) || kind == 1 /* volcano */) {
      magic = ZB_BIOME_ASHLANDS;
    } else if (kind == 0 && s >= 0.95f) {
      magic = ZB_BIOME_HOLLOWS; /* the strongest triple junctions lift the ground */
    } else if (zb_biome_group_of(around) == ZB_GROUP_WETLAND || around == ZB_BIOME_LAKE ||
               (c->flags[mc] & ZB_CELL_RIVER)) {
      magic = sample_at_macro(c, c->annual_t, mc) > 18.0 ? ZB_BIOME_MIREFEN : ZB_BIOME_DROWNED_VALE;
    } else if (around == ZB_BIOME_RAINFOREST || around == ZB_BIOME_SEASONAL_FOREST ||
               around == ZB_BIOME_TEMPERATE_RAINFOREST) {
      magic = ZB_BIOME_WILDHEART;
    } else if (zb_biome_group_of(around) == ZB_GROUP_ARID && h > 600.0) {
      magic = ZB_BIOME_STORMSPIRES;
    } else if (c->in.minerals && (c->in.minerals[mc] & (1u << 14))) { /* ZB_MIN_MANA_CRYSTAL */
      magic = ZB_BIOME_GLASSWOOD;
    } else if (kind == 2 /* suture */ || h > 1500.0) {
      magic = ZB_BIOME_DEEPSTONE;
    } else {
      magic = ZB_BIOME_GLASSWOOD;
    }
    double radius = (15.0 + 45.0 * (double)s) / r_km; /* radians */
    double min_dot = zb_dcos(radius);
    for (size_t m = 0; m < g->cells; ++m) {
      if ((c->flags[m] & ZB_CELL_OCEAN) || (c->flags[m] & ZB_CELL_LAKE)) {
        continue;
      }
      if (zb_dv_dot(zb_cubegrid_dir(g, (int32_t)m), centre) >= min_dot) {
        c->biome[m] = (uint8_t)magic;
      }
    }
    c->magical_patches++;
  }
}

/* ---- Biogeographic provinces ----------------------------------------------------------- */

#define MAX_PROVINCES 254
#define BIG_LANDMASSES 512

static int lineage_at(const zb_climate *c, size_t m) {
  int l = c->in.lineage ? c->in.lineage[m] : 0;
  return l < c->in.lineages ? l : 0;
}

/* Myr since two lineages last met, < 0 if never. */
static double lineage_contact(const zb_climate *c, int a, int b) {
  if (!c->in.contact || a <= 0 || b <= 0 || a >= c->in.lineages || b >= c->in.lineages) {
    return -1.0;
  }
  return (double)c->in.contact[a * c->in.lineages + b];
}

static bool lineages_close(const zb_climate *c, int a, int b) {
  if (a == b || a == 0 || b == 0) {
    return true;
  }
  double met = lineage_contact(c, a, b);
  return met >= 0.0 && met < 50.0;
}

typedef struct landmass_info {
  double area;
  int big; /* index among large landmasses, -1 for small ones */
} landmass_info;

static int label_landmasses(zb_climate *c, int32_t *land_id, landmass_info **out) {
  const zb_cubegrid *g = c->macro;
  size_t cells = g->cells, cap = 256;
  int32_t *queue = c->upwind;
  landmass_info *info = zb_alloc(c->a, sizeof(landmass_info) * cap);
  int count = 0;
  for (size_t m = 0; m < cells; ++m) {
    land_id[m] = -1;
  }
  for (size_t s = 0; s < cells && info; ++s) {
    if (land_id[s] >= 0 || (c->flags[s] & ZB_CELL_OCEAN)) {
      continue;
    }
    if ((size_t)count == cap) {
      landmass_info *grown = zb_realloc(c->a, info, sizeof(landmass_info) * cap * 2);
      if (!grown) {
        zb_free(c->a, info);
        info = NULL;
        break;
      }
      info = grown;
      cap *= 2;
    }
    size_t head = 0, tail = 0;
    queue[tail++] = (int32_t)s;
    land_id[s] = count;
    double area = 0.0;
    while (head < tail) {
      int32_t m = queue[head++];
      area += (double)zb_cubegrid_area(g, m);
      for (int k = 0; k < 4; ++k) {
        int32_t nb = zb_cubegrid_nbr(g, m, k);
        if (nb >= 0 && land_id[nb] < 0 && !(c->flags[nb] & ZB_CELL_OCEAN)) {
          land_id[nb] = count;
          queue[tail++] = nb;
        }
      }
    }
    info[count].area = area;
    info[count].big = -1;
    count++;
  }
  *out = info;
  return info ? count : -1;
}

static int add_province(zb_climate *c, zb_heap *heap, int32_t seed, int landmass, double lm_area) {
  if (c->province_count >= MAX_PROVINCES) {
    return -1;
  }
  int id = c->province_count++;
  zb_province *pv = &c->provinces[id];
  pv->landmass = landmass;
  pv->lineage = lineage_at(c, (size_t)seed);
  double iso = 1.0 - lm_area / 2.0e6;
  pv->isolation = (float)(iso < 0.0 ? 0.0 : iso);
  zb_heap_push(heap, 0.0, seed, id);
  return id;
}

static void grow_provinces(zb_climate *c, zb_heap *heap, const uint8_t *near_land) {
  const zb_cubegrid *g = c->macro;
  float *cost = c->rate; /* macro-sized scratch */
  for (size_t m = 0; m < g->cells; ++m) {
    cost[m] = 3.0e38f;
  }
  uint8_t *done = c->order; /* reused: river orders are restored by the caller */
  zb_heap_item it;
  while (zb_heap_pop(heap, &it)) {
    if (done[it.id]) {
      continue;
    }
    done[it.id] = 1;
    bool sea = (c->flags[it.id] & ZB_CELL_OCEAN) != 0;
    if (!sea) {
      c->province[it.id] = (uint8_t)(it.aux + 1);
    }
    for (int k = 0; k < 4; ++k) {
      int32_t nb = zb_cubegrid_nbr(g, it.id, k);
      if (nb < 0 || done[nb]) {
        continue;
      }
      double step;
      if (c->flags[nb] & ZB_CELL_OCEAN) {
        if (!near_land[nb]) {
          continue; /* open sea stops plants and animals */
        }
        step = 12.0;
      } else {
        step = 1.0;
        step += c->in.height[nb] > 2500.0f ? 4.0 : 0.0;
        step += (c->biome[nb] == ZB_BIOME_SAND_DESERT || c->biome[nb] == ZB_BIOME_ROCKY_DESERT) ? 2.0 : 0.0;
        step += (c->flags[nb] & ZB_CELL_GLACIER) ? 4.0 : 0.0;
        step += lineages_close(c, lineage_at(c, (size_t)it.id), lineage_at(c, (size_t)nb)) ? 0.0 : 25.0;
      }
      double key = it.key + step;
      if (key < (double)cost[nb]) {
        cost[nb] = (float)key;
        zb_heap_push(heap, key, nb, it.aux);
      }
    }
  }
}

static void provinces(zb_climate *c) {
  const zb_cubegrid *g = c->macro;
  size_t cells = g->cells;
  int32_t *land_id = (int32_t *)c->q2; /* macro-sized scratch */
  landmass_info *lm = NULL;
  int lineages = c->in.lineages > 0 && c->in.lineage ? c->in.lineages : 1;
  double *lin_area = NULL;
  int32_t *lin_count = NULL;
  uint8_t *near_land = zb_alloc(c->a, cells);
  uint8_t *orders = zb_alloc(c->a, cells);
  zb_heap heap;
  zb_heap_init(&heap, c->a);
  memset(c->province, 0, cells);
  c->province_count = 0;
  c->provinces = zb_calloc(c->a, sizeof(zb_province) * MAX_PROVINCES);
  int landmasses = c->provinces && near_land && orders ? label_landmasses(c, land_id, &lm) : -1;
  if (landmasses < 0) {
    goto done;
  }
  memcpy(orders, c->order, cells);
  double land_total = 0.0;
  int big = 0;
  for (int k = 0; k < landmasses; ++k) {
    land_total += lm[k].area;
  }
  for (int k = 0; k < landmasses && big < BIG_LANDMASSES; ++k) {
    if (lm[k].area >= 0.002 * land_total) {
      lm[k].big = big++;
    }
  }
  /* Area and cell count of every lineage on every large landmass. */
  lin_area = zb_calloc(c->a, sizeof(double) * (size_t)(big > 0 ? big : 1) * (size_t)lineages);
  lin_count = zb_calloc(c->a, sizeof(int32_t) * (size_t)(big > 0 ? big : 1) * (size_t)lineages);
  if (!lin_area || !lin_count) {
    goto done;
  }
  for (size_t m = 0; m < cells; ++m) {
    if (land_id[m] >= 0 && lm[land_id[m]].big >= 0) {
      size_t slot = (size_t)lm[land_id[m]].big * (size_t)lineages + (size_t)lineage_at(c, m);
      lin_area[slot] += (double)zb_cubegrid_area(g, (int32_t)m);
      lin_count[slot]++;
    }
  }
  /* Seeds: on each large landmass, a few for every lineage holding a sizeable part. */
  int32_t *rank = zb_calloc(c->a, sizeof(int32_t) * (size_t)(big > 0 ? big : 1) * (size_t)lineages);
  int32_t *want = zb_calloc(c->a, sizeof(int32_t) * (size_t)(big > 0 ? big : 1) * (size_t)lineages);
  if (!rank || !want) {
    zb_free(c->a, rank);
    zb_free(c->a, want);
    goto done;
  }
  for (int k = 0; k < landmasses; ++k) {
    if (lm[k].big < 0) {
      continue;
    }
    size_t base = (size_t)lm[k].big * (size_t)lineages;
    int largest = 0;
    for (int l = 1; l < lineages; ++l) {
      largest = lin_area[base + (size_t)l] > lin_area[base + (size_t)largest] ? l : largest;
    }
    for (int l = 0; l < lineages; ++l) {
      double a = lin_area[base + (size_t)l];
      if (l == largest || a >= 0.05 * lm[k].area || a >= 3.0e5) {
        int n = (int)(a / 2.5e6) + 1;
        want[base + (size_t)l] = n > 6 ? 6 : n;
      }
    }
  }
  /* Seed cells at evenly spaced ranks of each lineage's cells, in cell order. */
  for (size_t m = 0; m < cells; ++m) {
    if (land_id[m] < 0 || lm[land_id[m]].big < 0) {
      continue;
    }
    size_t slot = (size_t)lm[land_id[m]].big * (size_t)lineages + (size_t)lineage_at(c, m);
    int n = want[slot];
    if (n == 0) {
      continue;
    }
    int r = rank[slot]++, total = lin_count[slot];
    for (int s2 = 0; s2 < n; ++s2) {
      if (r == (int)(((double)s2 + 0.5) * (double)total / (double)n)) {
        add_province(c, &heap, (int32_t)m, land_id[m], lm[land_id[m]].area);
      }
    }
  }
  zb_free(c->a, rank);
  zb_free(c->a, want);
  /* Coastal waters (two cells) can be crossed: islands near a coast join it. */
  memset(near_land, 0, cells);
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t m = 0; m < cells; ++m) {
      if (!(c->flags[m] & ZB_CELL_OCEAN) || near_land[m]) {
        continue;
      }
      for (int k = 0; k < 4; ++k) {
        int32_t nb = zb_cubegrid_nbr(g, (int32_t)m, k);
        if (nb >= 0 && (!(c->flags[nb] & ZB_CELL_OCEAN) || near_land[nb] == pass + 1)) {
          near_land[m] = (uint8_t)(pass + 2);
          break;
        }
      }
    }
    for (size_t m = 0; m < cells; ++m) {
      near_land[m] = near_land[m] == pass + 2 ? (uint8_t)(pass + 1) : near_land[m];
    }
  }
  memset(c->order, 0, cells);
  grow_provinces(c, &heap, near_land);
  /* Remote islands not reached become provinces of their own. */
  for (size_t m = 0; m < cells && c->province_count < MAX_PROVINCES; ++m) {
    if (!(c->flags[m] & ZB_CELL_OCEAN) && c->province[m] == 0) {
      add_province(c, &heap, (int32_t)m, land_id[m], lm[land_id[m]].area);
      grow_provinces(c, &heap, near_land);
    }
  }
  memcpy(c->order, orders, cells);
  /* Areas, centres and isolation. */
  zb_dvec3 *sum = zb_calloc(c->a, sizeof(zb_dvec3) * MAX_PROVINCES);
  if (sum) {
    for (size_t m = 0; m < cells; ++m) {
      int p = c->province[m];
      if (p > 0) {
        c->provinces[p - 1].area += zb_cubegrid_area(g, (int32_t)m);
        sum[p - 1] = zb_dv_add(sum[p - 1], zb_cubegrid_dir(g, (int32_t)m));
      }
    }
  }
  for (int p = 0; p < c->province_count; ++p) {
    zb_province *pv = &c->provinces[p];
    pv->centre = sum ? zb_dv_norm(sum[p]) : zb_dv(0, 0, 1);
    /* A lineage that met no other for long evolved alone (endemics). */
    double latest = -1.0;
    for (int l = 1; l < lineages && pv->lineage > 0; ++l) {
      double met = lineage_contact(c, pv->lineage, l);
      latest = met >= 0.0 && (latest < 0.0 || met < latest) ? met : latest;
    }
    double lineage_iso = pv->lineage == 0 ? 0.0 : (latest < 0.0 ? 0.6 : (latest > 100.0 ? 0.3 : 0.0));
    pv->isolation = pv->isolation > (float)lineage_iso ? pv->isolation : (float)lineage_iso;
  }
  zb_free(c->a, sum);
done:
  zb_heap_free(&heap);
  zb_free(c->a, lm);
  zb_free(c->a, lin_area);
  zb_free(c->a, lin_count);
  zb_free(c->a, near_land);
  zb_free(c->a, orders);
}

double zb_climate_province_affinity(const zb_climate *c, int a, int b) {
  if (c->phase != ZB_CLIMATE_PHASE_DONE || a < 0 || b < 0 || a >= c->province_count || b >= c->province_count) {
    return 0.0;
  }
  if (a == b) {
    return 1.0;
  }
  const zb_province *pa = &c->provinces[a], *pb = &c->provinces[b];
  double v = pa->landmass == pb->landmass ? 0.6 : 0.1;
  if (pa->lineage != 0 && pa->lineage == pb->lineage) {
    v += 0.3;
  } else {
    double met = lineage_contact(c, pa->lineage, pb->lineage);
    v += met >= 0.0 ? (met < 50.0 ? 0.2 : (met < 150.0 ? 0.1 : 0.0)) : 0.0;
  }
  return v > 0.95 ? 0.95 : v;
}

static void biomes(zb_climate *c) {
  const zb_cubegrid *g = c->macro;
  for (size_t m = 0; m < g->cells; ++m) {
    int32_t mi = (int32_t)m;
    c->biome[m] = (uint8_t)((c->flags[m] & ZB_CELL_OCEAN) ? ocean_biome(c, mi) : land_biome(c, mi));
  }
  magical_biomes(c);
  provinces(c);
}

/* ---- Lifetime ------------------------------------------------------------------------- */

zb_climate *zb_climate_create(const zb_allocator *a, const zb_climate_input *in, char *err, size_t err_len) {
  if (!in->grid || !in->height || in->grid->res < 8) {
    snprintf(err, err_len, "a macro grid of at least 8 cells per face edge with heights is needed");
    return NULL;
  }
  if (!(in->radius >= ZB_PLANET_MIN_RADIUS && in->radius <= ZB_PLANET_MAX_RADIUS)) {
    snprintf(err, err_len, "radius out of range");
    return NULL;
  }
  if (!(in->axial_tilt >= 0.0 && in->axial_tilt <= 60.0)) {
    snprintf(err, err_len, "axial tilt must be 0..60 degrees");
    return NULL;
  }
  zb_climate *c = zb_calloc(a, sizeof *c);
  if (!c) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  c->a = a;
  c->in = *in;
  c->macro = in->grid;
  int rc = in->grid->res / 4;
  rc = rc < 8 ? 8 : rc;
  size_t mc = in->grid->cells;
  bool ok = zb_cubegrid_init(&c->grid, a, rc, in->radius / 1000.0) == ZB_OK;
  size_t cc = c->grid.cells, big = mc > cc ? mc : cc;
  if (ok) {
    c->dir = zb_alloc(a, sizeof(zb_dvec3) * cc);
    c->height = zb_alloc(a, sizeof(float) * cc);
    c->land = zb_alloc(a, sizeof(float) * cc);
    c->ocean = zb_alloc(a, cc);
    c->coast_km = zb_alloc(a, sizeof(float) * cc);
    c->current = zb_alloc(a, sizeof(float) * cc);
    c->temp = zb_alloc(a, sizeof(float) * cc * ZB_MONTHS);
    c->precip = zb_alloc(a, sizeof(float) * cc * ZB_MONTHS);
    c->annual_t = zb_alloc(a, sizeof(float) * cc);
    c->annual_p = zb_alloc(a, sizeof(float) * cc);
    c->warmest = zb_alloc(a, sizeof(float) * cc);
    c->dry_share = zb_alloc(a, sizeof(float) * cc);
    c->biome = zb_alloc(a, mc);
    c->flags = zb_alloc(a, sizeof(uint16_t) * mc);
    c->discharge = zb_alloc(a, sizeof(float) * mc);
    c->order = zb_alloc(a, mc);
    c->groundwater = zb_alloc(a, sizeof(float) * mc);
    c->province = zb_alloc(a, mc);
    c->upwind = zb_alloc(a, sizeof(int32_t) * big);
    c->q = zb_alloc(a, sizeof(float) * cc);
    c->q2 = zb_alloc(a, sizeof(float) * big);
    c->rate = zb_alloc(a, sizeof(float) * big);
    c->evap = zb_alloc(a, sizeof(float) * cc);
    ok = c->dir && c->height && c->land && c->ocean && c->coast_km && c->current && c->temp && c->precip &&
         c->annual_t && c->annual_p && c->warmest && c->dry_share && c->biome && c->flags && c->discharge && c->order &&
         c->groundwater && c->province && c->upwind && c->q && c->q2 && c->rate && c->evap;
  }
  if (!ok) {
    zb_climate_destroy(c);
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  c->phase = ZB_CLIMATE_PHASE_GRID;
  return c;
}

void zb_climate_destroy(zb_climate *c) {
  if (!c) {
    return;
  }
  zb_cubegrid_free(&c->grid);
  void *bufs[] = {c->dir,       c->height, c->land,      c->ocean,    c->coast_km,    c->current,
                  c->temp,      c->precip, c->annual_t,  c->annual_p, c->warmest,     c->dry_share,
                  c->biome,     c->flags,  c->discharge, c->order,    c->groundwater, c->province,
                  c->provinces, c->upwind, c->q,         c->q2,       c->rate,        c->evap};
  for (size_t k = 0; k < sizeof bufs / sizeof bufs[0]; ++k) {
    zb_free(c->a, bufs[k]);
  }
  zb_free(c->a, c);
}

zb_climate_phase zb_climate_advance(zb_climate *c) {
  switch (c->phase) {
    case ZB_CLIMATE_PHASE_GRID:
      downsample(c);
      ocean_currents(c);
      coast_distance(c);
      c->month = 0;
      c->phase = ZB_CLIMATE_PHASE_MONTHS;
      break;
    case ZB_CLIMATE_PHASE_MONTHS:
      compute_month(c, c->month++);
      if (c->month >= ZB_MONTHS) {
        annual_summary(c);
        c->phase = ZB_CLIMATE_PHASE_HYDROLOGY;
      }
      break;
    case ZB_CLIMATE_PHASE_HYDROLOGY:
      hydrology(c);
      ice_and_groundwater(c);
      c->phase = ZB_CLIMATE_PHASE_BIOMES;
      break;
    case ZB_CLIMATE_PHASE_BIOMES:
      biomes(c);
      c->phase = ZB_CLIMATE_PHASE_DONE;
      break;
    case ZB_CLIMATE_PHASE_DONE: break;
  }
  return c->phase;
}

void zb_climate_run(zb_climate *c) {
  while (zb_climate_advance(c) != ZB_CLIMATE_PHASE_DONE) {
  }
}

double zb_climate_progress(const zb_climate *c) {
  switch (c->phase) {
    case ZB_CLIMATE_PHASE_GRID: return 0.0;
    case ZB_CLIMATE_PHASE_MONTHS: return 0.05 + 0.6 * (double)c->month / ZB_MONTHS;
    case ZB_CLIMATE_PHASE_HYDROLOGY: return 0.65;
    case ZB_CLIMATE_PHASE_BIOMES: return 0.8;
    default: return 1.0;
  }
}

/* ---- Queries ----------------------------------------------------------------------------- */

const zb_cubegrid *zb_climate_grid(const zb_climate *c) { return &c->grid; }

const float *zb_climate_monthly(const zb_climate *c, bool precipitation) { return precipitation ? c->precip : c->temp; }

void zb_climate_month(const zb_climate *c, zb_dvec3 dir, int month, double height, double *temperature,
                      double *precipitation) {
  size_t off = (size_t)(month % ZB_MONTHS) * c->grid.cells;
  *temperature = zb_cubegrid_sample(&c->grid, c->temp + off, dir) - LAPSE * (height > 0.0 ? height : 0.0);
  *precipitation = zb_cubegrid_sample(&c->grid, c->precip + off, dir);
}

void zb_climate_annual(const zb_climate *c, zb_dvec3 dir, double height, double *temperature, double *precipitation) {
  *temperature = zb_cubegrid_sample(&c->grid, c->annual_t, dir) - LAPSE * (height > 0.0 ? height : 0.0);
  *precipitation = zb_cubegrid_sample(&c->grid, c->annual_p, dir);
}

static bool done(const zb_climate *c) { return c->phase == ZB_CLIMATE_PHASE_DONE; }

const uint8_t *zb_climate_biomes(const zb_climate *c) { return done(c) ? c->biome : NULL; }
const uint16_t *zb_climate_flags(const zb_climate *c) { return done(c) ? c->flags : NULL; }
const float *zb_climate_discharge(const zb_climate *c) { return done(c) ? c->discharge : NULL; }
const uint8_t *zb_climate_river_order(const zb_climate *c) { return done(c) ? c->order : NULL; }
const float *zb_climate_groundwater(const zb_climate *c) { return done(c) ? c->groundwater : NULL; }
const uint8_t *zb_climate_province_map(const zb_climate *c) { return done(c) ? c->province : NULL; }

const zb_province *zb_climate_provinces(const zb_climate *c, int *count) {
  *count = done(c) ? c->province_count : 0;
  return done(c) ? c->provinces : NULL;
}

zb_biome zb_climate_biome_at(const zb_climate *c, zb_dvec3 dir) {
  return done(c) ? (zb_biome)c->biome[zb_cubegrid_cell_at(c->macro, dir)] : ZB_BIOME_NONE;
}

void zb_climate_get_stats(const zb_climate *c, zb_climate_stats *out) {
  memset(out, 0, sizeof *out);
  if (!done(c)) {
    return;
  }
  const zb_cubegrid *g = c->macro;
  out->cells = (int)g->cells;
  for (size_t m = 0; m < g->cells; ++m) {
    double a = (double)zb_cubegrid_area(g, (int32_t)m);
    uint16_t f = c->flags[m];
    out->biome_area[c->biome[m]] += a;
    if (!(f & ZB_CELL_OCEAN)) {
      out->land_cells++;
      out->land_area += a;
    }
    out->lake_area += (f & ZB_CELL_LAKE) ? a : 0.0;
    out->salt_lake_area += (f & ZB_CELL_SALT_LAKE) ? a : 0.0;
    out->glacier_area += (f & ZB_CELL_GLACIER) ? a : 0.0;
    if (f & ZB_CELL_RIVER_MOUTH) {
      out->rivers++;
      out->great_rivers += c->discharge[m] > GREAT_RIVER;
    }
    out->max_order = c->order[m] > out->max_order ? c->order[m] : out->max_order;
  }
  out->provinces = c->province_count;
  out->magical_patches = c->magical_patches;
}

uint64_t zb_climate_hash(const zb_climate *c) {
  zb_hasher h;
  zb_hasher_init(&h, 0x636c696d617465ull);
  zb_hasher_bytes(&h, c->temp, sizeof(float) * c->grid.cells * ZB_MONTHS);
  zb_hasher_bytes(&h, c->precip, sizeof(float) * c->grid.cells * ZB_MONTHS);
  if (done(c)) {
    size_t mc = c->macro->cells;
    zb_hasher_bytes(&h, c->biome, mc);
    zb_hasher_bytes(&h, c->flags, sizeof(uint16_t) * mc);
    zb_hasher_bytes(&h, c->discharge, sizeof(float) * mc);
    zb_hasher_bytes(&h, c->province, mc);
  }
  return zb_hasher_final(&h);
}

/* ---- From geology ------------------------------------------------------------------------ */

zb_result zb_climate_input_from_geology(zb_climate_input *in, zb_climate_link *link, const zb_allocator *a,
                                        const zb_geology *g, uint64_t seed) {
  memset(in, 0, sizeof *in);
  memset(link, 0, sizeof *link);
  const zb_geology_macro *m = zb_geology_get_macro(g);
  const zb_cubegrid *grid = zb_geology_macro_grid(g);
  if (!m || !grid) {
    return ZB_ERR_STATE;
  }
  zb_geology_points pts;
  zb_geology_get_points(g, &pts);
  zb_ley_network ley;
  zb_geology_get_ley(g, &ley);
  link->a = a;
  link->geo_flags = zb_alloc(a, sizeof(uint16_t) * grid->cells);
  link->minerals = zb_alloc(a, sizeof(uint32_t) * grid->cells);
  link->lineage = zb_alloc(a, grid->cells);
  int nodes = ley.nodes > 0 ? ley.nodes : 1;
  link->nexus = zb_alloc(a, sizeof(zb_dvec3) * (size_t)nodes);
  link->strength = zb_alloc(a, sizeof(float) * (size_t)nodes);
  link->kind = zb_alloc(a, (size_t)nodes);
  if (!link->geo_flags || !link->minerals || !link->lineage || !link->nexus || !link->strength || !link->kind) {
    zb_climate_link_free(link);
    return ZB_ERR_NOMEM;
  }
  for (size_t c = 0; c < grid->cells; ++c) {
    int32_t p = m->point[c];
    link->geo_flags[c] = pts.flags[p];
    link->minerals[c] = pts.minerals[p];
    link->lineage[c] = pts.lineage[p];
  }
  const zb_lattice *lat = zb_geology_lattice(g);
  for (int k = 0; k < ley.nodes; ++k) {
    link->nexus[k] = lat->p[ley.node[k].point];
    link->strength[k] = ley.node[k].strength;
    link->kind[k] = ley.node[k].kind;
  }
  in->grid = grid;
  in->height = m->height;
  in->lake = m->lake;
  in->receiver = m->receiver;
  in->geo_flags = link->geo_flags;
  in->minerals = link->minerals;
  in->lineage = link->lineage;
  in->contact = zb_geology_contacts(g);
  in->lineages = ZB_GEO_LINEAGES;
  in->radius = zb_geology_radius(g);
  in->axial_tilt = 23.0;
  in->seed = seed;
  in->nexus = link->nexus;
  in->nexus_strength = link->strength;
  in->nexus_kind = link->kind;
  in->nexus_count = ley.nodes;
  return ZB_OK;
}

void zb_climate_link_free(zb_climate_link *link) {
  if (!link->a) {
    return;
  }
  zb_free(link->a, link->geo_flags);
  zb_free(link->a, link->minerals);
  zb_free(link->a, link->lineage);
  zb_free(link->a, link->nexus);
  zb_free(link->a, link->strength);
  zb_free(link->a, link->kind);
  memset(link, 0, sizeof *link);
}
