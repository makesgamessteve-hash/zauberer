/* SPDX-License-Identifier: MIT */
#include "core/deflate.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"

/* Defined by the stb_image_write implementation unit (third_party/impl), which
 * does not declare it in its header part. */
unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);

zb_result zb_deflate(const void *data, size_t len, zb_array *out) {
  if (len > (size_t)INT_MAX / 2) {
    return ZB_ERR_INVALID;
  }
  int zlen = 0;
  unsigned char *z = stbi_zlib_compress((unsigned char *)(uintptr_t)data, (int)len, &zlen, 6);
  if (!z) {
    return ZB_ERR_NOMEM;
  }
  zb_result r = zb_array_reserve(out, out->len + (size_t)zlen);
  if (r == ZB_OK) {
    memcpy((char *)out->data + out->len, z, (size_t)zlen);
    out->len += (size_t)zlen;
  }
  free(z); /* allocated by stb with malloc */
  return r;
}

zb_result zb_inflate(const void *data, size_t len, void *out, size_t out_len) {
  if (len > (size_t)INT_MAX || out_len > (size_t)INT_MAX) {
    return ZB_ERR_INVALID;
  }
  int n = stbi_zlib_decode_buffer((char *)out, (int)out_len, (const char *)data, (int)len);
  return n == (int)out_len ? ZB_OK : ZB_ERR_INVALID;
}
