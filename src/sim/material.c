/* SPDX-License-Identifier: MIT */
#include "sim/material.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/log.h"

static const char *const PHASES[ZB_PHASE_COUNT] = {"solid", "powder", "liquid", "gas"};
static const char *const COLORS[ZB_MANA_COLORS] = {"fire", "storm", "light", "life", "water", "earth", "void", "decay"};
static const char *const TRANSITIONS[ZB_TRANSITIONS] = {"melts", "boils", "freezes", "condenses"};
/* The terrain's surface materials by zb_material (index 0, none, has no entry). */
static const char *const SURFACES[ZB_MAT_COUNT] = {NULL, "soil", "sand", "rock", "gravel", "snow", "ice", "mud"};

const char *zb_phase_name(zb_phase p) { return p >= 0 && p < ZB_PHASE_COUNT ? PHASES[p] : "?"; }

const char *zb_mana_color_name(zb_mana_color c) { return c >= 0 && c < ZB_MANA_COLORS ? COLORS[c] : "?"; }

/* Why an entry is refused: the field and the reason. */
typedef struct problem {
  char text[160];
} problem;

/* "<field> <why>", cut to fit. */
static bool fail(problem *p, const char *field, const char *why) {
  size_t n = 0, cap = sizeof p->text - 1;
  for (const char *c = field; *c && n < cap; ++c) {
    p->text[n++] = *c;
  }
  if (n < cap) {
    p->text[n++] = ' ';
  }
  for (const char *c = why; *c && n < cap; ++c) {
    p->text[n++] = *c;
  }
  p->text[n] = '\0';
  return false;
}

static bool number(const zb_value *t, const char *field, bool required, float lo, float hi, float def, float *out,
                   problem *p) {
  const zb_value *v = zb_table_get(t, field);
  if (!v) {
    *out = def;
    return required ? fail(p, field, "is missing") : true;
  }
  if (v->type != ZB_VAL_NUMBER || !isfinite(v->as.num)) {
    return fail(p, field, "must be a number");
  }
  if (v->as.num < lo || v->as.num > hi) {
    char why[64];
    snprintf(why, sizeof why, "must be %g .. %g", (double)lo, (double)hi);
    return fail(p, field, why);
  }
  *out = (float)v->as.num;
  return true;
}

/* A reference to another material: its index, or a problem. */
static bool reference(const zb_registry *r, const zb_value *v, const char *field, int *out, problem *p) {
  if (!zb_value_is(v, ZB_VAL_STRING)) {
    return fail(p, field, "must name a material");
  }
  *out = zb_registry_index(r, v->as.str.ptr);
  if (*out < 0) {
    char why[96];
    snprintf(why, sizeof why, "names no material (%s)", v->as.str.ptr);
    return fail(p, field, why);
  }
  return true;
}

static bool transition(const zb_registry *r, const zb_value *t, zb_transition_kind k, zb_material_info *m, problem *p) {
  m->transition[k].to = -1;
  const zb_value *v = zb_table_get(t, TRANSITIONS[k]);
  if (!v) {
    return true;
  }
  if (!zb_value_is(v, ZB_VAL_TABLE)) {
    return fail(p, TRANSITIONS[k], "must be a table { at, to, latent }");
  }
  if (!number(v, "at", true, -273.15f, 10000.0f, 0.0f, &m->transition[k].at, p) ||
      !number(v, "latent", false, 0.0f, 1e8f, 0.0f, &m->transition[k].latent, p) ||
      !reference(r, zb_table_get(v, "to"), "to", &m->transition[k].to, p)) {
    char why[160];
    snprintf(why, sizeof why, "%s", p->text);
    char field[32];
    snprintf(field, sizeof field, "%s:", TRANSITIONS[k]);
    return fail(p, field, why);
  }
  return true;
}

static bool parse_phase(const zb_value *t, zb_phase *out, problem *p) {
  const char *s = zb_value_str(zb_table_get(t, "phase"), NULL);
  for (int i = 0; s && i < ZB_PHASE_COUNT; ++i) {
    if (strcmp(s, PHASES[i]) == 0) {
      *out = (zb_phase)i;
      return true;
    }
  }
  return fail(p, "phase", "must be solid, powder, liquid or gas");
}

