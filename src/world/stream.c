/* SPDX-License-Identifier: MIT */
#include "world/stream.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/array.h"
#include "core/dmath.h"

#define REFRESH_DISTANCE 32.0 /* metres a focus moves before the rings are recomputed */
#define HALF_DIAGONAL 184.0   /* metres: half the diagonal of the largest chunk */
#define CAP_SAMPLES 32        /* boundary samples for a cap's bounding box on a face */
#define RETRY_FRAMES 30       /* frames before retrying a chunk that ran out of memory */
#define COLUMN_CACHE 1024     /* columns per face scan with cached tangents */

typedef struct entry {
  zb_chunk_terrain data; /* what readers see */
  zb_chunk_terrain work; /* what a job writes: a copy of data plus new stages */
  zb_dvec3 dir;          /* chunk centre */
  zb_job *job;
  int job_stage;
  zb_result job_result;
  const zb_terrain *terrain;
  const zb_allocator *chunk_alloc;
  int want, keep, target;
  uint64_t epoch;       /* refresh that set want and keep */
  uint64_t last_wanted; /* refresh in which the chunk was last inside the rings */
  uint64_t retry_frame;
  uint32_t version;
} entry;

typedef struct candidate {
  entry *e;
  double priority;
} candidate;

typedef struct focus_state {
  zb_dvec3 dir;       /* focus direction */
  zb_dvec3 predicted; /* direction after lookahead seconds */
  zb_dvec3 view;      /* view direction in planet space */
  double far;         /* far radius for this focus */
} focus_state;

struct zb_stream {
  zb_stream_config cfg;
  const zb_terrain *t;
  zb_jobs *jobs;
  bool own_jobs;
  zb_map map;          /* chunk key -> entry * */
  zb_array inflight;   /* entry * */
  zb_array candidates; /* candidate */
  uint64_t epoch, frame;
  int last_count;
  focus_state last[ZB_STREAM_MAX_FOCUS];
  focus_state now[ZB_STREAM_MAX_FOCUS];
  size_t bytes, wanted;
  uint64_t generated, evicted, cancelled;
  double column_tan[COLUMN_CACHE]; /* face-coordinate tangents of the scanned columns */
};

void zb_stream_default_config(zb_stream_config *c, zb_stream_profile profile) {
  memset(c, 0, sizeof *c);
  c->a = zb_heap_allocator();
  c->chunk_alloc = zb_heap_allocator();
  c->hysteresis = 1.25f;
  c->lookahead = 2.0f;
  c->inline_budget_ms = 4.0f;
  c->far_per_altitude = 4.0f;
  switch (profile) {
    case ZB_STREAM_DESKTOP:
      c->near_radius = 1536.0f;
      c->far_radius = 12000.0f;
      c->max_far_radius = 24000.0f;
      c->memory_cap = (size_t)512 << 20;
      c->max_jobs = 8;
      break;
    case ZB_STREAM_WEB:
      c->near_radius = 1024.0f;
      c->far_radius = 8000.0f;
      c->max_far_radius = 16000.0f;
      c->memory_cap = (size_t)256 << 20;
      c->max_jobs = 4;
      break;
    default: /* mobile */
      c->near_radius = 640.0f;
      c->far_radius = 5000.0f;
      c->max_far_radius = 10000.0f;
      c->memory_cap = (size_t)128 << 20;
      c->max_jobs = 2;
      c->inline_budget_ms = 3.0f;
      break;
  }
}

float zb_stream_profile_ecology_radius(zb_stream_profile profile) {
  switch (profile) {
    case ZB_STREAM_DESKTOP: return 768.0f;
    case ZB_STREAM_WEB: return 512.0f;
    default: return 320.0f;
  }
}

zb_stream *zb_stream_create(const zb_terrain *t, const zb_stream_config *cfg) {
  zb_stream *s = zb_alloc(cfg->a, sizeof *s);
  if (!s) {
    return NULL;
  }
  memset(s, 0, sizeof *s);
  s->cfg = *cfg;
  if (!s->cfg.chunk_alloc) {
    s->cfg.chunk_alloc = zb_heap_allocator();
  }
  s->t = t;
  s->jobs = cfg->jobs;
  if (!s->jobs) {
    s->jobs = zb_jobs_create(cfg->a, NULL, 0);
    s->own_jobs = true;
    if (!s->jobs) {
      zb_free(cfg->a, s);
      return NULL;
    }
  }
  zb_map_init(&s->map, cfg->a);
  zb_array_init(&s->inflight, cfg->a, sizeof(entry *));
  zb_array_init(&s->candidates, cfg->a, sizeof(candidate));
  return s;
}

