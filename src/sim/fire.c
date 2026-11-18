/* SPDX-License-Identifier: MIT */
#include "sim/fire.h"

#include "sim/active.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"
#include "core/hash.h"
#include "core/log.h"
#include "world/calendar.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/flora.h"
#include "world/stream.h"

#define N ZB_FIRE_RES
#define CELLS (N * N)

/* The model, per square metre of ground. Heat capacity is a thin layer of
 * ground plus the fine fuel (the part that heats before it ignites) and its
 * water. A burning cell gives a share of its heat to its eight neighbours,
 * weighted downwind and uphill, and loses heat to the air like every cell. */
#define BASE_HEAT 800.0   /* J/(m^2 K): the top of the ground and the air in the fuel */
#define FINE_FUEL 1.0     /* kg/m^2 that must heat up before the fuel catches */
#define WATER_HEAT 4186.0 /* J/(kg K) */
#define LATENT 2257000.0  /* J/kg to evaporate water */
#define EXTINCTION 0.3    /* kg water per kg fuel: wetter fuel does not burn */
#define FLAME 800.0       /* deg C above ambient in a burning cell */
#define RADIATED 0.5      /* share of a fire's heat its neighbours get */
#define LOSS 25.0         /* W/(m^2 K) to the air */
#define CONDUCT 4.0       /* W/(m^2 K) to each of the four neighbours */
#define WIND_LEAN 0.35    /* s/m: exp(this x wind along the direction) */
#define SLOPE_LEAN 4.0    /* exp(this x rise per metre) */
#define MIN_FUEL 0.02     /* kg/m^2: less does not burn */
#define DRYING 600.0      /* s: moisture approaches its equilibrium */
#define EMBERS 0.25       /* embers per second from a crown fire of 200 kW/m^2 */
#define AWAKE 0.5         /* deg C: a chunk with less everywhere goes to sleep */

/* Cell states. */
enum { WATER = 1, BURNING = 2, BURNT = 4, SCARRED = 8 };

/* Fuels by kind, with the registry's properties. */
enum { FUEL_NONE, FUEL_GRASS, FUEL_LEAVES, FUEL_WOOD, FUEL_KINDS };

typedef struct fuel_kind {
  int material;
  double ignition, heat, rate, cp, flammability;
  double smoke; /* kg of smoke per kg burnt */
} fuel_kind;

typedef struct fire_chunk {
  zb_chunk chunk;
  double cw, ch; /* cell size, metres */
  double ambient;
  double emc;     /* equilibrium moisture of the fuel here */
  double wx, wy;  /* wind along the cell axes, m/s */
  double lean[8]; /* wind weight per neighbour direction */
  double dist[8]; /* metres to each neighbour */
  bool awake;
  float *block;    /* one allocation for every array below */
  float *t, *next; /* temperature above ambient, deg C (swapped every step) */
  float *gain;     /* W/m^2 received this step */
  float *fuel, *fuel0, *moist, *height;
  uint8_t *kind, *state;
  uint16_t scars[CELLS]; /* cells burnt out and not yet in the world's deltas */
  int scar_count;
} fire_chunk;

/* A fire still burning when its chunk left the active tier: where, and when. */
typedef struct dormant_fire {
  zb_chunk chunk;
  double left_at; /* game seconds */
  uint16_t *cells;
  int count;
} dormant_fire;

struct zb_fire {
  const zb_allocator *a;
  zb_active grid;
  const zb_materials *materials;
  zb_fire_weather weather;
  fuel_kind fuels[FUEL_KINDS];
  bool has_fuels;
  zb_fire_wind_fn wind_fn;
  void *wind_ctx;
  double pending; /* seconds not yet stepped */
  uint64_t steps;
  double since_survey;                      /* seconds since chunks were last taken in */
  dormant_fire dormant[ZB_FIRE_MAX_CHUNKS]; /* fires in chunks that left the active tier */
  int dormant_count;
  int burnt, spots, embers;
};

static fire_chunk *FC(const zb_fire *f, int s) { return f->grid.slot[s]->data; }
static fire_chunk *chunk_of(const zb_fire *f, int32_t cell) { return zb_active_chunk_of(&f->grid, cell)->data; }

/* exp(SLOPE_LEAN x rise) for rises of -1.5 .. 1.5 in steps of 1/128. */
#define SLOPE_STEPS 384
static double g_slope[SLOPE_STEPS + 1];
static bool g_slope_ready;

static void slope_table(void) {
  if (!g_slope_ready) {
    for (int k = 0; k <= SLOPE_STEPS; ++k) {
      g_slope[k] = zb_dexp(SLOPE_LEAN * (k / 128.0 - 1.5));
    }
    g_slope_ready = true;
  }
}

static double slope_weight(double rise) {
  int k = (int)((rise + 1.5) * 128.0 + 0.5);
  return g_slope[k < 0 ? 0 : (k > SLOPE_STEPS ? SLOPE_STEPS : k)];
}

