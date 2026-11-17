/* SPDX-License-Identifier: MIT */
#include "audio/audio.h"

#include "core/log.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

typedef struct sound {
  char path[ZB_VFS_MAX_PATH];
  float *samples; /* decoded WAV, stereo */
  size_t frames;
  int rate;
  char *compressed; /* OGG bytes (streamed) */
  size_t compressed_len;
  bool loading;
  bool failed;
  int refs;
  zb_vfs_req *req;
  struct zb_audio *owner;
} sound;

typedef struct voice {
  bool active;
  uint16_t generation;
  sound *snd;
  bool deferred;  /* waiting for unlock */
  double pos;     /* in source frames */
  float volume;
  float pitch;
  bool loop;
  zb_audio_group group;
  /* streaming */
  stb_vorbis *vorbis;
  unsigned char *vorbis_mem;
  float stream_buf[2048 * 2];
  int stream_len;   /* frames in stream_buf */
  int stream_pos;
  double stream_frac;
  bool stream_end;
} voice;

struct zb_audio {
  const zb_allocator *a;
  zb_vfs *vfs;
  const zb_thread_api *threads;
  void *mutex;
  voice voices[ZB_AUDIO_VOICES];
  sound *sounds[ZB_AUDIO_VOICES * 2];
  size_t sound_count;
  float group_volume[ZB_AUDIO_GROUP_COUNT];
  float master;
  float limiter_gain;
  bool has_device;
  bool locked;
  float scratch[1024 * 2];
};

static void lock(zb_audio *au) {
  if (au->mutex) {
    au->threads->mutex_lock(au->mutex);
  }
}

static void unlock(zb_audio *au) {
  if (au->mutex) {
    au->threads->mutex_unlock(au->mutex);
  }
}

/* ---- WAV ------------------------------------------------------------------- */

static uint32_t rd32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static float sample_at(const unsigned char *p, int bits, bool is_float) {
  if (is_float) {
    float f;
    uint32_t v = rd32(p);
    memcpy(&f, &v, 4);
    return f;
  }
  switch (bits) {
    case 8: return ((float)p[0] - 128.0f) / 128.0f;
    case 16: return (float)(int16_t)rd16(p) / 32768.0f;
    case 24: {
      int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
      return (float)v / 8388608.0f;
    }
    default: return (float)(int32_t)rd32(p) / 2147483648.0f;
  }
}

zb_result zb_audio_decode_wav(const zb_allocator *a, const void *data, size_t len, float **samples,
                              size_t *frames, int *rate) {
  const unsigned char *d = data;
  if (len < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
    return ZB_ERR_PARSE;
  }
  int channels = 0, bits = 0, format = 0;
  uint32_t srate = 0;
  const unsigned char *pcm = NULL;
  size_t pcm_len = 0;
  size_t pos = 12;
  while (pos + 8 <= len) {
    uint32_t size = rd32(d + pos + 4);
    const unsigned char *body = d + pos + 8;
    if (size > len - pos - 8) {
      return ZB_ERR_PARSE;
    }
    if (memcmp(d + pos, "fmt ", 4) == 0 && size >= 16) {
      format = rd16(body);
      channels = rd16(body + 2);
      srate = rd32(body + 4);
      bits = rd16(body + 14);
      if (format == 0xFFFE && size >= 26) {
        format = rd16(body + 24); /* WAVE_FORMAT_EXTENSIBLE sub-format */
      }
    } else if (memcmp(d + pos, "data", 4) == 0) {
      pcm = body;
      pcm_len = size;
    }
    pos += 8 + size + (size & 1u);
  }
  bool is_float = format == 3 && bits == 32;
  if (!pcm || (format != 1 && !is_float) || (channels != 1 && channels != 2) ||
      (bits != 8 && bits != 16 && bits != 24 && bits != 32) || srate == 0) {
    return ZB_ERR_UNSUPPORTED;
  }
  size_t bytes = (size_t)(bits / 8);
  size_t n = pcm_len / (bytes * (size_t)channels);
  float *out = zb_alloc(a, (n ? n : 1) * 2 * sizeof(float));
  if (!out) {
    return ZB_ERR_NOMEM;
  }
  for (size_t i = 0; i < n; ++i) {
    const unsigned char *f = pcm + i * bytes * (size_t)channels;
    float l = sample_at(f, bits, is_float);
    float r = channels == 2 ? sample_at(f + bytes, bits, is_float) : l;
    out[i * 2] = l;
    out[i * 2 + 1] = r;
  }
  *samples = out;
  *frames = n;
  *rate = (int)srate;
  return ZB_OK;
}

/* ---- Setup ----------------------------------------------------------------- */

