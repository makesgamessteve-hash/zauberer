/* SPDX-License-Identifier: MIT */
/* Deep-time geology, final phase: Earth-likeness metrics, mineral deposits by
 * geological event, and the ley network along old structures. */
#include "world/geology_internal.h"

#include <string.h>

#include "world/dvec.h"
#include "world/heap.h"

#define SIN_23 0.3907
#define SIN_60 0.8660
#define LEY_MAX_NODES 64
#define LEY_MIN_SEPARATION 0.3 /* radians between nexuses */
#define LEY_NEAREST 3          /* candidate edges per node */

/* ---- Metrics ---------------------------------------------------------------------- */

/* Mean annual temperature in degrees Celsius, crude: latitude, altitude, era. */
static double present_temperature(double z, double height, double offset) {
  return 28.0 - 52.0 * z * z - 6.5 * (height > 0.0 ? height : 0.0) / 1000.0 + 8.0 * offset;
}

static double miss(double value, double lo, double hi) {
  if (value < lo) {
    return (lo - value) / (lo > 0.0 ? lo : 1.0);
  }
  if (value > hi) {
    return (value - hi) / (hi > 0.0 ? hi : 1.0);
  }
  return 0.0;
}

void zb_geo_compute_metrics(zb_geology *g, zb_geology_metrics *m) {
  const zb_lattice *l = &g->lat;
  int n = g->n;
  memset(m, 0, sizeof *m);
  int32_t *comp = g->scratch_a, *queue = g->scratch_b;
  int land = 0, ice = 0, cont_land = 0;
  for (int i = 0; i < n; ++i) {
    comp[i] = -1;
    if (g->elevation[i] > 0.0f) {
      land++;
      cont_land += g->cur.continental[i];
      double z = l->p[i].z, az = z < 0.0 ? -z : z;
      m->climate_bands |= az < SIN_23 ? 1u : (az < SIN_60 ? 2u : 4u);
      if (present_temperature(z, (double)g->elevation[i], g->temp_offset) < -8.0) {
        ice++;
      }
    }
  }
  m->land_fraction = (double)land / (double)n;
  m->ice_fraction = (double)ice / (double)n;
  m->continental_land = land > 0 ? (double)cont_land / (double)land : 0.0;
  /* Landmasses: connected land over the neighbour graph. */
  int big = n / 100, components = 0;
  for (int s = 0; s < n; ++s) {
    if (comp[s] >= 0 || g->elevation[s] <= 0.0f) {
      continue;
    }
    int head = 0, tail = 0, size = 0;
    bool mountains = false;
    queue[tail++] = s;
    comp[s] = components;
    while (head < tail) {
      int i = queue[head++];
      size++;
      if ((g->cur.flags[i] & (ZB_GEO_COLLISION | ZB_GEO_ARC)) && g->elevation[i] > 1500.0f) {
        mountains = true;
      }
      for (int k = 0; k < 6; ++k) {
        int j = zb_lattice_nbr(l, i, k);
        if (j >= 0 && comp[j] < 0 && g->elevation[j] > 0.0f) {
          comp[j] = components;
          queue[tail++] = j;
        }
      }
    }
    components++;
    double share = (double)size / (double)n;
    if (share > m->largest_landmass) {
      m->largest_landmass = share;
    }
    if (size > big) {
      m->continents++;
      if (size > 2 * big) {
        m->large_continents++;
        m->continents_with_mountains += mountains;
      }
    } else {
      m->islands++;
    }
  }
  for (int i = 0; i < n; ++i) {
    float o = g->cur.orogeny[i];
    if (g->elevation[i] > 1500.0f && o >= 0.0f && o < 60.0f) {
      m->young_ranges = true;
    }
    if (g->elevation[i] > 300.0f && o > 200.0f) {
      m->old_ranges = true;
    }
  }
  /* Scores per target: 0 when met. */
  double score = 0.0;
  switch (g->opt.target) {
    case ZB_GEO_TARGET_EARTH:
      score += miss(m->land_fraction, 0.25, 0.40);
      score += miss((double)m->continents, 3.0, 8.0);
      score += m->large_continents > 0
                   ? (double)(m->large_continents - m->continents_with_mountains) / (double)m->large_continents
                   : 1.0;
      score += m->young_ranges ? 0.0 : 0.5;
      score += m->old_ranges ? 0.0 : 0.5;
      score += miss(m->continental_land, 0.85, 1.0);
      score += miss(m->ice_fraction, 0.0, 0.15);
      score += m->climate_bands == 7u ? 0.0 : 0.5;
      break;
    case ZB_GEO_TARGET_ICE_WORLD:
    case ZB_GEO_TARGET_HOTHOUSE:
      score += miss((double)m->continents, 2.0, 10.0);
      score += m->continents_with_mountains > 0 ? 0.0 : 0.5;
      score += miss(m->continental_land, 0.85, 1.0);
      if (g->opt.target == ZB_GEO_TARGET_ICE_WORLD) {
        score += miss(m->ice_fraction, 0.15, 0.6);
      } else {
        score += miss(m->ice_fraction, 0.0, 0.02);
        score += (m->climate_bands & 3u) == 3u ? 0.0 : 0.5;
      }
      break;
    case ZB_GEO_TARGET_PANGAEA:
      score += miss((double)m->continents, 1.0, 2.0);
      score += miss(m->largest_landmass, 0.7 * m->land_fraction, 1.0);
      score += m->continents_with_mountains > 0 ? 0.0 : 0.5;
      break;
    case ZB_GEO_TARGET_ARCHIPELAGO:
    default:
      score += miss(m->largest_landmass, 0.0, 0.05);
      score += miss((double)(m->continents + m->islands), 10.0, 1e9);
      break;
  }
  m->score = score;
  m->passed = score <= 0.0;
}

