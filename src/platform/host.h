/* SPDX-License-Identifier: MIT */
/* The application lifecycle the platform entry point drives. The functions
 * are implemented by the composition root (src/app), which keeps the
 * platform module free of any dependency on the application. */
#ifndef ZB_PLATFORM_HOST_H
#define ZB_PLATFORM_HOST_H

typedef enum zb_host_status {
  ZB_HOST_CONTINUE,
  ZB_HOST_SUCCESS,
  ZB_HOST_FAILURE
} zb_host_status;

zb_host_status zb_host_init(void **state, int argc, char **argv);
zb_host_status zb_host_iterate(void *state);
/* native is an SDL_Event* in SDL builds. */
zb_host_status zb_host_event(void *state, const void *native);
void zb_host_quit(void *state, zb_host_status status);

#endif