zb_audio *zb_audio_create(const zb_allocator *a, zb_vfs *vfs, const zb_thread_api *threads) {
  zb_audio *au = zb_calloc(a, sizeof(*au));
  if (!au) {
    return NULL;
  }
  au->a = a;
  au->vfs = vfs;
  au->threads = threads;
  if (threads) {
    au->mutex = threads->mutex_create();
  }
  au->group_volume[ZB_AUDIO_EFFECTS] = 1.0f;
  au->group_volume[ZB_AUDIO_MUSIC] = 1.0f;
  au->master = 1.0f;
  au->limiter_gain = 1.0f;
  return au;
}

static void voice_release(zb_audio *au, voice *v) {
  if (v->vorbis) {
    stb_vorbis_close(v->vorbis);
    v->vorbis = NULL;
  }
  zb_free(au->a, v->vorbis_mem);
  v->vorbis_mem = NULL;
  if (v->snd) {
    v->snd->refs--;
  }
  v->snd = NULL;
  v->active = false;
}

void zb_audio_destroy(zb_audio *au) {
  if (!au) {
    return;
  }
  for (int i = 0; i < ZB_AUDIO_VOICES; ++i) {
    if (au->voices[i].active) {
      voice_release(au, &au->voices[i]);
    }
  }
  for (size_t i = 0; i < au->sound_count; ++i) {
    sound *s = au->sounds[i];
    if (s->req) {
      zb_vfs_req_release(au->vfs, s->req);
    }
    zb_free(au->a, s->samples);
    zb_free(au->a, s->compressed);
    zb_free(au->a, s);
  }
  if (au->mutex) {
    au->threads->mutex_destroy(au->mutex);
  }
  zb_free(au->a, au);
}

void zb_audio_set_device(zb_audio *au, bool has_device) { au->has_device = has_device; }
bool zb_audio_has_device(const zb_audio *au) { return au->has_device; }

void zb_audio_set_locked(zb_audio *au, bool locked) {
  lock(au);
  au->locked = locked;
  if (!locked) {
    for (int i = 0; i < ZB_AUDIO_VOICES; ++i) {
      au->voices[i].deferred = false;
    }
  }
  unlock(au);
}

bool zb_audio_is_locked(const zb_audio *au) { return au->locked; }

zb_play_desc zb_play_defaults(void) {
  zb_play_desc d = {1.0f, 1.0f, false, ZB_AUDIO_EFFECTS};
  return d;
}

/* ---- Loading ----------------------------------------------------------------- */

static bool ends_with(const char *s, const char *suffix) {
  size_t n = strlen(s), m = strlen(suffix);
  return n >= m && strcmp(s + n - m, suffix) == 0;
}

static sound *find_sound(zb_audio *au, const char *path) {
  for (size_t i = 0; i < au->sound_count; ++i) {
    if (strcmp(au->sounds[i]->path, path) == 0) {
      return au->sounds[i];
    }
  }
  return NULL;
}

static void decode_sound(zb_audio *au, sound *s, const char *data, size_t len) {
  if (ends_with(s->path, ".ogg")) {
    s->compressed = zb_alloc(au->a, len ? len : 1);
    if (!s->compressed) {
      s->failed = true;
      return;
    }
    memcpy(s->compressed, data, len);
    s->compressed_len = len;
    int error = 0;
    stb_vorbis *probe = stb_vorbis_open_memory((const unsigned char *)data, (int)len, &error, NULL);
    if (!probe) {
      zb_error("sound '%s' is not a valid OGG Vorbis file", s->path);
      s->failed = true;
      return;
    }
    s->rate = (int)stb_vorbis_get_info(probe).sample_rate;
    s->frames = stb_vorbis_stream_length_in_samples(probe);
    stb_vorbis_close(probe);
  } else {
    zb_result r = zb_audio_decode_wav(au->a, data, len, &s->samples, &s->frames, &s->rate);
    if (r != ZB_OK) {
      zb_error("sound '%s' could not be decoded (%s)", s->path, zb_result_str(r));
      s->failed = true;
    }
  }
}

static void on_loaded(zb_vfs_req *req, void *user) {
  sound *s = user;
  zb_audio *au = s->owner;
  size_t len = 0;
  const char *data = zb_vfs_req_data(req, &len);
  lock(au);
  s->req = NULL;
  if (data) {
    decode_sound(au, s, data, len);
  } else {
    char key[ZB_VFS_MAX_PATH + 16];
    snprintf(key, sizeof key, "missing:%s", s->path);
    zb_log_once(ZB_LOG_WARN, key, "sound '%s' could not be loaded (%s); playing silence", s->path,
                zb_result_str(zb_vfs_req_error(req)));
    s->failed = true;
  }
  s->loading = false;
  unlock(au);
  zb_vfs_req_release(au->vfs, req);
}

