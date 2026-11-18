/* SPDX-License-Identifier: MIT */
/* Draws the streamed terrain: one mesh per loaded chunk at a level of detail
 * chosen by distance (4, 8, 16, 32 or 64 m grids), in the chunk's own tangent
 * frame so that the planet's curvature is exact, with skirts against cracks
 * between levels and a water surface at sea level. Meshes are rebuilt within
 * a per-frame budget when a chunk's data or level changes. */
#ifndef ZB_APP_TERRAIN_VIEW_H
#define ZB_APP_TERRAIN_VIEW_H

#include "render/render.h"
#include "world/world.h"

#define ZB_TERRAIN_LODS 5

typedef struct zb_terrain_view_config {
  /* Grid steps 4, 8, 16, 32 m are used below these distances, 64 m beyond. */
  float lod_distance[ZB_TERRAIN_LODS - 1];
  int fine_builds;   /* 4 m and 8 m meshes built per frame */
  int coarse_builds; /* coarser meshes built per frame */
} zb_terrain_view_config;

typedef struct zb_terrain_view_stats {
  size_t meshes;    /* meshes held */
  size_t built;     /* meshes built this frame */
  size_t drawn;     /* chunks submitted this frame (before culling) */
  size_t triangles; /* triangles submitted this frame */
  size_t per_lod[ZB_TERRAIN_LODS];
} zb_terrain_view_stats;

typedef struct zb_terrain_view zb_terrain_view;

void zb_terrain_view_default_config(zb_terrain_view_config *c);
/* Reads the material atlas description (e.g. "textures/terrain/atlas.lua")
 * through vfs for layer colours and the texture; vfs or atlas may be NULL
 * (untextured default colours). cfg may be NULL for the defaults. */
zb_terrain_view *zb_terrain_view_create(const zb_allocator *a, zb_render *r, zb_vfs *vfs, const char *atlas,
                                        const zb_terrain_view_config *cfg);
void zb_terrain_view_destroy(zb_terrain_view *tv);
/* Builds what the budget allows and draws every chunk inside the rings (between
 * zb_render_begin and zb_render_end). eye is the camera in render coordinates. */
void zb_terrain_view_draw(zb_terrain_view *tv, zb_world *w, zb_vec3 eye);
void zb_terrain_view_get_stats(const zb_terrain_view *tv, zb_terrain_view_stats *out);
/* The level a chunk at this distance gets (0 = 4 m grid). */
int zb_terrain_view_lod(const zb_terrain_view *tv, float distance);
/* Builds a chunk mesh (exposed for tests): grid step 4 << lod metres. */
zb_result zb_terrain_mesh_build(const zb_allocator *a, const zb_planet *p, const zb_chunk_terrain *ct, int lod,
                                const zb_vec3 colors[ZB_MAT_COUNT], const int layers[ZB_MAT_COUNT], zb_mesh_data *out);

#endif
