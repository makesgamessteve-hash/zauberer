/* SPDX-License-Identifier: MIT */
#include "audio/audio.h"
#include "core/log.h"
#include "core/memfs.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static zb_memfs *g_fs;
static zb_vfs *g_vfs;

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  g_fs = zb_memfs_create(zb_test_alloc());
  g_vfs = NULL;
}

void tearDown(void) {
  zb_vfs_destroy(g_vfs);
  zb_memfs_destroy(g_fs);
  zb_log_reset();
  zb_test_end();
}

static void put_u16(unsigned char *p, unsigned v) {
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void put_u32(unsigned char *p, uint32_t v) {
  put_u16(p, v & 0xFFFF);
  put_u16(p + 2, v >> 16);
}

/* Builds a WAV file in a malloc'ed buffer. */
static unsigned char *make_wav(int format, int channels, int bits, uint32_t rate, uint32_t frames, size_t *len,
                               bool extensible) {
  uint32_t bytes = (uint32_t)(bits / 8) * (uint32_t)channels;
  uint32_t data_len = frames * bytes;
  uint32_t fmt_len = extensible ? 40u : 16u;
  *len = 12 + 8 + fmt_len + 8 + data_len + (data_len & 1u) + 10; /* plus an odd "LIST" chunk */
  unsigned char *w = calloc(1, *len);
  memcpy(w, "RIFF", 4);
  put_u32(w + 4, (uint32_t)(*len - 8));
  memcpy(w + 8, "WAVE", 4);
  unsigned char *p = w + 12;
  memcpy(p, "LIST", 4);
  put_u32(p + 4, 1);
  p += 10; /* 8 + 1 byte + pad */
  memcpy(p, "fmt ", 4);
  put_u32(p + 4, fmt_len);
  put_u16(p + 8, extensible ? 0xFFFEu : (unsigned)format);
  put_u16(p + 10, (unsigned)channels);
  put_u32(p + 12, rate);
  put_u32(p + 16, rate * bytes);
  put_u16(p + 20, bytes);
  put_u16(p + 22, (unsigned)bits);
  if (extensible) {
    put_u16(p + 24, 22);
    put_u16(p + 32, (unsigned)format);
  }
  p += 8 + fmt_len;
  memcpy(p, "data", 4);
  put_u32(p + 4, data_len);
  p += 8;
  for (uint32_t i = 0; i < frames * (uint32_t)channels; ++i) {
    double s = sin((double)i * 0.05) * 0.9;
    unsigned char *q = p + (size_t)i * (size_t)(bits / 8);
    if (format == 3) {
      float f = (float)s;
      memcpy(q, &f, 4);
    } else if (bits == 8) {
      q[0] = (unsigned char)(128 + (int)(s * 127));
    } else if (bits == 16) {
      put_u16(q, (unsigned)(uint16_t)(int16_t)(s * 32767));
    } else if (bits == 24) {
      int32_t v = (int32_t)(s * 8388607);
      q[0] = (unsigned char)(v & 0xFF);
      q[1] = (unsigned char)((v >> 8) & 0xFF);
      q[2] = (unsigned char)((v >> 16) & 0xFF);
    } else {
      put_u32(q, (uint32_t)(int32_t)(s * 2147483647.0));
    }
  }
  return w;
}

static void put_wav(const char *path, uint32_t frames) {
  size_t len;
  unsigned char *w = make_wav(1, 1, 16, 48000, frames, &len, false);
  zb_memfs_put(g_fs, path, w, len);
  free(w);
}

/* A stereo WAV whose samples are all at full scale. */
static void put_full_scale(const char *path, uint32_t frames) {
  size_t len;
  unsigned char *w = make_wav(1, 2, 16, 48000, frames, &len, false);
  unsigned char *pcm = w + len - (size_t)frames * 4u;
  for (uint32_t i = 0; i < frames * 2; ++i) {
    put_u16(pcm + i * 2, 32767);
  }
  zb_memfs_put(g_fs, path, w, len);
  free(w);
}

static void put_real(const char *memfs_path, const char *repo_path) {
  char full[1024];
  snprintf(full, sizeof full, "%s/%s", zb_test_source_dir(), repo_path);
  size_t len = 0;
  char *data = zb_test_read_file(full, &len);
  TEST_ASSERT_NOT_NULL_MESSAGE(data, full);
  zb_memfs_put(g_fs, memfs_path, data, len);
  free(data);
}

static zb_audio *make_audio(const zb_thread_api *threads) {
  put_wav("assets/base/sounds/beep.wav", 48000); /* 1 second */
  put_wav("assets/base/sounds/short.wav", 4800); /* 0.1 second */
  put_full_scale("assets/base/sounds/loud.wav", 48000);
  put_real("assets/base/music/loop.ogg", "assets/base/music/loop.ogg");
  zb_memfs_put(g_fs, "assets/base/sounds/bad.wav", "RIFF....WAVEjunk", 16);
  zb_memfs_put(g_fs, "assets/base/music/bad.ogg", "OggS not really", 15);
  static const char manifest[] = "return { id = 'base', version = '0.1.0', api = '1.0', license = 'MIT' }";
  zb_memfs_put(g_fs, "assets/base/package.lua", manifest, sizeof manifest - 1);
  g_vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(g_fs), NULL);
  zb_vfs_add_root(g_vfs, "assets");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_mount(g_vfs, 1, 0));
  zb_audio *au = zb_audio_create(zb_test_alloc(), g_vfs, threads);
  TEST_ASSERT_NOT_NULL(au);
  return au;
}

