/* SPDX-License-Identifier: MIT */
/* Deltas: everything the players changed in the generated world, per chunk, so
 * that a chunk generated again comes back as they left it. Saves store only
 * these (in region files of 16 x 16 chunks), and co-op sends the same edits.
 * So far: removed plants and painted surface materials. */
#ifndef ZB_WORLD_DELTA_H
#define ZB_WORLD_DELTA_H

#include "core/array.h"
#include "world/terrain.h"

#define ZB_REGION_CHUNKS 16 /* chunks per region edge */
#define ZB_DELTA_VERSION 1  /* region file format */

typedef enum zb_edit_kind {
  ZB_EDIT_PLANT_REMOVED = 1, /* index: the plant (stage 3) */
  ZB_EDIT_MATERIAL = 2,      /* x, y: the 1 m cell; value: the zb_material (stage 2) */
  ZB_EDIT_BURNT = 3,         /* x, y: the 2 m cell (0..127); index: when, in game minutes */
  ZB_EDIT_HEIGHT = 4         /* x, y: the 1 m height sample (0..256); index: its height in metres (float bits) */
} zb_edit_kind;

#define ZB_BURN_RES 128 /* burnt cells per chunk edge */
/* After a fire the ground is ash for a while, and plants grow back by layer
 * (zb_plant_layer): ground cover first, trees last (game seconds). */
#define ZB_ASH_SECONDS (20.0 * 86400.0)
double zb_regrowth_seconds(int layer);

typedef struct zb_edit {
  uint8_t kind;
  uint8_t value;
  uint16_t x, y;
  uint32_t index;
} zb_edit;

typedef struct zb_deltas zb_deltas;

zb_deltas *zb_deltas_create(const zb_allocator *a);
void zb_deltas_destroy(zb_deltas *d);
/* Records an edit of a chunk (an edit of the same plant or cell replaces the
 * earlier one) and marks its region for saving. */
zb_result zb_deltas_add(zb_deltas *d, zb_chunk c, const zb_edit *e);
/* The edits of a chunk (NULL and 0 when it has none). */
const zb_edit *zb_deltas_get(const zb_deltas *d, zb_chunk c, size_t *count);
size_t zb_deltas_chunks(const zb_deltas *d);
/* Calls fn for every chunk with edits. */
void zb_deltas_each_chunk(const zb_deltas *d, void (*fn)(void *ctx, zb_chunk c), void *ctx);
/* Whether a burn in chunk c reaches a turning point in (from, to]: its ash
 * goes, or a layer of its plants has grown back. */
bool zb_deltas_burn_due(const zb_deltas *d, zb_chunk c, double from, double to);
/* Applies a chunk's edits to whatever stages it has at the game time now
 * (burnt ground heals); true if anything changed. */
bool zb_deltas_apply(const zb_deltas *d, zb_chunk_terrain *ct, double now);

/* Regions with edits not yet saved, and all regions with edits. */
typedef void (*zb_region_fn)(void *ctx, int face, int32_t rx, int32_t ry);
void zb_deltas_each_dirty(const zb_deltas *d, zb_region_fn fn, void *ctx);
zb_result zb_deltas_each_region(const zb_deltas *d, zb_region_fn fn, void *ctx);
void zb_deltas_clear_dirty(zb_deltas *d);
/* A region's file name ("<face>.<rx>.<ry>.bin") and file image (appended to
 * out): a versioned header, a checksum and the zlib-compressed edits. */
void zb_region_file_name(int face, int32_t rx, int32_t ry, char *out, size_t len);
zb_result zb_deltas_write_region(const zb_deltas *d, int face, int32_t rx, int32_t ry, zb_array *out);
/* Adds the edits of a region file; a message in err for a damaged or newer file. */
zb_result zb_deltas_read_region(zb_deltas *d, const void *data, size_t len, char *err, size_t err_len);

#endif