static entry *lookup(const zb_stream *s, zb_chunk c) {
  uint64_t v;
  return zb_map_get(&s->map, zb_chunk_key(c), &v) ? (entry *)(uintptr_t)v : NULL;
}

/* Frees the arrays a job added to work that data does not share. */
static void discard_work(entry *e) {
  if (e->work.height && e->work.height != e->data.height) {
    zb_free(e->chunk_alloc, e->work.height);
  }
  if (e->work.material && e->work.material != e->data.material) {
    zb_free(e->chunk_alloc, e->work.material);
  }
  if (e->work.plants && e->work.plants != e->data.plants) {
    zb_free(e->chunk_alloc, e->work.plants);
  }
  e->work = e->data;
}

static void free_entry(zb_stream *s, entry *e) {
  s->bytes -= e->data.stage >= 0 ? zb_chunk_terrain_bytes(&e->data) : 0;
  zb_chunk_terrain_release(e->chunk_alloc, &e->data);
  zb_free(s->cfg.a, e);
}

void zb_stream_destroy(zb_stream *s) {
  if (!s) {
    return;
  }
  for (size_t i = 0; i < s->inflight.len; ++i) {
    entry *e = *(entry **)zb_array_at(&s->inflight, i);
    if (!zb_job_cancel(s->jobs, e->job)) {
      zb_job_wait(s->jobs, e->job);
      discard_work(e);
    }
    zb_job_release(s->jobs, e->job);
    e->job = NULL;
  }
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    free_entry(s, (entry *)(uintptr_t)value);
  }
  zb_map_free(&s->map);
  zb_array_free(&s->inflight);
  zb_array_free(&s->candidates);
  if (s->own_jobs) {
    zb_jobs_destroy(s->jobs);
  }
  zb_free(s->cfg.a, s);
}

void zb_stream_set_radii(zb_stream *s, float near_radius, float far_radius) {
  s->cfg.near_radius = near_radius;
  s->cfg.far_radius = far_radius;
  s->cfg.max_far_radius = 2.0f * far_radius;
  s->last_count = -1; /* recompute the rings */
}

void zb_stream_get_radii(const zb_stream *s, float *near_radius, float *far_radius) {
  *near_radius = s->cfg.near_radius;
  *far_radius = s->cfg.far_radius;
}

/* ---- Jobs ------------------------------------------------------------------ */

static void generate_job(void *arg) {
  entry *e = arg;
  e->job_result = zb_terrain_generate(e->terrain, e->chunk_alloc, &e->work, e->job_stage);
}

/* Applies finished jobs. */
static void collect(zb_stream *s) {
  for (size_t i = 0; i < s->inflight.len;) {
    entry *e = *(entry **)zb_array_at(&s->inflight, i);
    zb_job_state st = zb_job_get_state(s->jobs, e->job);
    if (st != ZB_JOB_DONE && st != ZB_JOB_CANCELLED) {
      ++i;
      continue;
    }
    zb_job_release(s->jobs, e->job);
    e->job = NULL;
    zb_array_remove_swap(&s->inflight, i);
    if (st == ZB_JOB_CANCELLED) {
      continue;
    }
    /* The job leaves work consistent even when it ran out of memory: the
     * stages it finished are complete, the failed one left nothing behind. */
    s->bytes -= e->data.stage >= 0 ? zb_chunk_terrain_bytes(&e->data) : 0;
    bool changed = e->work.stage != e->data.stage;
    e->data = e->work;
    if (changed && s->cfg.arrived) {
      s->cfg.arrived(s->cfg.arrived_ctx, &e->data); /* contents only: the size stays */
    }
    s->bytes += e->data.stage >= 0 ? zb_chunk_terrain_bytes(&e->data) : 0;
    if (changed) {
      e->version++;
    }
    if (e->job_result == ZB_OK) {
      s->generated++;
    } else {
      e->retry_frame = s->frame + RETRY_FRAMES;
    }
  }
}

static bool submit(zb_stream *s, entry *e) {
  e->work = e->data;
  e->job_stage = e->target;
  e->job_result = ZB_OK;
  e->job = zb_job_submit(s->jobs, generate_job, e, ZB_JOB_NORMAL);
  if (!e->job) {
    return false;
  }
  if (!zb_array_push(&s->inflight, &e)) {
    /* Bookkeeping failed: take the job back (or wait for it) and retry later. */
    if (!zb_job_cancel(s->jobs, e->job)) {
      zb_job_wait(s->jobs, e->job);
      discard_work(e);
    }
    zb_job_release(s->jobs, e->job);
    e->job = NULL;
    return false;
  }
  return true;
}

