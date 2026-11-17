/* SPDX-License-Identifier: MIT */
/* Entry point for SDL builds: SDL3 main callbacks on every platform. */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "platform/host.h"

static SDL_AppResult to_sdl(zb_host_status s) {
  switch (s) {
    case ZB_HOST_SUCCESS: return SDL_APP_SUCCESS;
    case ZB_HOST_FAILURE: return SDL_APP_FAILURE;
    case ZB_HOST_CONTINUE: break;
  }
  return SDL_APP_CONTINUE;
}

static zb_host_status from_sdl(SDL_AppResult r) {
  return r == SDL_APP_SUCCESS ? ZB_HOST_SUCCESS : (r == SDL_APP_FAILURE ? ZB_HOST_FAILURE : ZB_HOST_CONTINUE);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  return to_sdl(zb_host_init(appstate, argc, argv));
}

SDL_AppResult SDL_AppIterate(void *appstate) { return to_sdl(zb_host_iterate(appstate)); }

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) { return to_sdl(zb_host_event(appstate, event)); }

void SDL_AppQuit(void *appstate, SDL_AppResult result) { zb_host_quit(appstate, from_sdl(result)); }
