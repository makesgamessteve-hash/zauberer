/* SPDX-License-Identifier: MIT */
/* zauberer-relay: forwards co-op sessions reached by invite code (net/relay.h). */
#include <signal.h>
#include <stdio.h>

#include "net/relay.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
  (void)sig;
  g_stop = 1;
}

int main(int argc, char **argv) {
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  return zb_relay_main(argc, argv, stdout, &g_stop);
}
