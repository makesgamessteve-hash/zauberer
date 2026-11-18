/* SPDX-License-Identifier: MIT */
#include "core/registry.h"

#include <stdio.h>
#include <string.h>

#include "core/log.h"

void zb_registry_init(zb_registry *r, const zb_allocator *a, const char *name) {
  memset(r, 0, sizeof *r);
  r->a = a;
  snprintf(r->name, sizeof r->name, "%s", name);
  zb_arena_init(&r->arena, a, 16384);
}

void zb_registry_free(zb_registry *r) {
  zb_free(r->a, r->entry);
  zb_arena_destroy(&r->arena);
  memset(r, 0, sizeof *r);
}

static bool key_is(const zb_table_entry *e, const char *key) {
  return e->key.type == ZB_VAL_STRING && strlen(key) == e->key.as.str.len &&
         memcmp(e->key.as.str.ptr, key, e->key.as.str.len) == 0;
}

/* Deep copy of v into the arena, leaving out the field named skip. */
static bool copy_value(zb_arena *arena, const zb_value *v, const char *skip, zb_value *out) {
  *out = *v;
  if (v->type == ZB_VAL_STRING) {
    out->as.str.ptr = zb_arena_strndup(arena, v->as.str.ptr, v->as.str.len);
    return out->as.str.ptr != NULL;
  }
  if (v->type != ZB_VAL_TABLE) {
    return true;
  }
  const zb_table *t = v->as.table;
  zb_table *c = zb_arena_alloc(arena, sizeof *c, _Alignof(zb_table));
  zb_table_entry *e = t->count ? zb_arena_alloc(arena, sizeof *e * t->count, _Alignof(zb_table_entry)) : NULL;
  if (!c || (t->count && !e)) {
    return false;
  }
  size_t n = 0;
  for (size_t i = 0; i < t->count; ++i) {
    if (skip && key_is(&t->entries[i], skip)) {
      continue;
    }
    if (!copy_value(arena, &t->entries[i].key, NULL, &e[n].key) ||
        !copy_value(arena, &t->entries[i].value, NULL, &e[n].value)) {
      return false;
    }
    n++;
  }
  c->entries = e;
  c->count = n;
  out->as.table = c;
  return true;
}

/* The fields of base with those of patch replacing or joining them. */
static bool patch_value(zb_arena *arena, const zb_value *base, const zb_value *patch, zb_value *out) {
  const zb_table *b = base->as.table, *p = patch->as.table;
  zb_table *c = zb_arena_alloc(arena, sizeof *c, _Alignof(zb_table));
  zb_table_entry *e = zb_arena_alloc(arena, sizeof *e * (b->count + p->count), _Alignof(zb_table_entry));
  if (!c || !e) {
    return false;
  }
  size_t n = b->count;
  memcpy(e, b->entries, sizeof *e * n);
  for (size_t i = 0; i < p->count; ++i) {
    const zb_table_entry *pe = &p->entries[i];
    if (pe->key.type != ZB_VAL_STRING || key_is(pe, "patch")) {
      continue;
    }
    size_t k = 0;
    while (k < n && !(e[k].key.type == ZB_VAL_STRING && e[k].key.as.str.len == pe->key.as.str.len &&
                      memcmp(e[k].key.as.str.ptr, pe->key.as.str.ptr, pe->key.as.str.len) == 0)) {
      k++;
    }
    if (!copy_value(arena, &pe->key, NULL, &e[k].key) || !copy_value(arena, &pe->value, NULL, &e[k].value)) {
      return false;
    }
    n += k == n ? 1 : 0;
  }
  c->entries = e;
  c->count = n;
  out->type = ZB_VAL_TABLE;
  out->as.table = c;
  return true;
}

static bool valid_id(const char *s, size_t len) {
  if (len == 0) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    char ch = s[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) {
      return false;
    }
  }
  return true;
}

const zb_registry_entry *zb_registry_find(const zb_registry *r, const char *ref) {
  int i = zb_registry_index(r, ref);
  return i < 0 ? NULL : &r->entry[i];
}

int zb_registry_index(const zb_registry *r, const char *ref) {
  bool full = strchr(ref, ':') != NULL;
  int found = -1;
  for (size_t i = 0; i < r->count; ++i) {
    if (strcmp(full ? r->entry[i].key : r->entry[i].id, ref) == 0) {
      if (full || found >= 0) {
        return full ? (int)i : -1; /* an id two packages define is ambiguous */
      }
      found = (int)i;
    }
  }
  return found;
}

void zb_registry_remove(zb_registry *r, size_t index) {
  memmove(&r->entry[index], &r->entry[index + 1], sizeof *r->entry * (r->count - index - 1));
  r->count--;
}

