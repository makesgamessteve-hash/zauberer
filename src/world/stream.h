/* SPDX-License-Identifier: MIT */
/* Chunk streaming: keeps terrain chunks generated around up to four focus
 * points (the players' cameras) in rings (stage 2 near, stage 0 far), ordered
 * by distance to the predicted position and the view direction, within a
 * worker-job limit, a main-thread time budget (inline mode) and a memory cap
 * (least recently wanted chunks are evicted first). Generation runs on jobs;
 * every peer streams only around its own focus points. */
#ifndef ZB_WORLD_STREAM_H
#define ZB_WORLD_STREAM_H

#include "core/jobs.h"
#include "core/math.h"
#include "world/terrain.h"

#define ZB_STREAM_MAX_FOCUS 4

typedef enum zb_stream_profile { ZB_STREAM_DESKTOP, ZB_STREAM_WEB, ZB_STREAM_MOBILE } zb_stream_profile;

typedef struct zb_stream_config {
  const zb_allocator *a;           /* bookkeeping (main thread only) */
  const zb_allocator *chunk_alloc; /* chunk arrays, also used by workers: must be thread-safe */
  zb_jobs *jobs;                   /* NULL: an inline job queue of the stream's own */
  uint64_t (*clock_ns)(void *ctx); /* inline time budget; NULL: one job per update */
  void *clock_ctx;
  float ecology_radius;   /* metres: chunks brought to stage 3 (plants); 0 (the default): none */
  float near_radius;      /* metres: chunks brought to stage 2 */
  float far_radius;       /* metres at ground level: chunks brought to stage 0 */
  float far_per_altitude; /* extra far radius per metre of altitude */
  float max_far_radius;
  float hysteresis;       /* a chunk keeps its stage until this factor beyond its ring */
  float lookahead;        /* seconds of velocity used to predict the focus */
  size_t memory_cap;      /* bytes of chunk data */
  int max_jobs;           /* generation jobs in flight */
  float inline_budget_ms; /* main-thread generation time per update (inline jobs) */
} zb_stream_config;

/* A focus point; velocity and view are in the local frame at pos
 * (x east, y up, z south), as from zb_origin. */
typedef struct zb_stream_focus {
  zb_planet_pos pos;
  zb_vec3 velocity;
  zb_vec3 view;
} zb_stream_focus;

typedef struct zb_stream_stats {
  size_t chunks[4]; /* loaded chunks at stage 0, 1, 2 and 3 */
  size_t wanted;    /* chunks inside the rings */
  size_t cached;    /* loaded chunks outside the rings (kept until evicted) */
  size_t bytes;     /* chunk data */
  int jobs;         /* generation jobs in flight */
  size_t queued;    /* chunks waiting for a job */
  uint64_t generated, evicted, cancelled, refreshes;
} zb_stream_stats;

typedef struct zb_stream zb_stream;

void zb_stream_default_config(zb_stream_config *c, zb_stream_profile profile);
/* The terrain must outlive the stream. Returns NULL when out of memory. */
/* The ecology ring of a profile, for worlds with plants (metres). */
float zb_stream_profile_ecology_radius(zb_stream_profile profile);
zb_stream *zb_stream_create(const zb_terrain *t, const zb_stream_config *cfg);
/* Cancels pending jobs and waits for running ones. */
void zb_stream_destroy(zb_stream *s);
/* Changes the ring radii (metres); the far ring may grow with altitude up to
 * twice far. Takes effect at the next update. */
void zb_stream_set_radii(zb_stream *s, float near_radius, float far_radius);
void zb_stream_get_radii(const zb_stream *s, float *near_radius, float *far_radius);
/* One streaming step (call once per frame): collects finished jobs, updates
 * the rings when a focus moved, evicts, and starts new jobs. */
void zb_stream_update(zb_stream *s, const zb_stream_focus *focus, int count);
/* A loaded chunk (never one half-written by a job), or NULL. */
const zb_chunk_terrain *zb_stream_find(const zb_stream *s, zb_chunk c);
/* Stage the rings currently ask for (ZB_STAGE_NONE outside them). */
int zb_stream_target(const zb_stream *s, zb_chunk c);
/* Changes whenever a chunk's data changes (for mesh caches). */
uint32_t zb_stream_version(const zb_stream *s, zb_chunk c);
void zb_stream_get_stats(const zb_stream *s, zb_stream_stats *out);
/* Calls fn for every loaded chunk with its data version and the stage the
 * rings want (ZB_STAGE_NONE for cached chunks outside them). */
void zb_stream_each(const zb_stream *s, void (*fn)(void *ctx, const zb_chunk_terrain *ct, uint32_t version, int target),
                    void *ctx);

#endif
