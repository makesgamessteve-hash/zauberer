/* SPDX-License-Identifier: MIT */
/* Registries: data entries from every package, merged in load order (concept
 * 9.3). A package's data file (data/materials.lua, ...) returns a list of
 * entries, each one of:
 *
 *   { id = "oak", ... }              adds the entry "<package>:oak"
 *   { override = "base:oak", ... }   replaces an existing entry completely
 *   { patch = "base:oak", ... }      changes only the fields it gives
 *
 * A reference without a package ("oak") names the entry with that id when
 * exactly one package defines it. Entries that cannot be applied are skipped
 * with a warning that names the package and the entry; patches are logged. */
#ifndef ZB_CORE_REGISTRY_H
#define ZB_CORE_REGISTRY_H

#include "core/luon.h"

#define ZB_REGISTRY_KEY_MAX 64

typedef struct zb_registry_entry {
  const char *key;     /* "<package>:<id>" */
  const char *id;      /* the part after the colon */
  const char *package; /* the package that added it */
  const char *changed; /* the package that last overrode or patched it (NULL: none) */
  zb_value value;      /* its fields, without id, override and patch */
} zb_registry_entry;

typedef struct zb_registry {
  const zb_allocator *a;
  char name[32]; /* "materials", for messages */
  zb_arena arena;
  zb_registry_entry *entry;
  size_t count, cap;
  int problems; /* entries skipped so far */
} zb_registry;

void zb_registry_init(zb_registry *r, const zb_allocator *a, const char *name);
void zb_registry_free(zb_registry *r);
/* Merges the entries of one package's document (a list of tables). Returns
 * ZB_ERR_NOMEM when memory runs out; anything else wrong with the document
 * is reported and skipped. */
zb_result zb_registry_merge(zb_registry *r, const char *package, const zb_value *root);
/* An entry by key or by unambiguous id; NULL when there is none. */
const zb_registry_entry *zb_registry_find(const zb_registry *r, const char *ref);
/* The entry's index, or -1. */
int zb_registry_index(const zb_registry *r, const char *ref);
/* Drops an entry (one its consumer refused), keeping the order of the rest. */
void zb_registry_remove(zb_registry *r, size_t index);

#endif
