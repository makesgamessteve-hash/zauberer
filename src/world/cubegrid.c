/* SPDX-License-Identifier: MIT */
#include "world/cubegrid.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"

static const int DX[8] = {1, -1, 0, 0, 1, -1, 1, -1};
static const int DY[8] = {0, 0, 1, -1, 1, 1, -1, -1};

static double cell_uv(int res, double i) { return -1.0 + (i + 0.5) * 2.0 / (double)res; }

static int32_t cell_from_uv(int res, int face, double u, double v) {
  int x = (int)((u + 1.0) * 0.5 * (double)res), y = (int)((v + 1.0) * 0.5 * (double)res);
  x = x < 0 ? 0 : (x >= res ? res - 1 : x);
  y = y < 0 ? 0 : (y >= res ? res - 1 : y);
  return (int32_t)((face * res + y) * res + x);
}

/* Perimeter slot of an edge cell, -1 for interior cells. */
static int edge_slot(int res, int face, int x, int y) {
  int side, t;
  if (y == 0) {
    side = 0;
    t = x;
  } else if (y == res - 1) {
    side = 1;
    t = x;
  } else if (x == 0) {
    side = 2;
    t = y;
  } else if (x == res - 1) {
    side = 3;
    t = y;
  } else {
    return -1;
  }
  return (face * 4 + side) * res + t;
}

static int32_t compute_nbr(int res, int face, int x, int y, int k) {
  int nx = x + DX[k], ny = y + DY[k];
  bool out_x = nx < 0 || nx >= res, out_y = ny < 0 || ny >= res;
  if (!out_x && !out_y) {
    return (int32_t)((face * res + ny) * res + nx);
  }
  if (out_x && out_y) {
    return -1;
  }
  zb_dvec3 d = zb_planet_face_dir(face, cell_uv(res, (double)nx), cell_uv(res, (double)ny));
  int f2 = zb_planet_dir_face(d);
  double u, v;
  zb_planet_dir_uv(f2, d, &u, &v);
  return cell_from_uv(res, f2, u, v);
}

zb_result zb_cubegrid_init(zb_cubegrid *g, const zb_allocator *a, int res, double radius_km) {
  memset(g, 0, sizeof *g);
  if (res < 2) {
    return ZB_ERR_INVALID;
  }
  g->a = a;
  g->res = res;
  g->cells = (size_t)6 * (size_t)res * (size_t)res;
  g->area = zb_alloc(a, sizeof(float) * (size_t)res * (size_t)res);
  g->edge_nbr = zb_alloc(a, sizeof(int32_t) * (size_t)24 * (size_t)res * ZB_CUBEGRID_NBRS);
  if (!g->area || !g->edge_nbr) {
    zb_cubegrid_free(g);
    return ZB_ERR_NOMEM;
  }
  double du = 2.0 / (double)res, q = 0.78539816339744831;
  for (int y = 0; y < res; ++y) {
    double b = zb_dtan(cell_uv(res, (double)y) * q);
    for (int x = 0; x < res; ++x) {
      double c = zb_dtan(cell_uv(res, (double)x) * q);
      double s = 1.0 + c * c + b * b;
      double omega = q * q * (1.0 + c * c) * (1.0 + b * b) / (s * sqrt(s)) * du * du;
      g->area[y * res + x] = (float)(omega * radius_km * radius_km);
    }
  }
  for (int face = 0; face < 6; ++face) {
    for (int y = 0; y < res; ++y) {
      for (int x = 0; x < res; ++x) {
        int slot = edge_slot(res, face, x, y);
        if (slot < 0) {
          x = res - 2; /* skip the interior of the row */
          continue;
        }
        for (int k = 0; k < ZB_CUBEGRID_NBRS; ++k) {
          g->edge_nbr[slot * ZB_CUBEGRID_NBRS + k] = compute_nbr(res, face, x, y, k);
        }
      }
    }
  }
  return ZB_OK;
}

void zb_cubegrid_free(zb_cubegrid *g) {
  if (!g->a) {
    return;
  }
  zb_free(g->a, g->area);
  zb_free(g->a, g->edge_nbr);
  memset(g, 0, sizeof *g);
}

int32_t zb_cubegrid_nbr(const zb_cubegrid *g, int32_t cell, int k) {
  int res = g->res;
  int face = cell / (res * res), y = (cell / res) % res, x = cell % res;
  int slot = edge_slot(res, face, x, y);
  if (slot < 0) {
    return cell + DX[k] + DY[k] * res;
  }
  return g->edge_nbr[slot * ZB_CUBEGRID_NBRS + k];
}

zb_dvec3 zb_cubegrid_dir(const zb_cubegrid *g, int32_t cell) {
  int res = g->res;
  int face = cell / (res * res), y = (cell / res) % res, x = cell % res;
  return zb_planet_face_dir(face, cell_uv(res, (double)x), cell_uv(res, (double)y));
}

int32_t zb_cubegrid_cell_at(const zb_cubegrid *g, zb_dvec3 dir) {
  int face = zb_planet_dir_face(dir);
  double u, v;
  zb_planet_dir_uv(face, dir, &u, &v);
  return cell_from_uv(g->res, face, u, v);
}

double zb_cubegrid_sample(const zb_cubegrid *g, const float *field, zb_dvec3 dir) {
  int res = g->res;
  int face = zb_planet_dir_face(dir);
  double u, v;
  zb_planet_dir_uv(face, dir, &u, &v);
  double fx = (u + 1.0) * 0.5 * (double)res - 0.5, fy = (v + 1.0) * 0.5 * (double)res - 0.5;
  fx = fx < 0.0 ? 0.0 : (fx > (double)(res - 1) ? (double)(res - 1) : fx);
  fy = fy < 0.0 ? 0.0 : (fy > (double)(res - 1) ? (double)(res - 1) : fy);
  int x0 = (int)fx, y0 = (int)fy;
  int x1 = x0 + 1 < res ? x0 + 1 : x0, y1 = y0 + 1 < res ? y0 + 1 : y0;
  double tx = fx - (double)x0, ty = fy - (double)y0;
  const float *h = field + (size_t)face * (size_t)res * (size_t)res;
  double a = (double)h[y0 * res + x0] * (1.0 - tx) + (double)h[y0 * res + x1] * tx;
  double b = (double)h[y1 * res + x0] * (1.0 - tx) + (double)h[y1 * res + x1] * tx;
  return a * (1.0 - ty) + b * ty;
}
