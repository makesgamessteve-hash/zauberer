/* SPDX-License-Identifier: MIT */
/* Entry point for headless-only builds (web-node): a plain loop. */
#include "platform/host.h"

int main(int argc, char **argv) {
  void *state = 0;
  zb_host_status s = zb_host_init(&state, argc, argv);
  while (s == ZB_HOST_CONTINUE) {
    s = zb_host_iterate(state);
  }
  zb_host_quit(state, s);
  return s == ZB_HOST_SUCCESS ? 0 : 1;
}