static const int DX[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
static const int DY[8] = {0, 0, -1, 1, -1, -1, 1, 1};

/* ---------------------------------------------------------------- set-up */

static void *take_in(void *ctx, const zb_active_chunk *ac, const zb_chunk_terrain *ct);
static void catch_up(zb_fire *f, fire_chunk *c, const dormant_fire *d);
static void let_go(void *ctx, void *data);

static void find_fuels(zb_fire *f) {
  static const char *const names[FUEL_KINDS] = {NULL, "grass", "leaves", "wood"};
  f->has_fuels = f->materials != NULL;
  for (int k = 1; k < FUEL_KINDS && f->has_fuels; ++k) {
    int m = zb_materials_find(f->materials, names[k]);
    if (m < 0 || f->materials->info[m].flammability <= 0.0f) {
      zb_warn("fire: no burning material %s; nothing burns", names[k]);
      f->has_fuels = false;
      break;
    }
    const zb_material_info *x = &f->materials->info[m];
    f->fuels[k] = (fuel_kind){m, x->ignition, x->heat, x->burn_rate, x->specific_heat, x->flammability, 0.0};
    int smoke = zb_materials_find(f->materials, "smoke");
    for (int p = 0; p < ZB_MATERIAL_PRODUCTS; ++p) {
      f->fuels[k].smoke += smoke >= 0 && x->product[p] == smoke ? x->product_share[p] : 0.0;
    }
  }
}

zb_fire *zb_fire_create(const zb_allocator *a, zb_world *world, const zb_materials *materials, double radius) {
  zb_fire *f = zb_calloc(a, sizeof *f);
  if (!f) {
    return NULL;
  }
  f->a = a;
  f->materials = materials;
  slope_table();
  find_fuels(f);
  zb_active_hooks hooks = {f, take_in, let_go};
  zb_active_init(&f->grid, a, world, radius, ZB_STAGE_ECOLOGY, ZB_ACTIVE_RES, &hooks);
  return f;
}

static void free_chunk(const zb_fire *f, fire_chunk *c) {
  zb_free(f->a, c->block);
  zb_free(f->a, c);
}

static void forget_dormant(zb_fire *f) {
  for (int k = 0; k < f->dormant_count; ++k) {
    zb_free(f->a, f->dormant[k].cells);
  }
  f->dormant_count = 0;
}

void zb_fire_destroy(zb_fire *f) {
  if (f) {
    zb_active_clear(&f->grid, NULL);
    forget_dormant(f);
    zb_free(f->a, f);
  }
}

void zb_fire_set_radius(zb_fire *f, double radius) { f->grid.radius = radius; }

void zb_fire_set_world(zb_fire *f, zb_world *world) {
  zb_active_clear(&f->grid, NULL);
  forget_dormant(f); /* another world's fires */
  zb_active_clear(&f->grid, world);
}

static const zb_planet *planet(const zb_fire *f) { return zb_world_planet(f->grid.world); }

/* The cell axes of a chunk as unit vectors, at its centre. */
static void chunk_axes(const zb_planet *p, zb_chunk c, zb_dvec3 *ax, zb_dvec3 *ay, zb_dvec3 *centre) {
  zb_planet_pos o = {c, 128.0f, 128.0f, 0.0f}, px = {c, 129.0f, 128.0f, 0.0f}, py = {c, 128.0f, 129.0f, 0.0f};
  zb_dvec3 a = zb_planet_to_cart(p, &o);
  *ax = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &px), a));
  *ay = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &py), a));
  *centre = zb_dv_norm(a);
}

static void lean_weights(const zb_fire *f, fire_chunk *c) {
  zb_dvec3 ax, ay, centre, east, north, up;
  chunk_axes(planet(f), c->chunk, &ax, &ay, &centre);
  zb_planet_frame(centre, &east, &north, &up);
  double we = f->weather.wind_east, wn = f->weather.wind_north;
  if (f->wind_fn) {
    f->wind_fn(f->wind_ctx, c->chunk, &we, &wn);
  }
  zb_dvec3 wind = zb_dv_add(zb_dv_scale(east, we), zb_dv_scale(north, wn));
  c->wx = zb_dv_dot(wind, ax);
  c->wy = zb_dv_dot(wind, ay);
  for (int k = 0; k < 8; ++k) {
    double mx = DX[k] * c->cw, my = DY[k] * c->ch;
    c->dist[k] = sqrt(mx * mx + my * my);
    double along = (c->wx * mx + c->wy * my) / c->dist[k];
    c->lean[k] = zb_dexp(WIND_LEAN * along);
  }
}

void zb_fire_set_weather(zb_fire *f, const zb_fire_weather *w) {
  f->weather = *w;
  for (int i = 0; i < f->grid.count; ++i) {
    lean_weights(f, FC(f, i));
  }
}

const zb_fire_weather *zb_fire_weather_get(const zb_fire *f) { return &f->weather; }

void zb_fire_set_wind_source(zb_fire *f, zb_fire_wind_fn fn, void *ctx) {
  f->wind_fn = fn;
  f->wind_ctx = ctx;
}

/* The air's temperature over a chunk now: its climate, the season, the time
 * of day and its height. */
