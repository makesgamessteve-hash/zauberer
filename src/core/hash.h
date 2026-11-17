/* SPDX-License-Identifier: MIT */
/* Deterministic hashing, counter-based random numbers and state hashing.
 * All functions produce identical results on every platform: inputs are read
 * byte by byte in little-endian order and only integer arithmetic is used. */
#ifndef ZB_CORE_HASH_H
#define ZB_CORE_HASH_H

#include "core/base.h"

/* SplitMix64 finalizer: a strong 64-bit mixing function. */
uint64_t zb_mix64(uint64_t x);

/* Hash a byte buffer with a seed. */
uint64_t zb_hash64(const void *data, size_t len, uint64_t seed);

/* Hash a NUL-terminated string. */
uint64_t zb_hash_str(const char *s);

/* Combine two hashes (order matters). */
uint64_t zb_hash_combine(uint64_t a, uint64_t b);

/* ---- Counter-based RNG --------------------------------------------------- */

/* The value is a pure function of (seed, stream, counter): no hidden state. */
uint64_t zb_rng_at(uint64_t seed, uint64_t stream, uint64_t counter);

typedef struct zb_rng {
  uint64_t seed;
  uint64_t stream;
  uint64_t counter;
} zb_rng;

zb_rng zb_rng_make(uint64_t seed, uint64_t stream);
uint64_t zb_rng_next(zb_rng *r);
/* Uniform double in [0, 1) with 53 bits of precision. */
double zb_rng_next_double(zb_rng *r);
/* Uniform integer in [lo, hi] (inclusive); lo <= hi. */
int64_t zb_rng_range(zb_rng *r, int64_t lo, int64_t hi);

/* ---- Incremental hasher (for state hashes) ------------------------------- */

typedef struct zb_hasher {
  uint64_t state;
  uint64_t length;
} zb_hasher;

void zb_hasher_init(zb_hasher *h, uint64_t seed);
void zb_hasher_bytes(zb_hasher *h, const void *data, size_t len);
void zb_hasher_u64(zb_hasher *h, uint64_t v);
/* Hashes the IEEE-754 bit pattern; -0.0 and 0.0 differ, NaNs are canonical. */
void zb_hasher_f64(zb_hasher *h, double v);
void zb_hasher_str(zb_hasher *h, const char *s);
uint64_t zb_hasher_final(const zb_hasher *h);

/* Lower-case 16-digit hex representation into buf (at least 17 bytes). */
void zb_hash_hex(uint64_t h, char *buf);

#endif