/* ---- Minerals ------------------------------------------------------------------------ */

static bool column_has(const zb_stratum *col, zb_rock rock) {
  for (int k = 0; k < ZB_STRATA; ++k) {
    if (col[k].thickness > 0 && col[k].rock == rock) {
      return true;
    }
  }
  return false;
}

static bool column_has_marine_shale(const zb_stratum *col) {
  for (int k = 0; k < ZB_STRATA; ++k) {
    if (col[k].thickness > 0 && col[k].rock == ZB_ROCK_SHALE &&
        (col[k].env == ZB_ENV_SHALLOW_SEA || col[k].env == ZB_ENV_DEEP_SEA)) {
      return true;
    }
  }
  return false;
}

/* Points within `rings` neighbour steps of a ley nexus get marked in mark[]. */
static void mark_near_nodes(const zb_geology *g, uint8_t *mark, int rings) {
  int32_t *queue = g->scratch_a, *dist = g->scratch_b;
  int head = 0, tail = 0;
  for (int i = 0; i < g->n; ++i) {
    dist[i] = -1;
    mark[i] = 0;
  }
  for (int k = 0; k < g->ley_node_count; ++k) {
    int p = g->ley_nodes[k].point;
    if (dist[p] < 0) {
      dist[p] = 0;
      queue[tail++] = p;
    }
  }
  while (head < tail) {
    int i = queue[head++];
    mark[i] = 1;
    if (dist[i] >= rings) {
      continue;
    }
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(&g->lat, i, k);
      if (j >= 0 && dist[j] < 0) {
        dist[j] = dist[i] + 1;
        queue[tail++] = j;
      }
    }
  }
}

void zb_geo_compute_minerals(zb_geology *g) {
  uint8_t *near_node = zb_alloc(g->a, (size_t)g->n);
  if (near_node) {
    mark_near_nodes(g, near_node, 3);
  }
  for (int i = 0; i < g->n; ++i) {
    const zb_stratum *col = &g->cur.strata[(size_t)i * ZB_STRATA];
    uint16_t f = g->cur.flags[i];
    uint64_t h = zb_rng_at(g->seed, 0x6d696e6572616cull, (uint64_t)i);
    double r1 = (double)(h & 0xffff) / 65536.0, r2 = (double)((h >> 16) & 0xffff) / 65536.0;
    double r3 = (double)((h >> 32) & 0xffff) / 65536.0;
    double height = (double)g->elevation[i];
    uint32_t m = 0;
    if (f & ZB_GEO_ARC) {
      m |= ZB_MIN_COPPER;
      m |= r1 < 0.5 ? ZB_MIN_GOLD : 0u;
      m |= r2 < 0.35 ? ZB_MIN_SILVER : 0u;
    }
    bool granite_belt =
        g->cur.continental[i] && (g->cur.basement[i] == ZB_ROCK_GRANITE || g->cur.basement[i] == ZB_ROCK_GNEISS);
    if ((f & ZB_GEO_COLLISION) && granite_belt) {
      m |= r3 < 0.5 ? ZB_MIN_TIN : 0u;
      m |= r1 < 0.15 ? ZB_MIN_LAPIS : 0u;
    }
    if ((f & ZB_GEO_CRATON) && (double)g->cur.born[i] > g->opt.span_myr + 1500.0) {
      m |= ZB_MIN_IRON;
    }
    if (column_has(col, ZB_ROCK_SALT)) {
      m |= ZB_MIN_SALT;
    }
    if (column_has(col, ZB_ROCK_GYPSUM)) {
      m |= ZB_MIN_GYPSUM;
    }
    if (column_has(col, ZB_ROCK_COAL)) {
      m |= ZB_MIN_COAL;
    }
    if (zb_geo_sediment_thickness(col) > 2000.0 && column_has_marine_shale(col)) {
      m |= ZB_MIN_NAPHTHA;
      m |= height > 200.0 ? ZB_MIN_BITUMEN : 0u;
    }
    if ((m & ZB_MIN_COPPER) && (column_has(col, ZB_ROCK_RED_SANDSTONE) || column_has(col, ZB_ROCK_SALT))) {
      m |= ZB_MIN_TURQUOISE; /* weathered copper under a dry sky */
    }
    if (f & ZB_GEO_ACTIVE_VOLCANO) {
      m |= ZB_MIN_SULFUR;
    }
    if ((f & ZB_GEO_FLOOD_BASALT) && r2 < 0.4) {
      m |= ZB_MIN_CARNELIAN;
    }
    if (col[0].thickness > 0 && col[0].rock == ZB_ROCK_LIMESTONE && height > 0.0) {
      m |= ZB_MIN_LIMESTONE;
    }
    if (col[0].thickness > 0 && col[0].env == ZB_ENV_RIVER && height > 0.0 && height < 300.0) {
      m |= ZB_MIN_CLAY;
    }
    if ((f & ZB_GEO_SUTURE) && ((near_node && near_node[i]) || r3 < 0.2)) {
      m |= ZB_MIN_MANA_CRYSTAL;
    }
    g->minerals[i] = m;
  }
  zb_free(g->a, near_node);
}

