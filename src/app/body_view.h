/* SPDX-License-Identifier: MIT */
/* Rigid bodies drawn as low-polygon meshes in their material's colour: a
 * faceted ball for spheres, a block for boxes, and a ten-sided log with
 * rounded ends for capsules. Meshes are built once per shape and material. */
#ifndef ZB_APP_BODY_VIEW_H
#define ZB_APP_BODY_VIEW_H

#include "render/render.h"
#include "sim/bodies.h"

typedef enum zb_body_part { ZB_PART_BALL, ZB_PART_BLOCK, ZB_PART_TUBE, ZB_PARTS } zb_body_part;

typedef struct zb_body_view zb_body_view;

zb_body_view *zb_body_view_create(const zb_allocator *a, zb_render *r, const zb_materials *materials);
void zb_body_view_destroy(zb_body_view *v);
/* Draws the bodies within range metres of the eye (render coordinates);
 * returns how many. */
int zb_body_view_draw(zb_body_view *v, const zb_bodies *b, const zb_world *world, zb_vec3 eye, float range);
/* The mesh of a part (exposed for tests): a unit ball, a block of half size 1
 * or a tube of radius 1 from z = -1 to 1, flat-shaded in one colour. */
zb_result zb_body_part_mesh(const zb_allocator *a, zb_body_part part, const float color[3], zb_mesh_data *out);

#endif