static double ambient_of(const zb_fire *f, const fire_chunk *c, double mean_height) {
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(zb_world_terrain(f->grid.world), c->chunk, &mean, &coldest, &warmest, &rain);
  zb_sky_place sky;
  zb_calendar_sky(zb_world_calendar(f->grid.world), zb_world_time(f->grid.world),
                  zb_planet_chunk_dir(planet(f), c->chunk), &sky);
  double t = mean + 0.5 * (warmest - coldest) * zb_dsin(2.0 * ZB_DPI * (sky.season - 0.5) / 4.0);
  return t + 6.0 * (sky.daylight - 0.5) - 0.0065 * (mean_height > 0.0 ? mean_height : 0.0);
}

/* Fuel moisture in balance with a climate: deserts 0.05, rainforests 0.29. */
static double equilibrium_moisture(const zb_fire *f, zb_chunk c) {
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(zb_world_terrain(f->grid.world), c, &mean, &coldest, &warmest, &rain);
  double m = 0.04 + rain / 12000.0;
  return m < 0.05 ? 0.05 : (m > 0.29 ? 0.29 : m);
}

/* Adds fuel of a kind to a cell. */
static void add_fuel(fire_chunk *c, double *mass, int x, int y, int kind, double kg_m2) {
  if (x < 0 || y < 0 || x >= N || y >= N) {
    return;
  }
  int i = y * N + x;
  c->fuel[i] += (float)kg_m2;
  mass[i * FUEL_KINDS + kind] += kg_m2;
}

/* Fills a chunk's cells from its terrain: heights, water and the fuel of its
 * plants and litter. */
static zb_result fill(zb_fire *f, fire_chunk *c, const zb_chunk_terrain *ct) {
  const zb_terrain *t = zb_world_terrain(f->grid.world);
  uint8_t *wkind = zb_alloc(f->a, (N + 1) * (N + 1));
  float *wsurf = zb_alloc(f->a, sizeof(float) * (N + 1) * (N + 1));
  double *mass = zb_calloc(f->a, sizeof(double) * CELLS * FUEL_KINDS);
  zb_result r = wkind && wsurf && mass ? zb_terrain_water_grid(t, f->a, ct, 256 / N, wkind, wsurf) : ZB_ERR_NOMEM;
  zb_free(f->a, wsurf);
  if (r != ZB_OK) {
    zb_free(f->a, wkind);
    zb_free(f->a, mass);
    return r;
  }
  double sum_h = 0.0;
  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      int i = y * N + x;
      const float *h0 = &ct->height[(2 * y) * ZB_HEIGHT_RES + 2 * x], *h1 = h0 + ZB_HEIGHT_RES;
      c->height[i] = 0.25f * (h0[0] + h0[1] + h1[0] + h1[1]);
      sum_h += c->height[i];
      int wet = (wkind[y * (N + 1) + x] != 0) + (wkind[y * (N + 1) + x + 1] != 0) +
                (wkind[(y + 1) * (N + 1) + x] != 0) + (wkind[(y + 1) * (N + 1) + x + 1] != 0);
      c->state[i] = wet >= 2 ? WATER : 0;
    }
  }
  /* Grassy ground (soil) carries a little fuel everywhere. */
  for (int y = 0; ct->material && y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      const uint8_t *m0 = &ct->material[(2 * y) * ZB_SURFACE_RES + 2 * x], *m1 = m0 + ZB_SURFACE_RES;
      int soil = (m0[0] == ZB_MAT_SOIL) + (m0[1] == ZB_MAT_SOIL) + (m1[0] == ZB_MAT_SOIL) + (m1[1] == ZB_MAT_SOIL);
      if (soil >= 2) {
        add_fuel(c, mass, x, y, FUEL_GRASS, 0.3);
      }
    }
  }
  /* Plants: a tree's crown and litter over the cells around it, shrubs and
   * ground cover on their own cell. */
  const zb_flora *fl = t->flora;
  for (uint32_t k = 0; ct->plants && k < ct->plant_count; ++k) {
    const zb_plant *pl = &ct->plants[k];
    if (pl->layer >= ZB_PLANT_LAYERS) {
      continue;
    }
    int x = (int)(pl->x * N / 65536u), y = (int)(pl->y * N / 65536u);
    double size = 1.0;
    if (fl && pl->species < fl->count) {
      const zb_species *sp = &fl->species[pl->species];
      size = (sp->height[0] + (sp->height[1] - sp->height[0]) * pl->size / 255.0) / 15.0;
    }
    if (pl->layer == ZB_PLANT_TREE) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          add_fuel(c, mass, x + dx, y + dy, FUEL_WOOD, 1.5 * size);
          add_fuel(c, mass, x + dx, y + dy, FUEL_LEAVES, 0.5);
        }
      }
    } else if (pl->layer == ZB_PLANT_SHRUB) {
      add_fuel(c, mass, x, y, FUEL_WOOD, 0.6 * size);
      add_fuel(c, mass, x, y, FUEL_LEAVES, 0.4);
    } else {
      add_fuel(c, mass, x, y, FUEL_GRASS, 0.4);
    }
  }
  for (int i = 0; i < CELLS; ++i) {
    int best = FUEL_NONE;
    for (int k = 1; k < FUEL_KINDS; ++k) {
      best = mass[i * FUEL_KINDS + k] > mass[i * FUEL_KINDS + best] ? k : best;
    }
    if ((c->state[i] & WATER) || !f->has_fuels) {
      c->fuel[i] = 0.0f;
      best = FUEL_NONE;
    }
    c->kind[i] = (uint8_t)best;
    c->fuel0[i] = c->fuel[i];
    c->moist[i] = (float)c->emc;
  }
  zb_free(f->a, wkind);
  zb_free(f->a, mass);
  c->ambient = ambient_of(f, c, sum_h / CELLS);
  return ZB_OK;
}

