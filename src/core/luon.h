/* SPDX-License-Identifier: MIT */
/* "luon": a parser for the literal subset of Lua table syntax, used for
 * package manifests and data files. It never executes code: only nil,
 * booleans, numbers, strings and tables are accepted, optionally preceded by
 * `return`. Every value of a document lives in one arena. */
#ifndef ZB_CORE_LUON_H
#define ZB_CORE_LUON_H

#include "core/alloc.h"

typedef enum zb_value_type { ZB_VAL_NIL, ZB_VAL_BOOL, ZB_VAL_NUMBER, ZB_VAL_STRING, ZB_VAL_TABLE } zb_value_type;

typedef struct zb_table zb_table;

typedef struct zb_value {
  zb_value_type type;
  union {
    bool b;
    double num;
    struct {
      const char *ptr;
      size_t len;
    } str;
    zb_table *table;
  } as;
} zb_value;

typedef struct zb_table_entry {
  zb_value key;
  zb_value value;
} zb_table_entry;

struct zb_table {
  zb_table_entry *entries; /* in source order; positional items have keys 1..n */
  size_t count;
};

typedef struct zb_luon_error {
  int line;
  int column;
  char message[160];
} zb_luon_error;

typedef struct zb_luon_doc {
  zb_arena arena;
  zb_value root;
} zb_luon_doc;

#define ZB_LUON_MAX_DEPTH 64

zb_result zb_luon_parse(zb_luon_doc *doc, const zb_allocator *a, const char *text, size_t len, zb_luon_error *err);
void zb_luon_free(zb_luon_doc *doc);

/* Lookups return NULL when t is not a table or the key is missing. */
const zb_value *zb_table_get(const zb_value *t, const char *key);
/* 1-based positional lookup. */
const zb_value *zb_table_index(const zb_value *t, size_t index);
/* Length of the positional part (keys 1..n without gaps). */
size_t zb_table_len(const zb_value *t);
size_t zb_table_count(const zb_value *t);
const zb_table_entry *zb_table_entry_at(const zb_value *t, size_t i);

bool zb_value_is(const zb_value *v, zb_value_type type);
/* Convenience accessors with defaults for missing or mismatched values. */
const char *zb_value_str(const zb_value *v, const char *def);
double zb_value_num(const zb_value *v, double def);
bool zb_value_bool(const zb_value *v, bool def);
const char *zb_value_type_name(zb_value_type type);

#endif
