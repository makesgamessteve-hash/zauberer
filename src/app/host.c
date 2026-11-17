/* SPDX-License-Identifier: MIT */
/* Implements the platform's host lifecycle with the engine. */
#include "app/engine.h"
#include "platform/host.h"

zb_host_status zb_host_init(void **state, int argc, char **argv) {
  zb_engine *e = NULL;
  zb_host_status s = zb_engine_create(argc, argv, &e);
  *state = e;
  return s;
}

zb_host_status zb_host_iterate(void *state) { return zb_engine_iterate(state); }

zb_host_status zb_host_event(void *state, const void *native) {
  zb_engine *e = state;
  zb_event ev;
  if (!e || !zb_platform_translate_event(e->platform, native, &ev)) {
    return ZB_HOST_CONTINUE;
  }
  return zb_engine_event(e, &ev);
}

void zb_host_quit(void *state, zb_host_status status) {
  (void)status;
  zb_engine_destroy(state);
}