/* ---- Ley network ------------------------------------------------------------------------ */

typedef struct ley_candidate {
  double score;
  int32_t point;
  uint8_t kind;
} ley_candidate;

static double structure_cost(uint16_t f) {
  if (f & (ZB_GEO_SUTURE | ZB_GEO_RIFT | ZB_GEO_HOTSPOT)) {
    return 0.4;
  }
  if (f & (ZB_GEO_COLLISION | ZB_GEO_ARC | ZB_GEO_BOUNDARY)) {
    return 0.7;
  }
  return 1.0;
}

/* Shortest structure-weighted paths from one point; fills cost (and parent if given). */
static bool dijkstra(const zb_geology *g, int source, double *cost, int32_t *parent) {
  const zb_lattice *l = &g->lat;
  for (int i = 0; i < g->n; ++i) {
    cost[i] = 1e300;
    if (parent) {
      parent[i] = -1;
    }
  }
  zb_heap h;
  zb_heap_init(&h, g->a);
  if (!zb_heap_reserve(&h, (size_t)g->n)) {
    return false;
  }
  cost[source] = 0.0;
  zb_heap_push(&h, 0.0, source, 0);
  zb_heap_item it;
  while (zb_heap_pop(&h, &it)) {
    if (it.key > cost[it.id]) {
      continue;
    }
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, it.id, k);
      if (j < 0) {
        continue;
      }
      double c = it.key + zb_dv_angle(l->p[it.id], l->p[j]) * structure_cost(g->cur.flags[j]);
      if (c < cost[j]) {
        cost[j] = c;
        if (parent) {
          parent[j] = it.id;
        }
        zb_heap_push(&h, c, j, 0);
      }
    }
  }
  bool ok = !h.failed;
  zb_heap_free(&h);
  return ok;
}

static int find_root(int *uf, int x) {
  while (uf[x] != x) {
    uf[x] = uf[uf[x]];
    x = uf[x];
  }
  return x;
}

typedef struct ley_pair {
  double cost;
  int a, b;
  bool chosen, loop;
} ley_pair;

static int pair_cmp(const ley_pair *x, const ley_pair *y) {
  if (x->cost != y->cost) {
    return x->cost < y->cost ? -1 : 1;
  }
  if (x->a != y->a) {
    return x->a < y->a ? -1 : 1;
  }
  return x->b < y->b ? -1 : (x->b > y->b ? 1 : 0);
}

static void sort_pairs(ley_pair *p, int count) { /* insertion sort: at most a few hundred pairs */
  for (int i = 1; i < count; ++i) {
    ley_pair t = p[i];
    int j = i - 1;
    while (j >= 0 && pair_cmp(&p[j], &t) > 0) {
      p[j + 1] = p[j];
      --j;
    }
    p[j + 1] = t;
  }
}