static sound *get_sound(zb_audio *au, const char *path) {
  sound *s = find_sound(au, path);
  if (s) {
    return s;
  }
  if (au->sound_count >= ZB_ARRAY_LEN(au->sounds)) {
    return NULL;
  }
  s = zb_calloc(au->a, sizeof(*s));
  if (!s) {
    return NULL;
  }
  snprintf(s->path, sizeof s->path, "%s", path);
  s->owner = au;
  s->loading = true;
  au->sounds[au->sound_count++] = s;
  s->req = zb_vfs_request(au->vfs, path, on_loaded, s);
  if (!s->req) {
    s->loading = false;
    s->failed = true;
  }
  return s;
}

/* ---- Voices ------------------------------------------------------------------ */

static voice *voice_from(const zb_audio *au, uint32_t handle) {
  uint32_t slot = handle & 0xFFu;
  uint16_t gen = (uint16_t)(handle >> 8);
  if (handle == 0 || slot >= ZB_AUDIO_VOICES) {
    return NULL;
  }
  voice *v = (voice *)&au->voices[slot];
  return (v->active && v->generation == gen) ? v : NULL;
}

uint32_t zb_audio_play(zb_audio *au, const char *path, const zb_play_desc *desc) {
  zb_play_desc d = desc ? *desc : zb_play_defaults();
  bool one_shot = !d.loop && d.group != ZB_AUDIO_MUSIC;
  lock(au);
  int slot = -1;
  for (int i = 0; i < ZB_AUDIO_VOICES; ++i) {
    if (!au->voices[i].active) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    unlock(au);
    zb_log_once(ZB_LOG_WARN, "audio:voices", "all %d audio voices are busy; sound dropped", ZB_AUDIO_VOICES);
    return 0;
  }
  voice *v = &au->voices[slot];
  uint16_t gen = (uint16_t)(v->generation + 1);
  memset(v, 0, sizeof(*v));
  v->generation = gen ? gen : 1;
  uint32_t handle = ((uint32_t)v->generation << 8) | (uint32_t)slot;
  if (au->locked && one_shot) {
    /* Dropped: report as a stopped voice. */
    unlock(au);
    return handle;
  }
  unlock(au);
  sound *s = get_sound(au, path);
  lock(au);
  v->active = s != NULL;
  v->snd = s;
  if (s) {
    s->refs++;
  }
  v->volume = d.volume;
  v->pitch = d.pitch > 0.0f ? d.pitch : 1.0f;
  v->loop = d.loop;
  v->group = d.group;
  v->deferred = au->locked;
  unlock(au);
  return handle;
}

void zb_audio_stop(zb_audio *au, uint32_t handle) {
  lock(au);
  voice *v = voice_from(au, handle);
  if (v) {
    voice_release(au, v);
  }
  unlock(au);
}

void zb_audio_set_voice_volume(zb_audio *au, uint32_t handle, float volume) {
  lock(au);
  voice *v = voice_from(au, handle);
  if (v) {
    v->volume = volume;
  }
  unlock(au);
}

zb_voice_state zb_audio_voice_state(const zb_audio *au, uint32_t handle) {
  return voice_from(au, handle) ? ZB_VOICE_PLAYING : ZB_VOICE_STOPPED;
}

size_t zb_audio_voice_decoder_bytes(const zb_audio *au, uint32_t handle) {
  const voice *v = voice_from(au, handle);
  return (v && v->vorbis) ? ZB_AUDIO_STREAM_BUFFER : 0;
}

void zb_audio_set_master_volume(zb_audio *au, float volume) { au->master = volume < 0 ? 0 : volume; }

void zb_audio_set_group_volume(zb_audio *au, zb_audio_group group, float volume) {
  if ((unsigned)group < ZB_AUDIO_GROUP_COUNT) {
    au->group_volume[group] = volume < 0 ? 0 : volume;
  }
}

float zb_audio_group_volume(const zb_audio *au, zb_audio_group group) {
  return (unsigned)group < ZB_AUDIO_GROUP_COUNT ? au->group_volume[group] : 0.0f;
}

size_t zb_audio_active_voices(const zb_audio *au) {
  size_t n = 0;
  for (int i = 0; i < ZB_AUDIO_VOICES; ++i) {
    n += au->voices[i].active ? 1u : 0u;
  }
  return n;
}

/* ---- Mixing ------------------------------------------------------------------ */

