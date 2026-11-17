/* SPDX-License-Identifier: MIT */
#include "render/image.h"

#include <stdlib.h>
#include <string.h>

#include "stb_image.h"
#include "stb_image_write.h"

zb_result zb_image_decode_png(const zb_allocator *a, const void *data, size_t len, zb_image *out) {
  memset(out, 0, sizeof(*out));
  if (!data || len == 0 || len > (size_t)INT32_MAX) {
    return ZB_ERR_INVALID;
  }
  int w, h, comp;
  unsigned char *px = stbi_load_from_memory(data, (int)len, &w, &h, &comp, 4);
  if (!px) {
    return ZB_ERR_PARSE;
  }
  size_t bytes = (size_t)w * (size_t)h * 4u;
  out->rgba = zb_alloc(a, bytes);
  if (!out->rgba) {
    stbi_image_free(px);
    return ZB_ERR_NOMEM;
  }
  memcpy(out->rgba, px, bytes);
  stbi_image_free(px);
  out->width = w;
  out->height = h;
  return ZB_OK;
}

typedef struct png_sink {
  const zb_allocator *a;
  unsigned char *data;
  size_t len;
  bool oom;
} png_sink;

static void png_write(void *ctx, void *data, int size) {
  png_sink *s = ctx;
  unsigned char *grown = zb_realloc(s->a, s->data, s->len + (size_t)size);
  if (!grown) {
    s->oom = true;
    return;
  }
  memcpy(grown + s->len, data, (size_t)size);
  s->data = grown;
  s->len += (size_t)size;
}

zb_result zb_image_encode_png(const zb_allocator *a, const zb_image *img, unsigned char **png, size_t *len) {
  png_sink s = {a, NULL, 0, false};
  int ok = stbi_write_png_to_func(png_write, &s, img->width, img->height, 4, img->rgba, img->width * 4);
  if (!ok || s.oom) {
    zb_free(a, s.data);
    return s.oom ? ZB_ERR_NOMEM : ZB_ERR_INVALID;
  }
  *png = s.data;
  *len = s.len;
  return ZB_OK;
}

zb_result zb_image_checkerboard(const zb_allocator *a, int size, zb_image *out) {
  out->width = size;
  out->height = size;
  out->rgba = zb_alloc(a, (size_t)size * (size_t)size * 4u);
  if (!out->rgba) {
    return ZB_ERR_NOMEM;
  }
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      unsigned char *p = out->rgba + ((size_t)y * (size_t)size + (size_t)x) * 4u;
      bool on = ((x / 2) + (y / 2)) % 2 == 0;
      p[0] = on ? 255 : 0;
      p[1] = 0;
      p[2] = on ? 255 : 0;
      p[3] = 255;
    }
  }
  return ZB_OK;
}

void zb_image_free(const zb_allocator *a, zb_image *img) {
  zb_free(a, img->rgba);
  img->rgba = NULL;
  img->width = 0;
  img->height = 0;
}

void zb_image_flip_rows(zb_image *img) {
  size_t stride = (size_t)img->width * 4u;
  for (int y = 0; y < img->height / 2; ++y) {
    unsigned char *top = img->rgba + (size_t)y * stride;
    unsigned char *bottom = img->rgba + (size_t)(img->height - 1 - y) * stride;
    for (size_t i = 0; i < stride; ++i) {
      unsigned char t = top[i];
      top[i] = bottom[i];
      bottom[i] = t;
    }
  }
}

bool zb_image_compare(const zb_image *a, const zb_image *b, int channel_tolerance, double max_fraction,
                      zb_image_diff *diff) {
  zb_image_diff d = {false, 1.0, 255};
  if (a->width == b->width && a->height == b->height) {
    d.same_size = true;
    d.max_delta = 0;
    size_t over = 0;
    size_t n = (size_t)a->width * (size_t)a->height;
    for (size_t i = 0; i < n; ++i) {
      int worst = 0;
      for (int c = 0; c < 4; ++c) {
        int delta = abs((int)a->rgba[i * 4 + (size_t)c] - (int)b->rgba[i * 4 + (size_t)c]);
        worst = delta > worst ? delta : worst;
      }
      d.max_delta = worst > d.max_delta ? worst : d.max_delta;
      if (worst > channel_tolerance) {
        over++;
      }
    }
    d.fraction_over = n ? (double)over / (double)n : 0.0;
  }
  if (diff) {
    *diff = d;
  }
  return d.same_size && d.fraction_over <= max_fraction;
}
