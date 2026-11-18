/* SPDX-License-Identifier: MIT */
#include "sim/tier.h"

#include <string.h>

#include "core/hash.h"
#include "core/log.h"

#define TICK_HZ 60
#define SOAK_DEPTH 0.02 /* metres of standing water that put a fire out */

struct zb_tier {
  zb_tier_config cfg;
  zb_fire *fire;
  zb_surface_water *water;
  zb_air *air;
  zb_weather_control weather;
  uint64_t ticks;
  double spent[ZB_TIER_SYSTEMS]; /* ms this second */
  zb_tier_stats stats;
  int warned_at; /* over_budget count when last warned */
};

static void air_wind(void *ctx, zb_chunk c, double *east, double *north) { zb_air_chunk_wind(ctx, c, east, north); }

zb_tier *zb_tier_create(const zb_tier_config *cfg) {
  zb_tier *t = zb_calloc(cfg->a, sizeof *t);
  if (!t) {
    return NULL;
  }
  t->cfg = *cfg;
  t->fire = cfg->materials ? zb_fire_create(cfg->a, cfg->world, cfg->materials, cfg->radius) : NULL;
  t->water = zb_surface_water_create(cfg->a, cfg->world, cfg->radius);
  t->air = zb_air_create(cfg->a, cfg->world, cfg->radius);
  if ((cfg->materials && !t->fire) || !t->water || !t->air) {
    zb_tier_destroy(t);
    return NULL;
  }
  if (t->fire) {
    zb_fire_set_wind_source(t->fire, air_wind, t->air); /* fire leans with the local wind */
  }
  t->stats.budget_ms = ZB_TIER_BUDGET_MS;
  return t;
}

void zb_tier_destroy(zb_tier *t) {
  if (!t) {
    return;
  }
  zb_fire_destroy(t->fire);
  zb_surface_water_destroy(t->water);
  zb_air_destroy(t->air);
  zb_free(t->cfg.a, t);
}

void zb_tier_set_world(zb_tier *t, zb_world *world) {
  t->cfg.world = world;
  if (t->fire) {
    zb_fire_set_world(t->fire, world);
  }
  zb_surface_water_set_world(t->water, world);
  zb_air_set_world(t->air, world);
}

zb_fire *zb_tier_fire(const zb_tier *t) { return t->fire; }
zb_surface_water *zb_tier_water(const zb_tier *t) { return t->water; }
zb_air *zb_tier_air(const zb_tier *t) { return t->air; }
zb_weather_control *zb_tier_weather(zb_tier *t) { return &t->weather; }

void zb_tier_apply_weather(zb_tier *t, const zb_weather *w) {
  if (t->fire) {
    zb_fire_weather fw = {w->wind_east, w->wind_north, w->rain + w->snow};
    zb_fire_set_weather(t->fire, &fw);
  }
  zb_air_set_wind(t->air, w->wind_east, w->wind_north);
  zb_surface_water_set_rain(t->water, w->rain);
}

int zb_tier_lightning(zb_tier *t, const zb_planet_pos *pos) {
  t->weather.strikes++;
  return t->fire ? zb_fire_ignite(t->fire, pos, 3.0) : 0;
}

static double now_ms(const zb_tier *t) {
  return t->cfg.clock_ns ? (double)t->cfg.clock_ns(t->cfg.clock_ctx) / 1e6 : 0.0;
}

/* The world's weather at the first focus point, unless the scripts set one;
 * storms send lightning to the highest of a few places nearby. */