static void *take_in(void *ctx, const zb_active_chunk *ac, const zb_chunk_terrain *ct) {
  zb_fire *f = ctx;
  fire_chunk *c = zb_calloc(f->a, sizeof *c);
  if (!c) {
    return NULL;
  }
  size_t floats = (size_t)CELLS * 7, bytes = (size_t)CELLS * 2;
  c->block = zb_calloc(f->a, sizeof(float) * floats + bytes);
  c->t = c->block;
  if (!c->t) {
    zb_free(f->a, c);
    return NULL;
  }
  c->next = c->t + CELLS;
  c->gain = c->next + CELLS;
  c->fuel = c->gain + CELLS;
  c->fuel0 = c->fuel + CELLS;
  c->moist = c->fuel0 + CELLS;
  c->height = c->moist + CELLS;
  c->kind = (uint8_t *)(c->height + CELLS);
  c->state = c->kind + CELLS;
  c->chunk = ac->chunk;
  c->cw = ac->cw;
  c->ch = ac->ch;
  c->emc = equilibrium_moisture(f, ct->chunk);
  if (fill(f, c, ct) != ZB_OK) {
    free_chunk(f, c);
    return NULL;
  }
  lean_weights(f, c);
  /* Scars kept in the world: burnt until the trees have grown back. */
  size_t ne;
  const zb_edit *e = zb_deltas_get(zb_world_deltas(f->grid.world), c->chunk, &ne);
  double now = zb_world_time(f->grid.world);
  for (size_t k = 0; k < ne; ++k) {
    if (e[k].kind == ZB_EDIT_BURNT && e[k].x < N && e[k].y < N &&
        now - e[k].index * 60.0 < zb_regrowth_seconds(ZB_PLANT_TREE)) {
      c->state[e[k].y * N + e[k].x] |= BURNT | SCARRED;
    }
  }
  for (int k = 0; k < f->dormant_count; ++k) {
    if (zb_chunk_equal(f->dormant[k].chunk, c->chunk)) {
      catch_up(f, c, &f->dormant[k]);
      zb_free(f->a, f->dormant[k].cells);
      f->dormant[k] = f->dormant[--f->dormant_count];
      break;
    }
  }
  return c;
}

/* A chunk leaves: its scars go to the world, and a fire still burning in it
 * is remembered for when it comes back. */
static void let_go(void *ctx, void *data) {
  zb_fire *f = ctx;
  fire_chunk *c = data;
  if (c->scar_count > 0) {
    zb_world_burn(f->grid.world, c->chunk, c->scars, c->scar_count, zb_world_time(f->grid.world));
  }
  int burning = 0;
  for (int i = 0; c->awake && i < CELLS; ++i) {
    burning += (c->state[i] & BURNING) ? 1 : 0;
  }
  dormant_fire *d = burning && f->dormant_count < ZB_FIRE_MAX_CHUNKS ? &f->dormant[f->dormant_count] : NULL;
  uint16_t *cells = d ? zb_alloc(f->a, sizeof(uint16_t) * (size_t)burning) : NULL;
  if (cells) {
    d->chunk = c->chunk;
    d->left_at = zb_world_time(f->grid.world);
    d->cells = cells;
    d->count = 0;
    for (int i = 0; i < CELLS; ++i) {
      if (c->state[i] & BURNING) {
        cells[d->count++] = (uint16_t)i;
      }
    }
    f->dormant_count++;
  }
  free_chunk(f, c);
}

/* A chunk comes back to a fire that burnt while nobody watched (concept 5.2,
 * catch-up): shortly after, the fire goes on where it was; later, it has
 * burnt the dry fuel around it, up to a distance that grows with the days
 * gone (about 1.5 km a day in dry fuel, less in damp climates, within the
 * chunk), and those cells are scarred as of when the chunk left. */
