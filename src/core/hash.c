/* SPDX-License-Identifier: MIT */
#include "core/hash.h"

#include <string.h>

#define K1 0x9e3779b97f4a7c15ULL
#define K2 0xc2b2ae3d27d4eb4fULL
#define K3 0x165667b19e3779f9ULL

uint64_t zb_mix64(uint64_t x) {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

static uint64_t load_le(const unsigned char *p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) {
    v |= (uint64_t)p[i] << (8u * i);
  }
  return v;
}

uint64_t zb_hash64(const void *data, size_t len, uint64_t seed) {
  const unsigned char *p = data;
  uint64_t h = zb_mix64(seed ^ K1) ^ ((uint64_t)len * K2);
  while (len >= 8) {
    h = zb_mix64(h ^ load_le(p, 8)) * K3;
    p += 8;
    len -= 8;
  }
  if (len) {
    h = zb_mix64(h ^ load_le(p, len) ^ ((uint64_t)len << 56)) * K3;
  }
  return zb_mix64(h);
}

uint64_t zb_hash_str(const char *s) { return zb_hash64(s, strlen(s), 0); }

uint64_t zb_hash_combine(uint64_t a, uint64_t b) { return zb_mix64(a * K3 + zb_mix64(b + K1)); }

uint64_t zb_rng_at(uint64_t seed, uint64_t stream, uint64_t counter) {
  return zb_mix64(zb_mix64(seed + K1) ^ zb_mix64(stream * K2 + K3) ^ (counter * K1));
}

zb_rng zb_rng_make(uint64_t seed, uint64_t stream) {
  zb_rng r = {seed, stream, 0};
  return r;
}

uint64_t zb_rng_next(zb_rng *r) { return zb_rng_at(r->seed, r->stream, r->counter++); }

double zb_rng_next_double(zb_rng *r) {
  return (double)(zb_rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

int64_t zb_rng_range(zb_rng *r, int64_t lo, int64_t hi) {
  ZB_ASSERT(lo <= hi);
  uint64_t span = (uint64_t)hi - (uint64_t)lo + 1u;
  if (span == 0) { /* full 64-bit range */
    return (int64_t)zb_rng_next(r);
  }
  /* Rejection sampling avoids modulo bias. */
  uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
  uint64_t v;
  do {
    v = zb_rng_next(r);
  } while (v >= limit);
  return (int64_t)((uint64_t)lo + v % span);
}

void zb_hasher_init(zb_hasher *h, uint64_t seed) {
  h->state = zb_mix64(seed ^ K2);
  h->length = 0;
}

void zb_hasher_bytes(zb_hasher *h, const void *data, size_t len) {
  h->state = zb_hash_combine(h->state, zb_hash64(data, len, h->length));
  h->length += len;
}

void zb_hasher_u64(zb_hasher *h, uint64_t v) {
  unsigned char b[8];
  for (unsigned i = 0; i < 8; ++i) {
    b[i] = (unsigned char)(v >> (8u * i));
  }
  zb_hasher_bytes(h, b, sizeof b);
}

void zb_hasher_f64(zb_hasher *h, double v) {
  uint64_t bits;
  if (v != v) {
    bits = 0x7ff8000000000000ULL;
  } else {
    memcpy(&bits, &v, sizeof bits);
  }
  zb_hasher_u64(h, bits);
}

void zb_hasher_str(zb_hasher *h, const char *s) {
  size_t len = strlen(s);
  zb_hasher_u64(h, (uint64_t)len);
  zb_hasher_bytes(h, s, len);
}

uint64_t zb_hasher_final(const zb_hasher *h) { return zb_mix64(h->state ^ h->length); }

void zb_hash_hex(uint64_t h, char *buf) {
  static const char digits[] = "0123456789abcdef";
  for (int i = 15; i >= 0; --i) {
    buf[i] = digits[h & 0xfu];
    h >>= 4;
  }
  buf[16] = '\0';
}
