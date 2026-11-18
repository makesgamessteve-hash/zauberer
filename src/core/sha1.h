/* SPDX-License-Identifier: MIT */
/* SHA-1 and Base64, for the WebSocket handshake (RFC 6455). SHA-1 is not used
 * for anything that needs collision resistance. */
#ifndef ZB_CORE_SHA1_H
#define ZB_CORE_SHA1_H

#include "core/base.h"

void zb_sha1(const void *data, size_t len, uint8_t out[20]);
/* Writes NUL-terminated Base64 of len bytes; out needs 4 * ((len + 2) / 3) + 1 bytes. */
void zb_base64(const uint8_t *data, size_t len, char *out);

#endif
