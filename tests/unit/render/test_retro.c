/* SPDX-License-Identifier: MIT */
#include "core/fs.h"
#include "render/retro.h"
#include "support/test_support.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

/* spec: retro-rendering: Fog hides the distance */
static void test_fog(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_fog_factor(0.0f, 0.001f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_fog_factor(5000.0f, 0.0f));
  float d = zb_fog_density_for(12000.0f, 0.98f);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.98f, zb_fog_factor(12000.0f, d));
  TEST_ASSERT_TRUE(zb_fog_factor(1000.0f, d) < 0.05f); /* near terrain stays clear */
  TEST_ASSERT_TRUE(zb_fog_factor(6000.0f, d) < zb_fog_factor(9000.0f, d));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_fog_density_for(0.0f, 0.5f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_fog_density_for(100.0f, 0.0f));
  TEST_ASSERT_TRUE(zb_fog_density_for(100.0f, 1.0f) > 0.0f);
}

static zb_atmosphere atmosphere(void) {
  zb_atmosphere a;
  memset(&a, 0, sizeof a);
  a.enabled = true;
  a.zenith = zb_v3(0.2f, 0.4f, 0.8f);
  a.horizon = zb_v3(0.7f, 0.8f, 0.9f);
  a.up = zb_v3(0.0f, 1.0f, 0.0f);
  a.sun_dir = zb_v3_normalize(zb_v3(0.0f, 0.3f, -1.0f));
  a.sun_color = zb_v3(1.0f, 0.9f, 0.7f);
  return a;
}

static float dist3(zb_vec3 a, zb_vec3 b) { return zb_v3_length(zb_v3_sub(a, b)); }

/* spec: retro-rendering: Sky gradient and sun */
static void test_sky(void) {
  zb_atmosphere a = atmosphere();
  zb_vec3 away = zb_v3(-1.0f, 0.0f, 0.0f); /* horizontal, away from the sun */
  TEST_ASSERT_TRUE(dist3(zb_sky_color(&a, away), a.horizon) < 0.01f);
  TEST_ASSERT_TRUE(dist3(zb_sky_color(&a, zb_v3(0, 1, 0)), a.zenith) < 0.06f);
  zb_vec3 mid = zb_sky_color(&a, zb_v3_normalize(zb_v3(-1.0f, 0.3f, 0.0f)));
  TEST_ASSERT_TRUE(mid.z > a.zenith.z - 0.01f && mid.x < a.horizon.x && mid.x > a.zenith.x);
  /* The sun disc and its glow. */
  TEST_ASSERT_TRUE(dist3(zb_sky_color(&a, a.sun_dir), a.sun_color) < 1e-6f);
  zb_vec3 near_sun = zb_sky_color(&a, zb_v3_normalize(zb_v3_add(a.sun_dir, zb_v3(0.1f, 0.0f, 0.0f))));
  zb_vec3 plain = zb_sky_color(&a, zb_v3_normalize(zb_v3_add(a.sun_dir, zb_v3(-3.0f, 0.0f, 0.0f))));
  TEST_ASSERT_TRUE(near_sun.x > plain.x);
  /* Below the horizon: horizon colour (fog over the ground). */
  TEST_ASSERT_TRUE(dist3(zb_sky_color(&a, zb_v3(0, -1, 0)), a.horizon) < 0.01f);
  /* The horizon dips as the camera climbs: 1,000 m over a 2,000 km planet ~ 1.8 degrees. */
  float dip = zb_horizon_dip(2000000.0, 1000.0);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.0316f, dip);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_horizon_dip(2000000.0, -5.0));
  a.horizon_dip = dip;
  zb_vec3 slightly_down = zb_v3_normalize(zb_v3(-1.0f, -0.02f, 0.0f));
  zb_atmosphere flat = atmosphere();
  TEST_ASSERT_TRUE(dist3(zb_sky_color(&a, slightly_down), a.horizon) >
                   dist3(zb_sky_color(&flat, slightly_down), flat.horizon));
}

