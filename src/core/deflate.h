/* SPDX-License-Identifier: MIT */
/* zlib-format compression for saved data (world bundles), using the vendored
 * stb compressor and decompressor. */
#ifndef ZB_CORE_DEFLATE_H
#define ZB_CORE_DEFLATE_H

#include "core/array.h"

/* Appends the zlib stream of data to out (a byte array). */
zb_result zb_deflate(const void *data, size_t len, zb_array *out);
/* Decompresses into out, which must be exactly out_len bytes long. */
zb_result zb_inflate(const void *data, size_t len, void *out, size_t out_len);

#endif