static void catch_up(zb_fire *f, fire_chunk *c, const dormant_fire *d) {
  double elapsed = zb_world_time(f->grid.world) - d->left_at;
  if (elapsed < 600.0) {
    for (int k = 0; k < d->count; ++k) {
      int i = d->cells[k];
      if (c->fuel[i] >= MIN_FUEL && !(c->state[i] & WATER)) {
        c->state[i] |= BURNING;
        c->t[i] = (float)FLAME;
        c->awake = true;
      }
    }
    return;
  }
  double reach = elapsed / 86400.0 * 1500.0 * (1.0 - c->emc / EXTINCTION);
  int steps = (int)(reach / (c->cw < c->ch ? c->cw : c->ch));
  steps = steps > N ? N : steps;
  int32_t *queue = zb_alloc(f->a, sizeof(int32_t) * CELLS);
  int16_t *dist = zb_alloc(f->a, sizeof(int16_t) * CELLS);
  if (!queue || !dist) {
    zb_free(f->a, queue);
    zb_free(f->a, dist);
    return;
  }
  memset(dist, 0xff, sizeof(int16_t) * CELLS);
  int head = 0, tail = 0;
  for (int k = 0; k < d->count; ++k) {
    queue[tail++] = d->cells[k];
    dist[d->cells[k]] = 0;
  }
  c->scar_count = 0;
  while (head < tail) {
    int i = queue[head++], x = i % N, y = i / N;
    if (c->fuel[i] >= MIN_FUEL && !(c->state[i] & (WATER | BURNT))) {
      c->fuel[i] *= 0.1f;
      c->state[i] |= BURNT;
      c->scars[c->scar_count++] = (uint16_t)i;
      f->burnt++;
    }
    for (int k = 0; k < 8 && dist[i] < steps; ++k) {
      int nx = x + DX[k], ny = y + DY[k], j = ny * N + nx;
      if (nx < 0 || ny < 0 || nx >= N || ny >= N || dist[j] >= 0) {
        continue;
      }
      dist[j] = (int16_t)(dist[i] + 1);
      if (c->fuel[j] >= MIN_FUEL && c->moist[j] < EXTINCTION && !(c->state[j] & WATER)) {
        queue[tail++] = j;
      }
    }
  }
  if (c->scar_count > 0 && zb_world_burn(f->grid.world, c->chunk, c->scars, c->scar_count, d->left_at) == ZB_OK) {
    for (int k = 0; k < c->scar_count; ++k) {
      c->state[c->scars[k]] |= SCARRED;
    }
    c->scar_count = 0;
  }
  zb_free(f->a, queue);
  zb_free(f->a, dist);
}

/* ------------------------------------------------------------- one step */

static void wake(fire_chunk *c) {
  if (!c->awake) {
    c->awake = true;
    memset(c->gain, 0, sizeof(float) * CELLS);
  }
}

static double heat_capacity(const zb_fire *f, const fire_chunk *c, int i) {
  double fine = c->fuel[i] < FINE_FUEL ? c->fuel[i] : FINE_FUEL;
  return BASE_HEAT + fine * f->fuels[c->kind[i]].cp + fine * c->moist[i] * WATER_HEAT;
}

/* A random number 0 .. 1 for a cell and purpose, the same everywhere. */
static double chance(const fire_chunk *c, int i, uint64_t step, uint64_t purpose) {
  uint64_t h = zb_hash_combine(zb_hash_combine((uint64_t)c->chunk.face, (uint64_t)(uint32_t)c->chunk.x),
                               zb_hash_combine((uint64_t)(uint32_t)c->chunk.y, (uint64_t)i));
  h = zb_hash_combine(zb_hash_combine(h, step), purpose);
  return (double)(h >> 11) * (1.0 / 9007199254740992.0);
}

static double intensity(const zb_fire *f, const fire_chunk *c, int i) {
  const fuel_kind *k = &f->fuels[c->kind[i]];
  double wet = 1.0 - c->moist[i] / EXTINCTION;
  return k->rate * k->heat * (wet > 0.1 ? wet : 0.1);
}

static bool ignite_cell(zb_fire *f, int32_t cell) {
  fire_chunk *c = chunk_of(f, cell);
  int i = cell % CELLS;
  if ((c->state[i] & (WATER | BURNING)) || c->fuel[i] < MIN_FUEL || c->moist[i] >= EXTINCTION) {
    return false;
  }
  c->state[i] |= BURNING;
  c->t[i] = (float)FLAME;
  wake(c);
  return true;
}

/* Embers from a crown fire, carried downwind. */
static void embers(zb_fire *f, int s, int i, double power) {
  fire_chunk *c = FC(f, s);
  double wind = sqrt(c->wx * c->wx + c->wy * c->wy);
  if (c->kind[i] != FUEL_WOOD || wind < 2.0 || chance(c, i, f->steps, 1) >= EMBERS * ZB_FIRE_STEP * power / 2e5) {
    return;
  }
  f->embers++;
  double d = wind * (2.0 + 6.0 * chance(c, i, f->steps, 2)), side = (chance(c, i, f->steps, 3) - 0.5) * 0.4 * d;
  double ux = c->wx / wind, uy = c->wy / wind;
  double mx = ux * d - uy * side, my = uy * d + ux * side;
  int32_t cell = zb_active_find(&f->grid, c->chunk, (i % N) + mx / c->cw, (i / N) + my / c->ch);
  if (cell < 0) {
    return;
  }
  fire_chunk *tc = chunk_of(f, cell);
  if (tc->moist[cell % CELLS] < 0.8 * EXTINCTION && chance(c, i, f->steps, 4) < 0.5 && ignite_cell(f, cell)) {
    f->spots++;
  }
}