/* ---- Rings ------------------------------------------------------------------ */

static zb_dvec3 dnorm(zb_dvec3 v) {
  double inv = 1.0 / sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  zb_dvec3 r = {v.x * inv, v.y * inv, v.z * inv};
  return r;
}

static double ddot(zb_dvec3 a, zb_dvec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static zb_dvec3 local_to_planet(zb_dvec3 east, zb_dvec3 north, zb_dvec3 up, zb_vec3 l) {
  zb_dvec3 r = {east.x * l.x + up.x * l.y - north.x * l.z, east.y * l.x + up.y * l.y - north.y * l.z,
                east.z * l.x + up.z * l.y - north.z * l.z};
  return r;
}

static focus_state focus_of(const zb_stream *s, const zb_stream_focus *f) {
  focus_state st;
  const zb_planet *p = &s->t->planet;
  zb_dvec3 c = zb_planet_to_cart(p, &f->pos);
  st.dir = dnorm(c);
  zb_dvec3 east, north, up;
  zb_planet_frame(st.dir, &east, &north, &up);
  zb_vec3 ahead = zb_v3_scale(f->velocity, s->cfg.lookahead);
  zb_dvec3 move = local_to_planet(east, north, up, ahead);
  zb_dvec3 pred = {c.x + move.x, c.y + move.y, c.z + move.z};
  st.predicted = dnorm(pred);
  st.view = local_to_planet(east, north, up, f->view);
  double alt = f->pos.alt > 0.0f ? (double)f->pos.alt : 0.0;
  st.far = (double)s->cfg.far_radius + (double)s->cfg.far_per_altitude * alt;
  if (st.far > (double)s->cfg.max_far_radius) {
    st.far = (double)s->cfg.max_far_radius;
  }
  return st;
}

static entry *create_entry(zb_stream *s, zb_chunk c, zb_dvec3 dir) {
  entry *e = zb_alloc(s->cfg.a, sizeof *e);
  if (!e) {
    return NULL;
  }
  memset(e, 0, sizeof *e);
  zb_chunk_terrain_init(&e->data, c);
  e->work = e->data;
  e->dir = dir;
  e->terrain = s->t;
  e->chunk_alloc = s->cfg.chunk_alloc;
  e->target = ZB_STAGE_NONE;
  if (zb_map_put(&s->map, zb_chunk_key(c), (uint64_t)(uintptr_t)e) != ZB_OK) {
    zb_free(s->cfg.a, e);
    return NULL;
  }
  return e;
}

typedef struct ring_limits {
  double near, far, near_keep, far_keep; /* cosines of the angular radii */
  double ecology, ecology_keep;          /* (above 1 when there is no ecology ring) */
} ring_limits;

/* Visits every chunk of one face inside the focus's keep cap. */
static void scan_face(zb_stream *s, const focus_state *f, int face, const ring_limits *lim, double scan_angle) {
  const zb_planet *p = &s->t->planet;
  zb_dvec3 n, U, V;
  zb_planet_face_axes(face, &n, &U, &V);
  double nf = ddot(n, f->dir);
  /* A face's corners are 54.74 degrees from its centre. */
  if (nf < zb_dcos(fmin(scan_angle + 0.9553166181245093, ZB_DPI))) {
    return;
  }
  /* Bounding box of the cap in face coordinates, from its boundary. */
  zb_dvec3 e1 =
      dnorm(fabs(f->dir.z) < 0.9 ? (zb_dvec3){-f->dir.y, f->dir.x, 0.0} : (zb_dvec3){0.0, -f->dir.z, f->dir.y});
  zb_dvec3 e2 = {f->dir.y * e1.z - f->dir.z * e1.y, f->dir.z * e1.x - f->dir.x * e1.z,
                 f->dir.x * e1.y - f->dir.y * e1.x};
  double cs = zb_dcos(scan_angle), sn = zb_dsin(scan_angle);
  double umin = 2.0, umax = -2.0, vmin = 2.0, vmax = -2.0;
  for (int i = 0; i <= CAP_SAMPLES; ++i) {
    zb_dvec3 q = f->dir;
    if (i < CAP_SAMPLES) {
      double phi = 2.0 * ZB_DPI * i / CAP_SAMPLES;
      double c = zb_dcos(phi) * sn, d = zb_dsin(phi) * sn;
      q.x = f->dir.x * cs + e1.x * c + e2.x * d;
      q.y = f->dir.y * cs + e1.y * c + e2.y * d;
      q.z = f->dir.z * cs + e1.z * c + e2.z * d;
    }
    if (ddot(q, n) <= 0.05) {
      umin = vmin = -1.0;
      umax = vmax = 1.0;
      break;
    }
    double u, v;
    zb_planet_dir_uv(face, q, &u, &v);
    umin = fmin(umin, u);
    umax = fmax(umax, u);
    vmin = fmin(vmin, v);
    vmax = fmax(vmax, v);
  }
  int32_t last = p->n - 1;
  int32_t x0 = (int32_t)floor((umin + 1.0) / p->chunk_uv) - 1, x1 = (int32_t)floor((umax + 1.0) / p->chunk_uv) + 1;
  int32_t y0 = (int32_t)floor((vmin + 1.0) / p->chunk_uv) - 1, y1 = (int32_t)floor((vmax + 1.0) / p->chunk_uv) + 1;
  x0 = x0 < 0 ? 0 : x0;
  y0 = y0 < 0 ? 0 : y0;
  x1 = x1 > last ? last : x1;
  y1 = y1 > last ? last : y1;
  if (x0 > x1 || y0 > y1) {
    return;
  }
  double uf = ddot(U, f->dir), vf = ddot(V, f->dir);
  if (x1 - x0 >= COLUMN_CACHE) {
    x1 = x0 + COLUMN_CACHE - 1; /* caps far beyond any configured radius */
  }
  for (int32_t x = x0; x <= x1; ++x) {
    s->column_tan[x - x0] = zb_dtan((-1.0 + ((double)x + 0.5) * p->chunk_uv) * (ZB_DPI / 4.0));
  }
  for (int32_t y = y0; y <= y1; ++y) {
    double b = zb_dtan((-1.0 + ((double)y + 0.5) * p->chunk_uv) * (ZB_DPI / 4.0));
    for (int32_t x = x0; x <= x1; ++x) {
      double a = s->column_tan[x - x0];
      double len = sqrt(1.0 + a * a + b * b);
      double dd = (nf + a * uf + b * vf) / len;
      if (dd < lim->far_keep) {
        continue;
      }
      int want = dd >= lim->ecology ? ZB_STAGE_ECOLOGY
                 : dd >= lim->near  ? ZB_STAGE_SURFACE
                 : dd >= lim->far   ? ZB_STAGE_COARSE
                                    : ZB_STAGE_NONE;
      int keep = dd >= lim->ecology_keep ? ZB_STAGE_ECOLOGY : dd >= lim->near_keep ? ZB_STAGE_SURFACE : ZB_STAGE_COARSE;
      zb_chunk c = {face, x, y};
      entry *e = lookup(s, c);
      if (!e) {
        if (want < 0) {
          continue;
        }
        zb_dvec3 dir = {(n.x + a * U.x + b * V.x) / len, (n.y + a * U.y + b * V.y) / len,
                        (n.z + a * U.z + b * V.z) / len};
        e = create_entry(s, c, dir);
        if (!e) {
          continue; /* out of memory: try again at the next refresh */
        }
      }
      if (e->epoch != s->epoch) {
        e->epoch = s->epoch;
        e->want = want;
        e->keep = keep;
      } else {
        e->want = want > e->want ? want : e->want;
        e->keep = keep > e->keep ? keep : e->keep;
      }
    }
  }
}

static void refresh(zb_stream *s, int count) {
  s->epoch++;
  double r = s->t->planet.radius;
  double h = (double)s->cfg.hysteresis;
  for (int i = 0; i < count; ++i) {
    const focus_state *f = &s->now[i];
    double near = (double)s->cfg.near_radius;
    double eco = (double)s->cfg.ecology_radius < near ? (double)s->cfg.ecology_radius : near;
    ring_limits lim = {zb_dcos((near + HALF_DIAGONAL) / r),
                       zb_dcos((f->far + HALF_DIAGONAL) / r),
                       zb_dcos((near * h + HALF_DIAGONAL) / r),
                       zb_dcos((f->far * h + HALF_DIAGONAL) / r),
                       eco > 0.0 ? zb_dcos((eco + HALF_DIAGONAL) / r) : 2.0,
                       eco > 0.0 ? zb_dcos((eco * h + HALF_DIAGONAL) / r) : 2.0};
    double scan = (f->far * h + HALF_DIAGONAL) / r;
    for (int face = 0; face < ZB_PLANET_FACES; ++face) {
      scan_face(s, f, face, &lim, scan);
    }
    s->last[i] = *f;
  }
  s->last_count = count;
}

/* The stage a chunk is (or will be after its job) at. */
static int current_stage(const entry *e) {
  return e->job && e->job_stage > e->data.stage ? e->job_stage : e->data.stage;
}

static void update_targets(zb_stream *s) {
  s->wanted = 0;
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    entry *e = (entry *)(uintptr_t)value;
    if (e->epoch != s->epoch) {
      e->want = e->keep = ZB_STAGE_NONE;
    }
    int cur = current_stage(e);
    int kept = cur < e->keep ? cur : e->keep;
    e->target = e->want > kept ? e->want : kept;
    if (e->target >= 0) {
      e->last_wanted = s->epoch;
      s->wanted++;
    }
  }
}