static bool parse_burning(const zb_registry *r, const zb_value *t, zb_material_info *m, problem *p) {
  for (int i = 0; i < ZB_MATERIAL_PRODUCTS; ++i) {
    m->product[i] = -1;
  }
  const zb_value *b = zb_table_get(t, "burns");
  if (!b) {
    return true;
  }
  if (!zb_value_is(b, ZB_VAL_TABLE)) {
    return fail(p, "burns", "must be a table");
  }
  if (!number(b, "flammability", true, 0.0f, 1.0f, 0.0f, &m->flammability, p) ||
      !number(b, "ignition", true, -273.15f, 5000.0f, 0.0f, &m->ignition, p) ||
      !number(b, "heat", true, 0.0f, 2e8f, 0.0f, &m->heat, p) ||
      !number(b, "rate", true, 0.0f, 100.0f, 0.0f, &m->burn_rate, p)) {
    char why[160];
    snprintf(why, sizeof why, "%s", p->text);
    return fail(p, "burns:", why);
  }
  const zb_value *products = zb_table_get(b, "products");
  if (products && !zb_value_is(products, ZB_VAL_TABLE)) {
    return fail(p, "burns.products", "must map materials to kg per kg burnt");
  }
  size_t n = products ? zb_table_count(products) : 0;
  if (n > ZB_MATERIAL_PRODUCTS) {
    return fail(p, "burns.products", "has more than 4 materials");
  }
  for (size_t i = 0; i < n; ++i) {
    const zb_table_entry *e = zb_table_entry_at(products, i);
    if (e->value.type != ZB_VAL_NUMBER || e->value.as.num < 0.0 || e->value.as.num > 10.0) {
      return fail(p, "burns.products", "shares must be numbers 0 .. 10");
    }
    if (!reference(r, &e->key, "burns.products", &m->product[i], p)) {
      return false;
    }
    m->product_share[i] = (float)e->value.as.num;
  }
  return true;
}

static bool parse_mana(const zb_value *t, zb_material_info *m, problem *p) {
  const zb_value *mana = zb_table_get(t, "mana");
  if (!mana) {
    return true;
  }
  if (!zb_value_is(mana, ZB_VAL_TABLE)) {
    return fail(p, "mana", "must be a table");
  }
  for (size_t i = 0; i < zb_table_count(mana); ++i) {
    const zb_table_entry *e = zb_table_entry_at(mana, i);
    const char *key = zb_value_str(&e->key, "");
    bool known = strcmp(key, "conductivity") == 0;
    for (int c = 0; c < ZB_MANA_COLORS; ++c) {
      known = known || strcmp(key, COLORS[c]) == 0;
    }
    if (!known) {
      return fail(p, "mana",
                  "takes conductivity and the eight colours (fire, storm, light, life, water, earth, "
                  "void, decay)");
    }
  }
  if (!number(mana, "conductivity", false, 0.0f, 1.0f, 0.0f, &m->mana_conductivity, p)) {
    return fail(p, "mana.conductivity", "must be 0 .. 1");
  }
  for (int c = 0; c < ZB_MANA_COLORS; ++c) {
    if (!number(mana, COLORS[c], false, 0.0f, 1.0f, 0.0f, &m->affinity[c], p)) {
      return fail(p, "mana", "affinities must be 0 .. 1");
    }
  }
  return true;
}

static bool parse_color(const zb_value *t, zb_material_info *m, problem *p) {
  const zb_value *c = zb_table_get(t, "color");
  if (!zb_value_is(c, ZB_VAL_TABLE) || zb_table_len(c) != 3) {
    return fail(p, "color", "must be { r, g, b } with 0 .. 1");
  }
  for (size_t i = 0; i < 3; ++i) {
    const zb_value *v = zb_table_index(c, i + 1);
    if (v->type != ZB_VAL_NUMBER || v->as.num < 0.0 || v->as.num > 1.0) {
      return fail(p, "color", "must be { r, g, b } with 0 .. 1");
    }
    m->color[i] = (float)v->as.num;
  }
  return true;
}

/* Which phases a transition may lead to from which. */
static bool phases_fit(zb_transition_kind k, zb_phase from, zb_phase to) {
  bool solid_from = from == ZB_PHASE_SOLID || from == ZB_PHASE_POWDER;
  bool solid_to = to == ZB_PHASE_SOLID || to == ZB_PHASE_POWDER;
  switch (k) {
    case ZB_MELTS: return solid_from && to == ZB_PHASE_LIQUID;
    case ZB_BOILS: return from == ZB_PHASE_LIQUID && to == ZB_PHASE_GAS;
    case ZB_FREEZES: return from == ZB_PHASE_LIQUID && solid_to;
    default: return from == ZB_PHASE_GAS && to == ZB_PHASE_LIQUID;
  }
}

