/* SPDX-License-Identifier: MIT */
/* Creatures drawn from low-polygon parts in their species' colour: walkers
 * as a rounded body with a head and four legs that swing as they go, fliers
 * as a body with a head and flapping wings. The dead lie on their side. */
#ifndef ZB_APP_CREATURE_VIEW_H
#define ZB_APP_CREATURE_VIEW_H

#include "app/body_view.h"
#include "sim/creatures.h"

typedef struct zb_creature_view zb_creature_view;

zb_creature_view *zb_creature_view_create(const zb_allocator *a, zb_render *r);
void zb_creature_view_destroy(zb_creature_view *v);
/* Draws the creatures within range metres of the eye; returns how many. */
int zb_creature_view_draw(zb_creature_view *v, const zb_creatures *c, const zb_world *world, zb_vec3 eye, float range,
                          double time);

#endif
