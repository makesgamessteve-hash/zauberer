/* SPDX-License-Identifier: MIT */
/* Standing water from the surface-water simulation, drawn with the water
 * shader: a mesh per chunk of the cells deeper than 2 cm, rebuilt at most
 * twice a second while the water moves. */
#ifndef ZB_APP_WATER_VIEW_H
#define ZB_APP_WATER_VIEW_H

#include "render/render.h"
#include "sim/water.h"

typedef struct zb_water_view zb_water_view;

zb_water_view *zb_water_view_create(const zb_allocator *a, zb_render *r);
void zb_water_view_destroy(zb_water_view *v);
/* Draws the standing water of every simulated chunk (now: seconds, for
 * pacing the rebuilds); returns the meshes drawn. */
int zb_water_view_draw(zb_water_view *v, const zb_surface_water *w, const zb_world *world, double now);
/* Builds the mesh of one chunk's standing water (exposed for tests): quads at
 * the water's surface in the chunk's own frame; empty when it is dry. */
zb_result zb_standing_water_mesh(const zb_allocator *a, const zb_planet *p, const zb_water_chunk_view *c,
                                 zb_mesh_data *out);

#endif