static void settle(void) {
  for (int i = 0; i < 3; ++i) {
    zb_vfs_poll(g_vfs);
  }
}

static float peak_of(const float *buf, int frames) {
  float p = 0.0f;
  for (int i = 0; i < frames * 2; ++i) {
    p = fmaxf(p, fabsf(buf[i]));
  }
  return p;
}

/* spec: audio-playback: Play and stop a loop */
static void test_play_and_stop_loop(void) {
  zb_audio *au = make_audio(zb_test_thread_api());
  zb_audio_set_device(au, true);
  TEST_ASSERT_TRUE(zb_audio_has_device(au));
  zb_play_desc d = zb_play_defaults();
  d.loop = true;
  uint32_t h = zb_audio_play(au, "sounds/short.wav", &d);
  TEST_ASSERT_NOT_EQUAL(0, h);
  settle();
  static float buf[4800 * 2];
  /* 2 s of output in 0.1 s blocks: the 0.1 s sound loops and stays audible */
  for (int block = 0; block < 20; ++block) {
    zb_audio_mix(au, buf, 4800);
    TEST_ASSERT_TRUE(peak_of(buf, 4800) > 0.1f);
  }
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, h));
  zb_audio_stop(au, h);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, h));
  zb_audio_mix(au, buf, 4800);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak_of(buf, 4800));
  zb_audio_stop(au, h); /* stale handle: no effect */
  zb_audio_set_voice_volume(au, h, 0.5f);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, 0));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, 0xFFu));
  zb_audio_tick(au, 1.0); /* ignored with a device */
  /* Stopping everything (the game starting over). */
  uint32_t a = zb_audio_play(au, "sounds/short.wav", &d);
  uint32_t b = zb_audio_play(au, "sounds/short.wav", &d);
  settle();
  zb_audio_stop_all(au);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, a));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, b));
  TEST_ASSERT_EQUAL_size_t(0, zb_audio_active_voices(au));
  zb_audio_destroy(au);
}

/* spec: audio-playback: Many voices without clipping */
static void test_many_voices(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  uint32_t handles[ZB_AUDIO_VOICES];
  for (int i = 0; i < 32; ++i) {
    handles[i] = zb_audio_play(au, "sounds/loud.wav", NULL);
    TEST_ASSERT_NOT_EQUAL(0, handles[i]);
  }
  settle();
  TEST_ASSERT_EQUAL_size_t(32, zb_audio_active_voices(au));
  static float buf[1024 * 2];
  for (int block = 0; block < 4; ++block) {
    zb_audio_mix(au, buf, 1024);
    TEST_ASSERT_TRUE(peak_of(buf, 1024) <= 1.0f);
    TEST_ASSERT_TRUE(peak_of(buf, 1024) > 0.5f);
  }
  TEST_ASSERT_EQUAL_size_t(32, zb_audio_active_voices(au));
  for (int i = 32; i < ZB_AUDIO_VOICES; ++i) {
    handles[i] = zb_audio_play(au, "sounds/loud.wav", NULL);
  }
  TEST_ASSERT_EQUAL_UINT32(0, zb_audio_play(au, "sounds/loud.wav", NULL)); /* all busy */
  zb_audio_destroy(au);
}