/* A burning cell gives a share of its heat to its neighbours. */
static void radiate(zb_fire *f, int s, int i) {
  fire_chunk *c = FC(f, s);
  int x = i % N, y = i / N;
  int32_t to[8];
  double w[8], sum = 0.0;
  bool inside = x > 0 && y > 0 && x < N - 1 && y < N - 1;
  for (int k = 0; k < 8; ++k) {
    to[k] = inside ? s * CELLS + i + DY[k] * N + DX[k] : zb_active_locate(&f->grid, s, x + DX[k], y + DY[k]);
    w[k] = 0.0;
    if (to[k] >= 0) {
      const fire_chunk *tc = chunk_of(f, to[k]);
      double rise = (tc->height[to[k] % CELLS] - c->height[i]) / c->dist[k];
      w[k] = c->lean[k] * slope_weight(rise) / (c->dist[k] * c->dist[k]);
      sum += w[k];
    }
  }
  double power = intensity(f, c, i);
  for (int k = 0; k < 8 && sum > 0.0; ++k) {
    if (to[k] >= 0) {
      fire_chunk *tc = chunk_of(f, to[k]);
      wake(tc);
      tc->gain[to[k] % CELLS] += (float)(RADIATED * power * w[k] / sum);
    }
  }
  embers(f, s, i, power);
}

/* A neighbour's temperature above its ambient (0 beyond the simulation). */
static float temp_at(const zb_fire *f, int s, int x, int y) {
  int32_t cell = zb_active_locate(&f->grid, s, x, y);
  return cell < 0 ? 0.0f : (chunk_of(f, cell)->awake ? chunk_of(f, cell)->t[cell % CELLS] : 0.0f);
}

/* Heat, moisture, ignition and burning of one cell. */
static void update_cell(zb_fire *f, int s, int i, double rain_kg) {
  fire_chunk *c = FC(f, s);
  int x = i % N, y = i / N;
  float t = c->t[i];
  if (c->state[i] & WATER) {
    c->next[i] = 0.0f;
    return;
  }
  bool inside = x > 0 && y > 0 && x < N - 1 && y < N - 1;
  double around = inside ? (double)c->t[i - 1] + c->t[i + 1] + c->t[i - N] + c->t[i + N]
                         : (double)temp_at(f, s, x - 1, y) + temp_at(f, s, x + 1, y) + temp_at(f, s, x, y - 1) +
                               temp_at(f, s, x, y + 1);
  /* A quiet cell (no heat here or next door, settled fuel, no rain) stays as it is. */
  if (t == 0.0f && around == 0.0 && c->gain[i] == 0.0f && rain_kg == 0.0 && c->moist[i] == (float)c->emc &&
      !(c->state[i] & BURNING)) {
    c->next[i] = 0.0f;
    return;
  }
  /* Moisture: towards the climate's balance (settling on it), up with rain. */
  double fuel = c->fuel[i];
  double m = c->moist[i] + (c->emc - c->moist[i]) * ZB_FIRE_STEP / DRYING;
  if (fabs(m - c->emc) < 1e-4) {
    m = c->emc;
  }
  if (rain_kg > 0.0) {
    m += rain_kg / (fuel > 0.2 ? fuel : 0.2);
    m = m > 3.0 ? 3.0 : m;
  }
  c->moist[i] = (float)m;
  if (c->state[i] & BURNING) {
    const fuel_kind *k = &f->fuels[c->kind[i]];
    double wet = 1.0 - m / EXTINCTION;
    fuel -= k->rate * (wet > 0.1 ? wet : 0.1) * ZB_FIRE_STEP;
    c->fuel[i] = (float)(fuel > 0.0 ? fuel : 0.0);
    if (fuel < MIN_FUEL || m >= EXTINCTION) {
      c->state[i] &= (uint8_t)~BURNING;
      if (c->fuel[i] < 0.5f * c->fuel0[i] && !(c->state[i] & BURNT)) {
        c->state[i] |= BURNT;
        f->burnt++;
        if (c->scar_count < CELLS) {
          c->scars[c->scar_count++] = (uint16_t)i;
        }
      }
    }
    c->next[i] = (float)FLAME;
    return;
  }
  double cap = heat_capacity(f, c, i);
  double flux = c->gain[i] + CONDUCT * (around - 4.0 * t) - LOSS * t;
  double nt = t + flux * ZB_FIRE_STEP / cap;
  /* Water in the fuel boils off before the fuel gets hotter than 100 deg C. */
  double boil = 100.0 - c->ambient, from = t > boil ? t : boil;
  if (nt > from && m > 0.0 && fuel > 0.0) {
    double fine = fuel < FINE_FUEL ? fuel : FINE_FUEL;
    double spare = (nt - from) * cap, water = m * fine;
    double gone = spare / LATENT < water ? spare / LATENT : water;
    c->moist[i] = (float)(m - gone / fine);
    nt -= gone * LATENT / cap;
  }
  c->next[i] = (float)(nt < FLAME ? nt : FLAME); /* nothing gets hotter than the flames */
  nt = c->next[i];
  /* Ignition, a little different for every cell. */
  if (fuel >= MIN_FUEL && c->moist[i] < EXTINCTION) {
    const fuel_kind *k = &f->fuels[c->kind[i]];
    double ignition = k->ignition * (0.9 + 0.2 * chance(c, i, 0, 5)) - c->ambient;
    if (nt >= ignition) {
      c->state[i] |= BURNING;
      c->next[i] = (float)FLAME;
    }
  }
}