static void select_nodes(zb_geology *g, ley_candidate *cand, int count) {
  /* Highest score first (ties by point), then keep them apart. */
  for (int i = 1; i < count; ++i) {
    ley_candidate t = cand[i];
    int j = i - 1;
    while (j >= 0 && (cand[j].score < t.score || (cand[j].score == t.score && cand[j].point > t.point))) {
      cand[j + 1] = cand[j];
      --j;
    }
    cand[j + 1] = t;
  }
  for (int c = 0; c < count && g->ley_node_count < LEY_MAX_NODES; ++c) {
    zb_dvec3 p = g->lat.p[cand[c].point];
    bool far = true;
    for (int k = 0; k < g->ley_node_count && far; ++k) {
      far = zb_dv_angle(p, g->lat.p[g->ley_nodes[k].point]) > LEY_MIN_SEPARATION;
    }
    if (far) {
      zb_ley_node *nd = &g->ley_nodes[g->ley_node_count++];
      nd->point = cand[c].point;
      nd->kind = cand[c].kind;
      nd->strength = (float)(cand[c].score > 1.0 ? 1.0 : cand[c].score);
    }
  }
}

static int gather_candidates(zb_geology *g, ley_candidate *cand, int cap) {
  const zb_lattice *l = &g->lat;
  int count = 0;
  for (int h = 0; h < g->hotspot_count && count < cap; ++h) {
    if (g->hotspots[h].end < 0.0f && (double)g->hotspots[h].start > 0.0) {
      cand[count].point = zb_lattice_nearest(l, g->hotspots[h].dir, -1);
      cand[count].kind = ZB_LEY_HOTSPOT;
      cand[count].score = 0.8;
      count++;
    }
  }
  for (int i = 0; i < g->n && count < cap; ++i) {
    uint8_t plates[ZB_LATTICE_K + 1];
    int distinct = 0, sutures = 0;
    for (int k = -1; k < ZB_LATTICE_K; ++k) {
      int j = k < 0 ? i : zb_lattice_nbr(l, i, k);
      if (j < 0) {
        continue;
      }
      sutures += (g->cur.flags[j] & ZB_GEO_SUTURE) != 0;
      bool seen = false;
      for (int d = 0; d < distinct && !seen; ++d) {
        seen = plates[d] == g->cur.plate[j];
      }
      if (!seen) {
        plates[distinct++] = g->cur.plate[j];
      }
    }
    double score = 0.0;
    uint8_t kind = ZB_LEY_TRIPLE_JUNCTION;
    if (distinct >= 3) {
      score = 1.0;
    } else if (g->cur.flags[i] & ZB_GEO_ACTIVE_VOLCANO) {
      score = 0.7;
      kind = ZB_LEY_VOLCANO;
    } else if ((g->cur.flags[i] & ZB_GEO_SUTURE) && sutures >= 4) {
      /* Old sutures are stronger: the network has had longer to settle along them. */
      double age = (double)g->cur.orogeny[i];
      score = 0.5 + 0.3 * (age > 0.0 ? (age > g->opt.span_myr ? 1.0 : age / g->opt.span_myr) : 0.0);
      kind = ZB_LEY_SUTURE;
    }
    if (score > 0.0) {
      cand[count].point = i;
      cand[count].kind = kind;
      cand[count].score = score;
      count++;
    }
  }
  return count;
}