static bool parse(const zb_registry *r, const zb_registry_entry *e, zb_material_info *m, problem *p) {
  memset(m, 0, sizeof *m);
  m->key = e->key;
  const zb_value *t = &e->value;
  m->name = zb_value_str(zb_table_get(t, "name"), NULL);
  if (!m->name) {
    return fail(p, "name", "must be a string");
  }
  if (!parse_phase(t, &m->phase, p) || !number(t, "density", true, 0.01f, 25000.0f, 0.0f, &m->density, p) ||
      !number(t, "specific_heat", true, 1.0f, 20000.0f, 0.0f, &m->specific_heat, p) ||
      !number(t, "conductivity", true, 0.0f, 500.0f, 0.0f, &m->conductivity, p) ||
      !number(t, "hardness", false, 0.0f, 10.0f, 0.0f, &m->hardness, p) ||
      !number(t, "strength", false, 0.0f, 5000.0f, 0.0f, &m->strength, p) ||
      !number(t, "friction", false, 0.0f, 1.5f, 0.6f, &m->friction, p) || !parse_color(t, m, p) ||
      !parse_mana(t, m, p) || !parse_burning(r, t, m, p)) {
    return false;
  }
  for (int k = 0; k < ZB_TRANSITIONS; ++k) {
    if (!transition(r, t, (zb_transition_kind)k, m, p)) {
      return false;
    }
  }
  return true;
}

/* Phases of transition targets are checked once every entry is parsed. */
static bool check_phases(const zb_materials *m, int i, problem *p) {
  const zb_material_info *x = &m->info[i];
  for (int k = 0; k < ZB_TRANSITIONS; ++k) {
    int to = x->transition[k].to;
    if (to >= 0 && !phases_fit((zb_transition_kind)k, x->phase, m->info[to].phase)) {
      char why[96];
      snprintf(why, sizeof why, "cannot turn a %s into a %s", PHASES[x->phase], PHASES[m->info[to].phase]);
      return fail(p, TRANSITIONS[k], why);
    }
  }
  return true;
}

zb_result zb_materials_build(zb_materials *m, zb_registry *merged, char *err, size_t err_len) {
  memset(m, 0, sizeof *m);
  m->registry = *merged;
  memset(merged, 0, sizeof *merged);
  zb_registry *r = &m->registry;
  while (r->count > ZB_MATERIALS_MAX) {
    zb_warn("%s: more than %d entries, %s left out", r->name, ZB_MATERIALS_MAX, r->entry[r->count - 1].key);
    zb_registry_remove(r, r->count - 1);
  }
  /* Parse everything; drop the first entry that fails and start again, as
   * the indices (and references to the dropped entry) change. */
  bool again = true;
  while (again) {
    again = false;
    for (size_t i = 0; i < r->count && !again; ++i) {
      problem p;
      if (!parse(r, &r->entry[i], &m->info[i], &p)) {
        zb_warn("%s: %s left out: %s", r->name, r->entry[i].key, p.text);
        zb_registry_remove(r, i);
        again = true;
      }
    }
    for (size_t i = 0; i < r->count && !again; ++i) {
      problem p;
      m->count = (int)r->count;
      if (!check_phases(m, (int)i, &p)) {
        zb_warn("%s: %s left out: %s", r->name, r->entry[i].key, p.text);
        zb_registry_remove(r, i);
        again = true;
      }
    }
  }
  m->count = (int)r->count;
  m->air = zb_materials_find(m, "air");
  m->water = zb_materials_find(m, "water");
  m->surface[0] = -1;
  const char *missing = m->air < 0 ? "air" : (m->water < 0 ? "water" : NULL);
  for (int s = 1; s < ZB_MAT_COUNT; ++s) {
    m->surface[s] = zb_materials_find(m, SURFACES[s]);
    missing = missing ? missing : (m->surface[s] < 0 ? SURFACES[s] : NULL);
  }
  if (missing) {
    snprintf(err, err_len, "%s: no usable entry for %s", r->name, missing);
    return ZB_ERR_INVALID;
  }
  return ZB_OK;
}

void zb_materials_free(zb_materials *m) {
  zb_registry_free(&m->registry);
  m->count = 0;
}

int zb_materials_find(const zb_materials *m, const char *ref) { return zb_registry_index(&m->registry, ref); }

int zb_material_at_temperature(const zb_materials *m, int material, float temperature) {
  for (int step = 0; step < 4 && material >= 0; ++step) {
    const zb_material_info *x = &m->info[material];
    int next = -1;
    for (int k = 0; k < ZB_TRANSITIONS && next < 0; ++k) {
      const zb_transition *t = &x->transition[k];
      bool heating = k == ZB_MELTS || k == ZB_BOILS;
      if (t->to >= 0 && (heating ? temperature >= t->at : temperature < t->at)) {
        next = t->to;
      }
    }
    if (next < 0) {
      break;
    }
    material = next;
  }
  return material;
}