static void step(zb_fire *f) {
  double rain_kg = f->weather.rain / 3600.0 * ZB_FIRE_STEP;
  for (int s = 0; s < f->grid.count; ++s) {
    fire_chunk *c = FC(f, s);
    if (c->awake) {
      memset(c->gain, 0, sizeof(float) * CELLS);
    }
  }
  for (int s = 0; s < f->grid.count; ++s) {
    fire_chunk *c = FC(f, s);
    for (int i = 0; c->awake && i < CELLS; ++i) {
      if (c->state[i] & BURNING) {
        radiate(f, s, i);
      }
    }
  }
  for (int s = 0; s < f->grid.count; ++s) {
    fire_chunk *c = FC(f, s);
    if (!c->awake) {
      continue;
    }
    for (int i = 0; i < CELLS; ++i) {
      update_cell(f, s, i, rain_kg);
    }
  }
  /* Swap, and let chunks with nothing going on sleep (their heat is zero). */
  for (int s = 0; s < f->grid.count; ++s) {
    fire_chunk *c = FC(f, s);
    if (!c->awake) {
      continue;
    }
    float *t = c->t;
    c->t = c->next;
    c->next = t;
    bool busy = false, edge = false;
    for (int i = 0; i < CELLS; ++i) {
      if ((c->state[i] & BURNING) || c->t[i] > AWAKE || c->t[i] < -AWAKE) {
        busy = true;
        int x = i % N, y = i / N;
        edge = edge || x < 2 || y < 2 || x >= N - 2 || y >= N - 2;
      }
    }
    if (!busy) {
      c->awake = false;
      memset(c->t, 0, sizeof(float) * CELLS);
    }
    for (int k = 0; k < 8 && edge; ++k) { /* heat at an edge flows into the neighbours */
      int nb = f->grid.slot[s]->near[k];
      if (nb >= 0) {
        wake(FC(f, nb));
      }
    }
  }
  f->steps++;
}

/* Burnt-out cells go into the world's deltas, a chunk at a time. */
static void flush_scars(zb_fire *f) {
  for (int s = 0; s < f->grid.count; ++s) {
    fire_chunk *c = FC(f, s);
    if (c->scar_count > 0 &&
        zb_world_burn(f->grid.world, c->chunk, c->scars, c->scar_count, zb_world_time(f->grid.world)) == ZB_OK) {
      for (int k = 0; k < c->scar_count; ++k) {
        c->state[c->scars[k]] |= SCARRED;
      }
      c->scar_count = 0;
    }
  }
}

void zb_fire_update(zb_fire *f, const zb_planet_pos *focus, int focus_count, double seconds) {
  f->since_survey += seconds;
  if (f->since_survey >= 1.0 || f->grid.count == 0) {
    f->since_survey = 0.0;
    zb_active_survey(&f->grid, focus, focus_count);
    flush_scars(f);
    for (int s = 0; f->wind_fn && s < f->grid.count; ++s) {
      lean_weights(f, FC(f, s)); /* the local wind changes with the air */
    }
    for (int s = 0; s < f->grid.count; ++s) {
      double h = 0.0;
      for (int i = 0; i < CELLS; i += 97) {
        h += FC(f, s)->height[i];
      }
      FC(f, s)->ambient = ambient_of(f, FC(f, s), h / ((CELLS + 96) / 97));
    }
  }
  f->pending += seconds;
  int steps = 0;
  while (f->pending >= ZB_FIRE_STEP && steps < 4) {
    f->pending -= ZB_FIRE_STEP;
    step(f);
    steps++;
  }
  if (f->pending > ZB_FIRE_STEP) {
    f->pending = ZB_FIRE_STEP; /* behind: drop time rather than spiral */
  }
}

/* ------------------------------------------------------- outside access */

static bool ignite_fn(void *ctx, int32_t cell) { return ignite_cell(ctx, cell); }

int zb_fire_ignite(zb_fire *f, const zb_planet_pos *pos, double radius) {
  return zb_active_around(&f->grid, pos, radius, ignite_fn, f);
}

typedef struct heat_ctx {
  zb_fire *f;
  double joules_per_cell;
} heat_ctx;

static bool heat_fn(void *ctx, int32_t cell) {
  zb_fire *f = ((heat_ctx *)ctx)->f;
  fire_chunk *c = chunk_of(f, cell);
  int i = cell % CELLS;
  if (c->state[i] & (WATER | BURNING)) {
    return false;
  }
  double per_m2 = ((heat_ctx *)ctx)->joules_per_cell / (c->cw * c->ch);
  wake(c);
  c->t[i] += (float)(per_m2 / heat_capacity(f, c, i));
  return true;
}