static void weather_second(zb_tier *t, const zb_planet_pos *focus) {
  zb_world *world = t->cfg.world;
  zb_weather *w = &t->weather.now;
  if (!t->weather.overridden) {
    zb_planet_pos ground = *focus;
    ground.alt = (float)zb_world_ground_height(world, focus);
    zb_weather_at(zb_world_terrain(world), zb_world_calendar(world), &ground, zb_world_time(world), w);
    zb_tier_apply_weather(t, w);
  }
  uint64_t second = t->ticks / TICK_HZ;
  double roll = (double)(zb_hash_combine(t->cfg.seed, second) >> 11) * (1.0 / 9007199254740992.0);
  if (!t->fire || w->storm <= 0.0 || roll >= 0.03 * w->storm) {
    return;
  }
  zb_planet_pos best = *focus;
  double top = -1e9;
  for (uint64_t k = 0; k < 6; ++k) {
    double new_heading;
    uint64_t h = zb_hash_combine(zb_hash_combine(t->cfg.seed, second), k + 1);
    double heading = (double)(h & 0xffff) / 65536.0 * 6.283185307179586, dist = 40.0 + (double)((h >> 16) & 0xff);
    zb_planet_pos p = zb_planet_move(zb_world_planet(world), focus, heading, dist, 0.0, 0.0, &new_heading);
    double height = zb_world_ground_height(world, &p);
    if (height > top) {
      top = height;
      best = p;
    }
  }
  best.alt = (float)top;
  int lit = zb_tier_lightning(t, &best);
  zb_info("lightning struck %.0f m from the player (%d cell(s) caught)",
          zb_planet_distance(zb_world_planet(world), focus, &best), lit);
}

typedef struct smoke_out {
  zb_air *air;
  double seconds;
} smoke_out;

static void emit_smoke(void *ctx, const zb_planet_pos *pos, double kg_per_s) {
  smoke_out *so = ctx;
  zb_air_emit(so->air, pos, ZB_GAS_SMOKE, kg_per_s * so->seconds);
}

static void soak_fire(void *ctx, const zb_planet_pos *pos, double depth) {
  (void)depth;
  zb_fire_soak(ctx, pos);
}

void zb_tier_update(zb_tier *t, double seconds) {
  zb_planet_pos focus[ZB_STREAM_MAX_FOCUS];
  int n = zb_world_focus(t->cfg.world, focus, ZB_STREAM_MAX_FOCUS);
  double t0 = now_ms(t), t1, t2, t3, t4;
  if (t->fire) {
    zb_fire_update(t->fire, focus, n, seconds);
  }
  t1 = now_ms(t);
  zb_surface_water_update(t->water, focus, n, seconds);
  t2 = now_ms(t);
  if (t->fire) {
    smoke_out smoke = {t->air, seconds};
    zb_fire_each_smoke(t->fire, emit_smoke, &smoke);
  }
  zb_air_update(t->air, focus, n, seconds);
  t3 = now_ms(t);
  t->ticks++;
  if (t->ticks % TICK_HZ == 0 && n > 0) {
    weather_second(t, &focus[0]);
    if (t->fire) {
      zb_surface_water_each_wet(t->water, SOAK_DEPTH, soak_fire, t->fire);
    }
  }
  t4 = now_ms(t);
  t->spent[ZB_TIER_FIRE] += t1 - t0;
  t->spent[ZB_TIER_WATER] += t2 - t1;
  t->spent[ZB_TIER_AIR] += t3 - t2;
  t->spent[ZB_TIER_WEATHER] += t4 - t3;
  if (t->ticks % TICK_HZ == 0) {
    t->stats.total_ms = 0.0;
    for (int s = 0; s < ZB_TIER_SYSTEMS; ++s) {
      t->stats.ms[s] = t->spent[s] / TICK_HZ;
      t->stats.total_ms += t->stats.ms[s];
      t->spent[s] = 0.0;
    }
    if (t->stats.total_ms > t->stats.budget_ms) {
      t->stats.over_budget++;
      if (t->stats.over_budget - t->warned_at >= 60 || t->warned_at == 0) {
        zb_warn("the active tier took %.1f ms a tick (budget %.1f)", t->stats.total_ms, t->stats.budget_ms);
        t->warned_at = t->stats.over_budget;
      }
    }
  }
}

void zb_tier_get_stats(const zb_tier *t, zb_tier_stats *out) { *out = t->stats; }