static bool needs_refresh(const zb_stream *s, int count) {
  if (count != s->last_count || s->epoch == 0) {
    return true;
  }
  double r = s->t->planet.radius;
  for (int i = 0; i < count; ++i) {
    zb_dvec3 a = s->now[i].dir, b = s->last[i].dir;
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    if (sqrt(dx * dx + dy * dy + dz * dz) * r > REFRESH_DISTANCE ||
        fabs(s->now[i].far - s->last[i].far) > 2.0 * REFRESH_DISTANCE) {
      return true;
    }
  }
  return false;
}

/* ---- Eviction and scheduling ------------------------------------------------- */

static int cmp_lru(const void *a, const void *b) {
  const candidate *x = a, *y = b;
  if (x->priority != y->priority) {
    return x->priority < y->priority ? -1 : 1;
  }
  uint64_t kx = zb_chunk_key(x->e->data.chunk), ky = zb_chunk_key(y->e->data.chunk);
  return kx < ky ? -1 : (kx > ky);
}

/* Drops stages no longer wanted, then evicts the least recently wanted chunks
 * outside the rings while over the memory cap (or holding too many). */
static void evict(zb_stream *s) {
  size_t cursor = 0;
  uint64_t key, value;
  size_t loaded = 0;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    entry *e = (entry *)(uintptr_t)value;
    loaded++;
    if (!e->job && e->target >= 0 && e->data.stage > e->target) {
      s->bytes -= zb_chunk_terrain_bytes(&e->data);
      zb_chunk_terrain_trim(e->chunk_alloc, &e->data, e->target);
      s->bytes += zb_chunk_terrain_bytes(&e->data);
      e->version++;
    }
  }
  size_t max_entries = 2 * s->wanted + 1024;
  if (s->bytes <= s->cfg.memory_cap && loaded <= max_entries) {
    return;
  }
  zb_array_clear(&s->candidates);
  cursor = 0;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    entry *e = (entry *)(uintptr_t)value;
    if (!e->job && e->target < 0) {
      candidate c = {e, (double)e->last_wanted};
      if (!zb_array_push(&s->candidates, &c)) {
        break;
      }
    }
  }
  if (s->candidates.len > 1) {
    qsort(s->candidates.data, s->candidates.len, sizeof(candidate), cmp_lru);
  }
  for (size_t i = 0; i < s->candidates.len; ++i) {
    if (s->bytes <= s->cfg.memory_cap && loaded <= max_entries) {
      break;
    }
    entry *e = ((candidate *)zb_array_at(&s->candidates, i))->e;
    zb_map_remove(&s->map, zb_chunk_key(e->data.chunk));
    free_entry(s, e);
    loaded--;
    s->evicted++;
  }
}