static bool stream_open(zb_audio *au, voice *v) {
  v->vorbis_mem = zb_alloc(au->a, ZB_AUDIO_STREAM_BUFFER);
  if (!v->vorbis_mem) {
    return false;
  }
  stb_vorbis_alloc alloc = {(char *)v->vorbis_mem, (int)ZB_AUDIO_STREAM_BUFFER};
  int error = 0;
  v->vorbis = stb_vorbis_open_memory((const unsigned char *)v->snd->compressed, (int)v->snd->compressed_len,
                                     &error, &alloc);
  return v->vorbis != NULL;
}

/* Next stereo frame of a streaming voice; false at the end. */
static bool stream_frame(voice *v, float *l, float *r) {
  if (v->stream_pos >= v->stream_len) {
    if (v->stream_end) {
      return false;
    }
    int n = stb_vorbis_get_samples_float_interleaved(v->vorbis, 2, v->stream_buf, 2048 * 2);
    if (n <= 0) {
      if (v->loop) {
        stb_vorbis_seek_start(v->vorbis);
        n = stb_vorbis_get_samples_float_interleaved(v->vorbis, 2, v->stream_buf, 2048 * 2);
      }
      if (n <= 0) {
        v->stream_end = true;
        return false;
      }
    }
    v->stream_len = n;
    v->stream_pos = 0;
  }
  *l = v->stream_buf[v->stream_pos * 2];
  *r = v->stream_buf[v->stream_pos * 2 + 1];
  return true;
}

/* Mixes (or, with out == NULL, only advances) one voice. */
static void mix_voice(zb_audio *au, voice *v, float *out, int frames) {
  sound *s = v->snd;
  if (!s || s->loading || v->deferred) {
    return;
  }
  if (s->failed) {
    voice_release(au, v);
    return;
  }
  double step = (double)s->rate / (double)ZB_AUDIO_RATE * (double)v->pitch;
  float gain = v->volume * au->group_volume[v->group] * au->master;
  if (s->compressed) {
    if (!v->vorbis && !stream_open(au, v)) {
      voice_release(au, v);
      return;
    }
    for (int i = 0; i < frames; ++i) {
      v->stream_frac += step;
      float l = 0.0f, r = 0.0f;
      while (v->stream_frac >= 1.0) {
        if (!stream_frame(v, &l, &r)) {
          voice_release(au, v);
          return;
        }
        v->stream_pos++;
        v->stream_frac -= 1.0;
      }
      if (out) {
        out[i * 2] += l * gain;
        out[i * 2 + 1] += r * gain;
      }
    }
    return;
  }
  double length = (double)s->frames;
  for (int i = 0; i < frames; ++i) {
    if (v->pos >= length) {
      if (!v->loop || length <= 0.0) {
        voice_release(au, v);
        return;
      }
      v->pos = fmod(v->pos, length);
    }
    if (out) {
      size_t i0 = (size_t)v->pos;
      size_t i1 = i0 + 1 < s->frames ? i0 + 1 : (v->loop ? 0 : i0);
      float t = (float)(v->pos - (double)i0);
      out[i * 2] += (s->samples[i0 * 2] * (1.0f - t) + s->samples[i1 * 2] * t) * gain;
      out[i * 2 + 1] += (s->samples[i0 * 2 + 1] * (1.0f - t) + s->samples[i1 * 2 + 1] * t) * gain;
    }
    v->pos += step;
  }
  if (!v->loop && v->pos >= length) {
    voice_release(au, v); /* finished exactly at the end of this block */
  }
}

void zb_audio_mix(zb_audio *au, float *out, int frames) {
  memset(out, 0, (size_t)frames * 2 * sizeof(float));
  lock(au);
  for (int k = 0; k < ZB_AUDIO_VOICES; ++k) {
    if (au->voices[k].active) {
      mix_voice(au, &au->voices[k], out, frames);
    }
  }
  unlock(au);
  /* Peak limiter: fast attack, slow release, then a hard clamp. */
  float peak = 0.0f;
  for (int i = 0; i < frames * 2; ++i) {
    float m = fabsf(out[i]);
    peak = m > peak ? m : peak;
  }
  float target = peak > 0.95f ? 0.95f / peak : 1.0f;
  if (target < au->limiter_gain) {
    au->limiter_gain = target;
  } else {
    au->limiter_gain += (target - au->limiter_gain) * 0.05f;
  }
  for (int i = 0; i < frames * 2; ++i) {
    float s = out[i] * au->limiter_gain;
    out[i] = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
  }
}

void zb_audio_tick(zb_audio *au, double dt) {
  if (au->has_device) {
    return;
  }
  int frames = (int)(dt * (double)ZB_AUDIO_RATE + 0.5);
  lock(au);
  for (int k = 0; k < ZB_AUDIO_VOICES; ++k) {
    if (au->voices[k].active) {
      mix_voice(au, &au->voices[k], NULL, frames);
    }
  }
  unlock(au);
}