void zb_geo_build_ley(zb_geology *g) {
  zb_free(g->a, g->ley_nodes);
  zb_free(g->a, g->ley_edges);
  zb_free(g->a, g->ley_path);
  g->ley_nodes = NULL;
  g->ley_edges = NULL;
  g->ley_path = NULL;
  g->ley_node_count = g->ley_edge_count = g->ley_path_len = 0;
  int cap = g->n;
  ley_candidate *cand = zb_alloc(g->a, sizeof(ley_candidate) * (size_t)cap);
  g->ley_nodes = zb_alloc(g->a, sizeof(zb_ley_node) * LEY_MAX_NODES);
  double *cost = zb_alloc(g->a, sizeof(double) * (size_t)g->n);
  int32_t *parent = zb_alloc(g->a, sizeof(int32_t) * (size_t)g->n);
  ley_pair *pairs = zb_alloc(g->a, sizeof(ley_pair) * LEY_MAX_NODES * LEY_NEAREST);
  if (!cand || !g->ley_nodes || !cost || !parent || !pairs) {
    goto done;
  }
  select_nodes(g, cand, gather_candidates(g, cand, cap));
  int nodes = g->ley_node_count, npairs = 0;
  /* Candidate edges: each node to its nearest few. */
  for (int a = 0; a < nodes; ++a) {
    int best[LEY_NEAREST];
    double best_d[LEY_NEAREST];
    for (int k = 0; k < LEY_NEAREST; ++k) {
      best[k] = -1;
      best_d[k] = 1e300;
    }
    for (int b = 0; b < nodes; ++b) {
      if (b == a) {
        continue;
      }
      double d = zb_dv_angle(g->lat.p[g->ley_nodes[a].point], g->lat.p[g->ley_nodes[b].point]);
      for (int k = 0; k < LEY_NEAREST; ++k) {
        if (d < best_d[k]) {
          for (int m = LEY_NEAREST - 1; m > k; --m) {
            best[m] = best[m - 1];
            best_d[m] = best_d[m - 1];
          }
          best[k] = b;
          best_d[k] = d;
          break;
        }
      }
    }
    for (int k = 0; k < LEY_NEAREST; ++k) {
      if (best[k] < 0) {
        continue;
      }
      int lo = a < best[k] ? a : best[k], hi = a < best[k] ? best[k] : a;
      bool dup = false;
      for (int p = 0; p < npairs && !dup; ++p) {
        dup = pairs[p].a == lo && pairs[p].b == hi;
      }
      if (!dup) {
        pairs[npairs].a = lo;
        pairs[npairs].b = hi;
        pairs[npairs].chosen = pairs[npairs].loop = false;
        npairs++;
      }
    }
  }
  /* Path costs along the structures. */
  for (int a = 0; a < nodes; ++a) {
    bool needed = false;
    for (int p = 0; p < npairs && !needed; ++p) {
      needed = pairs[p].a == a;
    }
    if (!needed) {
      continue;
    }
    if (!dijkstra(g, g->ley_nodes[a].point, cost, NULL)) {
      goto done;
    }
    for (int p = 0; p < npairs; ++p) {
      if (pairs[p].a == a) {
        pairs[p].cost = cost[g->ley_nodes[pairs[p].b].point];
      }
    }
  }
  sort_pairs(pairs, npairs);
  /* A spanning tree, then the cheapest third of the rest as loops. */
  int uf[LEY_MAX_NODES];
  for (int k = 0; k < nodes; ++k) {
    uf[k] = k;
  }
  int extra = 0, rest = 0;
  for (int p = 0; p < npairs; ++p) {
    int ra = find_root(uf, pairs[p].a), rb = find_root(uf, pairs[p].b);
    if (ra != rb) {
      uf[ra] = rb;
      pairs[p].chosen = true;
    } else {
      rest++;
    }
  }
  int loops = (rest + 2) / 3;
  for (int p = 0; p < npairs && extra < loops; ++p) {
    if (!pairs[p].chosen) {
      pairs[p].chosen = pairs[p].loop = true;
      extra++;
    }
  }
  int chosen = 0;
  for (int p = 0; p < npairs; ++p) {
    chosen += pairs[p].chosen;
  }
  g->ley_edges = zb_alloc(g->a, sizeof(zb_ley_edge) * (size_t)(chosen > 0 ? chosen : 1));
  int path_cap = 1024;
  g->ley_path = zb_alloc(g->a, sizeof(int32_t) * (size_t)path_cap);
  if (!g->ley_edges || !g->ley_path) {
    goto done;
  }
  for (int a = 0; a < nodes; ++a) {
    bool ran = false;
    for (int p = 0; p < npairs; ++p) {
      if (!pairs[p].chosen || pairs[p].a != a) {
        continue;
      }
      if (!ran && !dijkstra(g, g->ley_nodes[a].point, cost, parent)) {
        goto done;
      }
      ran = true;
      /* Walk back from b to a, then store a..b. */
      int len = 0;
      for (int q = g->ley_nodes[pairs[p].b].point; q >= 0; q = parent[q]) {
        len++;
      }
      while (g->ley_path_len + len > path_cap) {
        int32_t *grown = zb_realloc(g->a, g->ley_path, sizeof(int32_t) * (size_t)path_cap * 2);
        if (!grown) {
          goto done;
        }
        g->ley_path = grown;
        path_cap *= 2;
      }
      int at = g->ley_path_len + len - 1;
      for (int q = g->ley_nodes[pairs[p].b].point; q >= 0; q = parent[q]) {
        g->ley_path[at--] = q;
      }
      zb_ley_edge *e = &g->ley_edges[g->ley_edge_count++];
      e->a = pairs[p].a;
      e->b = pairs[p].b;
      e->path = g->ley_path_len;
      e->length = len;
      e->loop = pairs[p].loop;
      g->ley_path_len += len;
    }
  }
done:
  zb_free(g->a, cand);
  zb_free(g->a, cost);
  zb_free(g->a, parent);
  zb_free(g->a, pairs);
}