/* Lower is sooner: distance to the nearest predicted focus, x1.5 behind the view. */
static double priority_of(const zb_stream *s, const entry *e, int count) {
  double best = 1e300;
  for (int i = 0; i < count; ++i) {
    const focus_state *f = &s->now[i];
    double dx = e->dir.x - f->predicted.x, dy = e->dir.y - f->predicted.y, dz = e->dir.z - f->predicted.z;
    double d = sqrt(dx * dx + dy * dy + dz * dz);
    zb_dvec3 rel = {e->dir.x - f->dir.x, e->dir.y - f->dir.y, e->dir.z - f->dir.z};
    if (ddot(rel, f->view) < 0.0) {
      d *= 1.5;
    }
    best = d < best ? d : best;
  }
  return best;
}

static void schedule(zb_stream *s, int count) {
  int free_slots = s->cfg.max_jobs - (int)s->inflight.len;
  if (free_slots <= 0) {
    return;
  }
  zb_array_clear(&s->candidates);
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    entry *e = (entry *)(uintptr_t)value;
    if (!e->job && e->target > e->data.stage && s->frame >= e->retry_frame) {
      candidate c = {e, priority_of(s, e, count)};
      if (!zb_array_push(&s->candidates, &c)) {
        break;
      }
    }
  }
  if (s->candidates.len > 1) {
    qsort(s->candidates.data, s->candidates.len, sizeof(candidate), cmp_lru);
  }
  for (size_t i = 0; i < s->candidates.len && free_slots > 0; ++i) {
    if (submit(s, ((candidate *)zb_array_at(&s->candidates, i))->e)) {
      free_slots--;
    }
  }
}