static void skip(zb_registry *r, const char *package, size_t n, const char *why, const char *what) {
  zb_warn("%s of %s, entry %zu: %s%s", r->name, package, n, why, what);
  r->problems++;
}

static zb_result add(zb_registry *r, const char *package, size_t n, const zb_value *e, const zb_value *id) {
  if (!valid_id(id->as.str.ptr, id->as.str.len) || id->as.str.len + strlen(package) + 1 >= ZB_REGISTRY_KEY_MAX) {
    skip(r, package, n, "ids use a-z, 0-9 and _ and are short: ", id->as.str.ptr);
    return ZB_OK;
  }
  char key[ZB_REGISTRY_KEY_MAX];
  snprintf(key, sizeof key, "%s:%.*s", package, (int)id->as.str.len, id->as.str.ptr);
  if (zb_registry_index(r, key) >= 0) {
    skip(r, package, n, "defined twice: ", key);
    return ZB_OK;
  }
  if (r->count == r->cap) {
    size_t cap = r->cap ? 2 * r->cap : 32;
    zb_registry_entry *grown = zb_realloc(r->a, r->entry, sizeof *grown * cap);
    if (!grown) {
      return ZB_ERR_NOMEM;
    }
    r->entry = grown;
    r->cap = cap;
  }
  zb_registry_entry *re = &r->entry[r->count];
  memset(re, 0, sizeof *re);
  re->key = zb_arena_strndup(&r->arena, key, strlen(key));
  if (!re->key || !copy_value(&r->arena, e, "id", &re->value)) {
    return ZB_ERR_NOMEM;
  }
  re->id = strchr(re->key, ':') + 1;
  re->package = zb_arena_strndup(&r->arena, package, strlen(package));
  if (!re->package) {
    return ZB_ERR_NOMEM;
  }
  r->count++;
  return ZB_OK;
}

static zb_result change(zb_registry *r, const char *package, size_t n, const zb_value *e, const char *field,
                        const char *ref) {
  int i = zb_registry_index(r, ref);
  if (i < 0) {
    char why[96];
    snprintf(why, sizeof why, "cannot %s an entry there is not (or two packages define): ", field);
    skip(r, package, n, why, ref);
    return ZB_OK;
  }
  zb_registry_entry *re = &r->entry[i];
  bool patch = strcmp(field, "patch") == 0;
  zb_value v;
  if (!(patch ? patch_value(&r->arena, &re->value, e, &v) : copy_value(&r->arena, e, field, &v))) {
    return ZB_ERR_NOMEM;
  }
  re->value = v;
  re->changed = zb_arena_strndup(&r->arena, package, strlen(package));
  if (!re->changed) {
    return ZB_ERR_NOMEM;
  }
  if (patch) {
    char fields[160] = "";
    size_t used = 0;
    for (size_t k = 0; k < e->as.table->count; ++k) {
      const zb_table_entry *pe = &e->as.table->entries[k];
      if (pe->key.type == ZB_VAL_STRING && !key_is(pe, "patch") && used < sizeof fields) {
        used += (size_t)snprintf(fields + used, sizeof fields - used, "%s%.*s", used ? ", " : "",
                                 (int)pe->key.as.str.len, pe->key.as.str.ptr);
      }
    }
    zb_info("%s: %s patched by %s (%s)", r->name, re->key, package, fields);
  } else {
    zb_info("%s: %s overridden by %s", r->name, re->key, package);
  }
  return ZB_OK;
}

zb_result zb_registry_merge(zb_registry *r, const char *package, const zb_value *root) {
  if (!zb_value_is(root, ZB_VAL_TABLE)) {
    skip(r, package, 0, "the data is not a list of entries", "");
    return ZB_OK;
  }
  size_t len = zb_table_len(root);
  for (size_t n = 1; n <= len; ++n) {
    const zb_value *e = zb_table_index(root, n);
    if (!zb_value_is(e, ZB_VAL_TABLE)) {
      skip(r, package, n, "not a table", "");
      continue;
    }
    const zb_value *id = zb_table_get(e, "id"), *over = zb_table_get(e, "override"), *patch = zb_table_get(e, "patch");
    int given = (id != NULL) + (over != NULL) + (patch != NULL);
    const zb_value *which = id ? id : (over ? over : patch);
    if (given != 1 || which->type != ZB_VAL_STRING) {
      skip(r, package, n, "needs exactly one of id, override or patch (a string)", "");
      continue;
    }
    zb_result res =
        id ? add(r, package, n, e, id) : change(r, package, n, e, over ? "override" : "patch", which->as.str.ptr);
    if (res != ZB_OK) {
      return res;
    }
  }
  return ZB_OK;
}