static zb_image load_png(const char *rel) {
  char path[1024];
  snprintf(path, sizeof path, "%s/assets/base/%s", getenv("ZB_SOURCE_DIR") ? getenv("ZB_SOURCE_DIR") : ".", rel);
  FILE *f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n);
  TEST_ASSERT_EQUAL_size_t((size_t)n, fread(data, 1, (size_t)n, f));
  fclose(f);
  zb_image img;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_decode_png(zb_test_alloc(), data, (size_t)n, &img));
  free(data);
  return img;
}

static uint32_t rgb_key(const unsigned char c[3]) { return (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2]; }

/* spec: retro-rendering: Ordered dithering */
static void test_palette(void) {
  zb_image lut = load_png("palettes/base_lut.png");
  zb_image bayer = load_png("palettes/bayer8.png");
  zb_image pal = load_png("palettes/base.png");
  TEST_ASSERT_EQUAL_INT(1024, lut.width);
  TEST_ASSERT_EQUAL_INT(32, lut.height);
  /* Palette colours map to themselves without dithering. */
  for (int i = 0; i < pal.width; ++i) {
    const unsigned char *p = &pal.rgba[i * 4];
    float rgb[3] = {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f};
    unsigned char out[3];
    zb_palette_quantize(&lut, &bayer, 0.0f, rgb, 0, 0, out);
    TEST_ASSERT_EQUAL_UINT32(rgb_key(p), rgb_key(out));
  }
  /* Halfway between two neighbours of a ramp (green 33 and 34): both appear in
   * an 8 x 8 block, in a fixed pattern. */
  const unsigned char *a = &pal.rgba[32 * 4], *b = &pal.rgba[33 * 4];
  float mid[3] = {(a[0] + b[0]) / 510.0f, (a[1] + b[1]) / 510.0f, (a[2] + b[2]) / 510.0f};
  int na = 0, nb = 0;
  uint32_t first_pattern[64];
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      unsigned char out[3];
      zb_palette_quantize(&lut, &bayer, 0.08f, mid, x, y, out);
      na += rgb_key(out) == rgb_key(a);
      nb += rgb_key(out) == rgb_key(b);
      first_pattern[y * 8 + x] = rgb_key(out);
    }
  }
  printf("dither: %d x colour 33, %d x colour 34\n", na, nb);
  TEST_ASSERT_TRUE(na >= 8 && nb >= 8);
  for (int y = 0; y < 8; ++y) { /* the pattern repeats every 8 pixels */
    for (int x = 0; x < 8; ++x) {
      unsigned char out[3];
      zb_palette_quantize(&lut, &bayer, 0.08f, mid, x + 8, y + 16, out);
      TEST_ASSERT_EQUAL_UINT32(first_pattern[y * 8 + x], rgb_key(out));
    }
  }
  /* Out-of-range input clamps. */
  float hot[3] = {2.0f, -1.0f, 0.5f};
  unsigned char out[3];
  zb_palette_quantize(&lut, &bayer, 0.08f, hot, 3, 5, out);
  zb_image_free(zb_test_alloc(), &lut);
  zb_image_free(zb_test_alloc(), &bayer);
  zb_image_free(zb_test_alloc(), &pal);
}

/* spec: webxr-play: Comfort vignette */
static void test_vignette(void) {
  /* Off: no darkening anywhere. */
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_vignette_factor(0.0f, 1.0f, 1.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_vignette_factor(-2.0f, 1.0f, 1.0f));
  /* The clear centre, the dark edge, and a smooth fall in between. */
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_vignette_factor(0.5f, 0.2f, -0.2f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, zb_vignette_factor(0.5f, 1.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, zb_vignette_factor(3.0f, -1.2f, 0.0f)); /* strength clamps to 1 */
  float prev = 1.0f;
  for (int i = 0; i <= 20; ++i) {
    float f = zb_vignette_factor(1.0f, (float)i * 0.06f, 0.0f);
    TEST_ASSERT_TRUE(f <= prev && f >= 0.0f);
    prev = f;
  }
  float mid = zb_vignette_factor(1.0f, 0.725f, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, mid);
  /* Round: the same at the same distance in any direction. */
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, zb_vignette_factor(0.7f, 0.6f, 0.0f), zb_vignette_factor(0.7f, 0.0f, -0.6f));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fog);
  RUN_TEST(test_sky);
  RUN_TEST(test_palette);
  RUN_TEST(test_vignette);
  return UNITY_END();
}