/* Cancels queued jobs whose chunk left the rings. */
static void cancel_unwanted(zb_stream *s) {
  for (size_t i = 0; i < s->inflight.len; ++i) {
    entry *e = *(entry **)zb_array_at(&s->inflight, i);
    if (e->target < 0 && zb_job_cancel(s->jobs, e->job)) {
      s->cancelled++;
    }
  }
}

void zb_stream_update(zb_stream *s, const zb_stream_focus *focus, int count) {
  s->frame++;
  count = count < 0 ? 0 : (count > ZB_STREAM_MAX_FOCUS ? ZB_STREAM_MAX_FOCUS : count);
  for (int i = 0; i < count; ++i) {
    s->now[i] = focus_of(s, &focus[i]);
  }
  collect(s);
  if (needs_refresh(s, count)) {
    refresh(s, count);
    update_targets(s);
    cancel_unwanted(s);
    collect(s);
  }
  evict(s);
  schedule(s, count);
  if (zb_jobs_is_inline(s->jobs)) {
    uint64_t start = s->cfg.clock_ns ? s->cfg.clock_ns(s->cfg.clock_ctx) : 0;
    uint64_t budget = (uint64_t)((double)s->cfg.inline_budget_ms * 1e6);
    while (zb_jobs_run_pending(s->jobs, 1) > 0) {
      collect(s);
      schedule(s, count);
      if (!s->cfg.clock_ns || s->cfg.clock_ns(s->cfg.clock_ctx) - start >= budget) {
        break;
      }
    }
  }
  /* Finished jobs may have added memory: stay within the cap at the end of every update. */
  collect(s);
  evict(s);
}

/* ---- Queries ------------------------------------------------------------------- */

const zb_chunk_terrain *zb_stream_find(const zb_stream *s, zb_chunk c) {
  const entry *e = lookup(s, c);
  return e && e->data.stage >= 0 ? &e->data : NULL;
}

zb_chunk_terrain *zb_stream_edit(zb_stream *s, zb_chunk c) {
  entry *e = lookup(s, c);
  /* While a job works on the chunk it shares its arrays: the edit waits for
   * the job's data to arrive (the arrived hook). */
  if (!e || e->data.stage < 0 || e->job) {
    return NULL;
  }
  e->version++;
  return &e->data;
}

int zb_stream_target(const zb_stream *s, zb_chunk c) {
  const entry *e = lookup(s, c);
  return e ? e->target : ZB_STAGE_NONE;
}

uint32_t zb_stream_version(const zb_stream *s, zb_chunk c) {
  const entry *e = lookup(s, c);
  return e ? e->version : 0;
}

void zb_stream_get_stats(const zb_stream *s, zb_stream_stats *out) {
  memset(out, 0, sizeof *out);
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    const entry *e = (const entry *)(uintptr_t)value;
    if (e->data.stage >= 0) {
      out->chunks[e->data.stage]++;
      out->cached += e->target < 0;
    }
    out->queued += !e->job && e->target > e->data.stage;
  }
  out->wanted = s->wanted;
  out->bytes = s->bytes;
  out->jobs = (int)s->inflight.len;
  out->generated = s->generated;
  out->evicted = s->evicted;
  out->cancelled = s->cancelled;
  out->refreshes = s->epoch;
}

void zb_stream_each(const zb_stream *s, void (*fn)(void *ctx, const zb_chunk_terrain *ct, uint32_t version, int target),
                    void *ctx) {
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&s->map, &cursor, &key, &value)) {
    const entry *e = (const entry *)(uintptr_t)value;
    if (e->data.stage >= 0) {
      fn(ctx, &e->data, e->version, e->target);
    }
  }
}