/* spec: audio-playback: Music volume group */
static void test_music_volume_group(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  zb_audio_set_group_volume(au, ZB_AUDIO_MUSIC, 0.0f);
  zb_audio_set_group_volume(au, ZB_AUDIO_GROUP_COUNT, 1.0f); /* ignored */
  zb_audio_set_group_volume(au, ZB_AUDIO_EFFECTS, -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_audio_group_volume(au, ZB_AUDIO_EFFECTS));
  zb_audio_set_group_volume(au, ZB_AUDIO_EFFECTS, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_audio_group_volume(au, ZB_AUDIO_GROUP_COUNT));
  zb_play_desc music = zb_play_defaults();
  music.group = ZB_AUDIO_MUSIC;
  music.loop = true;
  uint32_t m = zb_audio_play(au, "music/loop.ogg", &music);
  settle();
  static float buf[2048 * 2];
  zb_audio_mix(au, buf, 2048);
  zb_audio_mix(au, buf, 2048);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak_of(buf, 2048));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, m));
  uint32_t e = zb_audio_play(au, "sounds/beep.wav", NULL);
  settle();
  zb_audio_mix(au, buf, 2048);
  TEST_ASSERT_TRUE(peak_of(buf, 2048) > 0.1f);
  zb_audio_set_voice_volume(au, e, 0.0f);
  zb_audio_mix(au, buf, 2048);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak_of(buf, 2048));
  zb_audio_set_master_volume(au, -2.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_audio_master_volume(au)); /* (read back, clamped) */
  zb_audio_destroy(au);
}

/* spec: audio-playback: Long track */
static void test_long_track_memory(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  zb_play_desc music = zb_play_defaults();
  music.group = ZB_AUDIO_MUSIC;
  music.loop = true;
  music.pitch = 2.0f;
  uint32_t m = zb_audio_play(au, "music/loop.ogg", &music);
  settle();
  static float buf[4096 * 2];
  /* Ten minutes of track time (pitch 2: five minutes of output); the looped
   * track is decoded incrementally, so the decoder memory stays fixed. */
  int blocks = (int)(300.0 * ZB_AUDIO_RATE / 4096.0);
  float peak = 0.0f;
  for (int i = 0; i < blocks; ++i) {
    zb_audio_mix(au, buf, 4096);
    peak = fmaxf(peak, peak_of(buf, 4096));
    TEST_ASSERT_TRUE(zb_audio_voice_decoder_bytes(au, m) < 1024u * 1024u);
  }
  TEST_ASSERT_TRUE(peak > 0.01f);
  TEST_ASSERT_EQUAL_size_t(ZB_AUDIO_STREAM_BUFFER, zb_audio_voice_decoder_bytes(au, m));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, m));
  zb_audio_destroy(au);
}

static void test_stream_ends_without_loop(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  zb_play_desc music = zb_play_defaults();
  music.group = ZB_AUDIO_MUSIC;
  uint32_t m = zb_audio_play(au, "music/loop.ogg", &music);
  uint32_t bad = zb_audio_play(au, "music/bad.ogg", &music);
  settle();
  static float buf[4096 * 2];
  for (int i = 0; i < 2000 && zb_audio_voice_state(au, m) == ZB_VOICE_PLAYING; ++i) {
    zb_audio_mix(au, buf, 4096);
  }
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, m));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, bad));
  zb_audio_destroy(au);
}

/* spec: audio-playback: Music requested before interaction */
static void test_unlock_gate(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  zb_audio_set_locked(au, true);
  TEST_ASSERT_TRUE(zb_audio_is_locked(au));
  zb_play_desc music = zb_play_defaults();
  music.group = ZB_AUDIO_MUSIC;
  music.loop = true;
  uint32_t m = zb_audio_play(au, "music/loop.ogg", &music);
  uint32_t shot = zb_audio_play(au, "sounds/beep.wav", NULL);
  TEST_ASSERT_NOT_EQUAL(0, shot);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, shot)); /* dropped */
  settle();
  static float buf[2048 * 2];
  zb_audio_mix(au, buf, 2048);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak_of(buf, 2048));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, m));
  zb_audio_set_locked(au, false); /* first gesture */
  float peak = 0.0f;
  for (int i = 0; i < 10; ++i) {
    zb_audio_mix(au, buf, 2048);
    peak = fmaxf(peak, peak_of(buf, 2048));
  }
  TEST_ASSERT_TRUE(peak > 0.01f);
  zb_audio_destroy(au);
}

/* spec: audio-playback: Headless playback */
static void test_headless_playback(void) {
  zb_audio *au = make_audio(NULL);
  TEST_ASSERT_FALSE(zb_audio_has_device(au));
  uint32_t h = zb_audio_play(au, "sounds/beep.wav", NULL);
  settle();
  zb_audio_tick(au, 1.0 / 60.0);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, h));
  for (int t = 1; t < 59; ++t) {
    zb_audio_tick(au, 1.0 / 60.0);
  }
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_PLAYING, zb_audio_voice_state(au, h));
  zb_audio_tick(au, 1.0 / 60.0); /* tick 60 */
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, h));
  zb_audio_destroy(au);
}

