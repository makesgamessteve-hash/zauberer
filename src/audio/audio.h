/* SPDX-License-Identifier: MIT */
/* Audio: WAV and OGG Vorbis playback from packages through a software mixer
 * (float stereo), volume groups, a peak limiter, streamed music, a silent
 * tick-driven mode without a device, and the browser audio-unlock gate. */
#ifndef ZB_AUDIO_AUDIO_H
#define ZB_AUDIO_AUDIO_H

#include "core/alloc.h"
#include "core/thread.h"
#include "vfs/vfs.h"

#define ZB_AUDIO_VOICES 64
#define ZB_AUDIO_RATE 48000
#define ZB_AUDIO_STREAM_BUFFER (512u * 1024u)

typedef enum zb_audio_group {
  ZB_AUDIO_EFFECTS,
  ZB_AUDIO_MUSIC,
  ZB_AUDIO_GROUP_COUNT
} zb_audio_group;

typedef enum zb_voice_state {
  ZB_VOICE_PLAYING, /* includes loading */
  ZB_VOICE_STOPPED
} zb_voice_state;

typedef struct zb_play_desc {
  float volume;  /* 0..1+ */
  float pitch;   /* playback rate factor, > 0 */
  bool loop;
  zb_audio_group group;
} zb_play_desc;

typedef struct zb_audio zb_audio;

zb_audio *zb_audio_create(const zb_allocator *a, zb_vfs *vfs, const zb_thread_api *threads);
void zb_audio_destroy(zb_audio *au);

/* Without a device the mixer advances voices by simulation time (zb_audio_tick). */
void zb_audio_set_device(zb_audio *au, bool has_device);
bool zb_audio_has_device(const zb_audio *au);
/* Browser autoplay gate: while locked, one-shots are dropped and loops/music
 * wait; unlocking starts them. */
void zb_audio_set_locked(zb_audio *au, bool locked);
bool zb_audio_is_locked(const zb_audio *au);

zb_play_desc zb_play_defaults(void);
/* Returns a voice handle (never 0), or 0 when all voices are busy. */
uint32_t zb_audio_play(zb_audio *au, const char *path, const zb_play_desc *desc);
void zb_audio_stop(zb_audio *au, uint32_t voice);
void zb_audio_set_voice_volume(zb_audio *au, uint32_t voice, float volume);
zb_voice_state zb_audio_voice_state(const zb_audio *au, uint32_t voice);
/* Decoder memory held by a streaming voice (0 for decoded sounds). */
size_t zb_audio_voice_decoder_bytes(const zb_audio *au, uint32_t voice);

void zb_audio_set_master_volume(zb_audio *au, float volume);
void zb_audio_set_group_volume(zb_audio *au, zb_audio_group group, float volume);
float zb_audio_group_volume(const zb_audio *au, zb_audio_group group);
size_t zb_audio_active_voices(const zb_audio *au);

/* Mixes frames of interleaved stereo float output (device callback). */
void zb_audio_mix(zb_audio *au, float *out, int frames);
/* Advances one simulation tick of dt seconds (no-device mode). */
void zb_audio_tick(zb_audio *au, double dt);

/* Decodes a WAV (PCM 8/16/24/32-bit or float32, mono/stereo) into float
 * stereo. Exposed for tests. */
zb_result zb_audio_decode_wav(const zb_allocator *a, const void *data, size_t len, float **samples,
                              size_t *frames, int *rate);

#endif
