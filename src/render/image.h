/* SPDX-License-Identifier: MIT */
/* RGBA8 images: PNG decoding and encoding, placeholders and comparison. */
#ifndef ZB_RENDER_IMAGE_H
#define ZB_RENDER_IMAGE_H

#include "core/alloc.h"

typedef struct zb_image {
  int width;
  int height;
  unsigned char *rgba; /* width * height * 4 bytes, rows top to bottom */
} zb_image;

/* Decodes a PNG from memory. */
zb_result zb_image_decode_png(const zb_allocator *a, const void *data, size_t len, zb_image *out);
/* Encodes RGBA8 as PNG; *png is allocated with a. */
zb_result zb_image_encode_png(const zb_allocator *a, const zb_image *img, unsigned char **png, size_t *len);
/* An n x n magenta/black checkerboard (the missing-texture placeholder). */
zb_result zb_image_checkerboard(const zb_allocator *a, int size, zb_image *out);
void zb_image_free(const zb_allocator *a, zb_image *img);
/* Flips rows in place (GL reads bottom-up). */
void zb_image_flip_rows(zb_image *img);

typedef struct zb_image_diff {
  bool same_size;
  double fraction_over; /* fraction of pixels with a channel delta above tolerance */
  int max_delta;
} zb_image_diff;

/* Compares a against b: a pixel differs when any channel differs by more than
 * channel_tolerance. Returns true when sizes match and the fraction of
 * differing pixels is at most max_fraction. */
bool zb_image_compare(const zb_image *a, const zb_image *b, int channel_tolerance, double max_fraction,
                      zb_image_diff *diff);

#endif