static void test_missing_and_broken_sounds(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  uint32_t a = zb_audio_play(au, "sounds/missing.wav", NULL);
  uint32_t b = zb_audio_play(au, "sounds/bad.wav", NULL);
  uint32_t c = zb_audio_play(au, "sounds/missing.wav", NULL);
  settle();
  static float buf[256 * 2];
  zb_audio_mix(au, buf, 256);
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, a));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, b));
  TEST_ASSERT_EQUAL_INT(ZB_VOICE_STOPPED, zb_audio_voice_state(au, c));
  TEST_ASSERT_EQUAL_size_t(0, zb_audio_active_voices(au));
  /* a voice released while its sound is still loading */
  uint32_t d = zb_audio_play(au, "sounds/short.wav", NULL);
  zb_audio_stop(au, d);
  zb_audio_destroy(au); /* the pending request is released */
}

static void test_wav_formats(void) {
  struct {
    int format, channels, bits;
    bool ext;
  } cases[] = {{1, 1, 8, false},  {1, 2, 16, false}, {1, 1, 24, false},
               {1, 2, 32, false}, {3, 1, 32, false}, {1, 2, 16, true}};
  for (size_t i = 0; i < ZB_ARRAY_LEN(cases); ++i) {
    size_t len;
    unsigned char *w = make_wav(cases[i].format, cases[i].channels, cases[i].bits, 22050, 100, &len, cases[i].ext);
    float *samples;
    size_t frames;
    int rate;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_audio_decode_wav(zb_test_alloc(), w, len, &samples, &frames, &rate));
    TEST_ASSERT_EQUAL_size_t(100, frames);
    TEST_ASSERT_EQUAL_INT(22050, rate);
    float expect = (float)(sin(0.05 * (cases[i].channels == 2 ? 2.0 : 1.0)) * 0.9);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, expect, samples[2]);
    zb_free(zb_test_alloc(), samples);
    free(w);
  }
  float *s;
  size_t frames;
  int rate;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_audio_decode_wav(zb_test_alloc(), "RIFF", 4, &s, &frames, &rate));
  size_t len;
  unsigned char *w = make_wav(2, 1, 16, 22050, 10, &len, false); /* ADPCM: unsupported */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_audio_decode_wav(zb_test_alloc(), w, len, &s, &frames, &rate));
  put_u32(w + 16, 0xFFFFFFu); /* LIST chunk size beyond the end */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_audio_decode_wav(zb_test_alloc(), w, len, &s, &frames, &rate));
  free(w);
  w = make_wav(1, 1, 16, 22050, 10, &len, false);
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM,
                        zb_audio_decode_wav(zb_failing_init(&fa, zb_test_alloc(), 0), w, len, &s, &frames, &rate));
  free(w);
}

static void test_create_failure(void) {
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(zb_audio_create(zb_failing_init(&fa, zb_test_alloc(), 0), NULL, NULL));
  zb_audio_destroy(NULL);
}

static void test_out_of_memory_paths(void) {
  zb_audio *au = make_audio(NULL);
  zb_audio_set_device(au, true);
  zb_audio_destroy(au);
  zb_vfs_destroy(g_vfs);
  g_vfs = NULL;
  for (long budget = 0; budget < 8; ++budget) {
    g_vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(g_fs), NULL);
    zb_vfs_add_root(g_vfs, "assets");
    zb_vfs_mount(g_vfs, 1, 0);
    zb_failing_allocator fa;
    zb_audio *f = zb_audio_create(zb_failing_init(&fa, zb_test_alloc(), budget), g_vfs, NULL);
    if (f) {
      zb_audio_set_device(f, true);
      zb_play_desc music = zb_play_defaults();
      music.group = ZB_AUDIO_MUSIC;
      zb_audio_play(f, "music/loop.ogg", &music);
      zb_audio_play(f, "sounds/beep.wav", NULL);
      settle();
      static float buf[64 * 2];
      zb_audio_mix(f, buf, 64);
      zb_audio_destroy(f);
    }
    zb_vfs_destroy(g_vfs);
    g_vfs = NULL;
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_play_and_stop_loop);
  RUN_TEST(test_many_voices);
  RUN_TEST(test_music_volume_group);
  RUN_TEST(test_long_track_memory);
  RUN_TEST(test_stream_ends_without_loop);
  RUN_TEST(test_unlock_gate);
  RUN_TEST(test_headless_playback);
  RUN_TEST(test_missing_and_broken_sounds);
  RUN_TEST(test_wav_formats);
  RUN_TEST(test_create_failure);
  RUN_TEST(test_out_of_memory_paths);
  return UNITY_END();
}