void zb_fire_add_heat(zb_fire *f, const zb_planet_pos *pos, double radius, double joules) {
  heat_ctx count = {f, 0.0};
  int n = zb_active_around(&f->grid, pos, radius, heat_fn, &count); /* zero joules: counts the cells */
  if (n > 0) {
    heat_ctx h = {f, joules / n};
    zb_active_around(&f->grid, pos, radius, heat_fn, &h);
  }
}

void zb_fire_cell_at(const zb_fire *f, const zb_planet_pos *pos, zb_fire_cell *out) {
  memset(out, 0, sizeof *out);
  out->material = -1;
  int32_t cell = zb_active_cell_at(&f->grid, pos);
  if (cell < 0) {
    return;
  }
  const fire_chunk *c = chunk_of(f, cell);
  int i = cell % CELLS;
  out->loaded = true;
  out->burning = (c->state[i] & BURNING) != 0;
  out->burnt = (c->state[i] & BURNT) != 0;
  out->water = (c->state[i] & WATER) != 0;
  out->temperature = c->ambient + (c->awake ? c->t[i] : 0.0f);
  out->fuel = c->fuel[i];
  out->moisture = c->moist[i];
  out->material = c->kind[i] != FUEL_NONE ? f->fuels[c->kind[i]].material : -1;
}

void zb_fire_get_stats(const zb_fire *f, zb_fire_stats *out) {
  memset(out, 0, sizeof *out);
  out->chunks = f->grid.count;
  out->burnt = f->burnt;
  out->spots = f->spots;
  out->embers = f->embers;
  out->max_temperature = -273.15;
  for (int s = 0; s < f->grid.count; ++s) {
    const fire_chunk *c = FC(f, s);
    out->awake += c->awake ? 1 : 0;
    for (int i = 0; i < CELLS; ++i) {
      out->burning += (c->state[i] & BURNING) ? 1 : 0;
      double t = c->ambient + (c->awake ? c->t[i] : 0.0f);
      out->max_temperature = t > out->max_temperature ? t : out->max_temperature;
    }
  }
}

uint64_t zb_fire_hash(const zb_fire *f) {
  uint64_t h = zb_hash64(&f->steps, sizeof f->steps, 0x66697265);
  for (int s = 0; s < f->grid.count; ++s) {
    const fire_chunk *c = FC(f, s);
    h = zb_hash64(&c->chunk, sizeof c->chunk, h);
    h = zb_hash64(c->t, sizeof(float) * CELLS, h);
    h = zb_hash64(c->fuel, sizeof(float) * CELLS, h);
    h = zb_hash64(c->moist, sizeof(float) * CELLS, h);
    h = zb_hash64(c->state, CELLS, h);
  }
  return h;
}

void zb_fire_each_burning(const zb_fire *f, zb_fire_cell_fn fn, void *ctx) {
  for (int s = 0; s < f->grid.count; ++s) {
    const fire_chunk *c = FC(f, s);
    for (int i = 0; c->awake && i < CELLS; ++i) {
      if (c->state[i] & BURNING) {
        zb_planet_pos p = {c->chunk, (float)(((i % N) + 0.5) * 256.0 / N), (float)(((i / N) + 0.5) * 256.0 / N),
                           c->height[i]};
        fn(ctx, &p, intensity(f, c, i), c->fuel[i]);
      }
    }
  }
}

void zb_fire_each_smoke(const zb_fire *f, zb_fire_smoke_fn fn, void *ctx) {
  for (int s = 0; s < f->grid.count; ++s) {
    const fire_chunk *c = FC(f, s);
    for (int i = 0; c->awake && i < CELLS; ++i) {
      if (c->state[i] & BURNING) {
        const fuel_kind *k = &f->fuels[c->kind[i]];
        double wet = 1.0 - c->moist[i] / EXTINCTION;
        double kg = k->rate * (wet > 0.1 ? wet : 0.1) * k->smoke * c->cw * c->ch;
        zb_planet_pos p = zb_active_cell_pos(&f->grid, s * CELLS + i, c->height[i]);
        fn(ctx, &p, kg);
      }
    }
  }
}

int zb_fire_dormant_count(const zb_fire *f) { return f->dormant_count; }

void zb_fire_soak(zb_fire *f, const zb_planet_pos *pos) {
  int32_t cell = zb_active_cell_at(&f->grid, pos);
  if (cell < 0) {
    return;
  }
  fire_chunk *c = chunk_of(f, cell);
  int i = cell % CELLS;
  c->state[i] &= (uint8_t)~BURNING;
  c->moist[i] = c->moist[i] > 1.0f ? c->moist[i] : 1.0f;
  if (c->awake && c->t[i] > 0.0f) {
    c->t[i] = 0.0f;
  }
}

void zb_fire_ground_changed(zb_fire *f, const zb_chunk_terrain *ct) {
  int s = zb_active_slot(&f->grid, ct->chunk);
  if (s < 0 || !ct->height) {
    return;
  }
  fire_chunk *c = FC(f, s);
  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      const float *h0 = &ct->height[(2 * y) * ZB_HEIGHT_RES + 2 * x], *h1 = h0 + ZB_HEIGHT_RES;
      c->height[y * N + x] = 0.25f * (h0[0] + h0[1] + h1[0] + h1[1]);
    }
  }
}
