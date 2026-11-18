/* SPDX-License-Identifier: MIT */
#include "input/gesture.h"

#include "core/dmath.h"

#include <math.h>
#include <string.h>

#define R 64                  /* points a stroke is resampled to */
#define SHIFTS 8              /* starting points tried for closed forms */
#define STRICT (ZB_DPI / 3.0) /* angular distance that scores 0 */
#define LENIENT (ZB_DPI / 2.0)

static const char *const NAMES[ZB_FORMS] = {"tap", "line", "circle", "spiral", "triangle", "square", "zigzag", "wave"};

const char *zb_form_name(zb_form f) { return f >= 0 && f < ZB_FORMS ? NAMES[f] : "none"; }

zb_form zb_form_by_name(const char *name) {
  for (int k = 0; name && k < ZB_FORMS; ++k) {
    if (!strcmp(name, NAMES[k])) {
      return (zb_form)k;
    }
  }
  return ZB_FORM_NONE;
}

/* ---- Templates ---- */

/* A point at arc-length fraction t along a polyline of n vertices. */
static void along(const double (*v)[2], int n, double t, double *x, double *y) {
  double total = 0.0;
  for (int k = 1; k < n; ++k) {
    total +=
        sqrt((v[k][0] - v[k - 1][0]) * (v[k][0] - v[k - 1][0]) + (v[k][1] - v[k - 1][1]) * (v[k][1] - v[k - 1][1]));
  }
  double want = t * total, run = 0.0;
  for (int k = 1; k < n; ++k) {
    double dx = v[k][0] - v[k - 1][0], dy = v[k][1] - v[k - 1][1], len = sqrt(dx * dx + dy * dy);
    if (run + len >= want || k == n - 1) {
      double f = len > 0.0 ? (want - run) / len : 0.0;
      f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
      *x = v[k - 1][0] + dx * f;
      *y = v[k - 1][1] + dy * f;
      return;
    }
    run += len;
  }
  *x = v[0][0];
  *y = v[0][1];
}

static void template_point(zb_form f, double t, double *x, double *y) {
  static const double tri[4][2] = {{0.0, 1.0}, {-0.866, -0.5}, {0.866, -0.5}, {0.0, 1.0}};
  static const double sq[5][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
  static const double zz[5][2] = {{0, 0}, {0.5, 1}, {1, 0}, {1.5, 1}, {2, 0}};
  switch (f) {
    case ZB_FORM_LINE:
      *x = t;
      *y = 0.0;
      break;
    case ZB_FORM_CIRCLE:
      *x = zb_dcos(2.0 * ZB_DPI * t);
      *y = zb_dsin(2.0 * ZB_DPI * t);
      break;
    case ZB_FORM_SPIRAL: {
      double r = 0.15 + 0.85 * t;
      *x = r * zb_dcos(4.0 * ZB_DPI * t);
      *y = r * zb_dsin(4.0 * ZB_DPI * t);
      break;
    }
    case ZB_FORM_TRIANGLE: along(tri, 4, t, x, y); break;
    case ZB_FORM_SQUARE: along(sq, 5, t, x, y); break;
    case ZB_FORM_ZIGZAG: along(zz, 5, t, x, y); break;
    case ZB_FORM_WAVE:
      *x = 2.0 * t;
      *y = 0.3 * zb_dsin(4.0 * ZB_DPI * t);
      break;
    default: *x = *y = 0.0; break;
  }
}

void zb_gesture_template(zb_form f, float *xy, int count) {
  for (int k = 0; k < count; ++k) {
    double x, y;
    template_point(f, count > 1 ? (double)k / (count - 1) : 0.0, &x, &y);
    xy[2 * k] = (float)x;
    xy[2 * k + 1] = (float)y;
  }
}

/* ---- Normalised strokes ---- */

typedef struct shape {
  double x[R], y[R];
} shape;

/* Centred and scaled to unit length as one vector. */
static bool normalise(shape *s) {
  double cx = 0.0, cy = 0.0;
  for (int k = 0; k < R; ++k) {
    cx += s->x[k];
    cy += s->y[k];
  }
  cx /= R;
  cy /= R;
  double len = 0.0;
  for (int k = 0; k < R; ++k) {
    s->x[k] -= cx;
    s->y[k] -= cy;
    len += s->x[k] * s->x[k] + s->y[k] * s->y[k];
  }
  len = sqrt(len);
  if (len < 1e-12) {
    return false;
  }
  for (int k = 0; k < R; ++k) {
    s->x[k] /= len;
    s->y[k] /= len;
  }
  return true;
}

/* The stroke resampled to R points evenly along its length. */
static double resample(const float *xy, int n, shape *s) {
  double total = 0.0;
  for (int k = 1; k < n; ++k) {
    double dx = xy[2 * k] - xy[2 * k - 2], dy = xy[2 * k + 1] - xy[2 * k - 1];
    total += sqrt(dx * dx + dy * dy);
  }
  if (total <= 0.0) {
    return 0.0;
  }
  double step = total / (R - 1), run = 0.0, want = 0.0;
  int out = 0;
  for (int k = 1; k < n && out < R; ++k) {
    double x0 = xy[2 * k - 2], y0 = xy[2 * k - 1], dx = xy[2 * k] - x0, dy = xy[2 * k + 1] - y0;
    double len = sqrt(dx * dx + dy * dy);
    while (out < R && want <= run + len + 1e-9) {
      double f = len > 0.0 ? (want - run) / len : 0.0;
      s->x[out] = x0 + dx * f;
      s->y[out] = y0 + dy * f;
      out++;
      want = step * out;
    }
    run += len;
  }
  for (; out < R; ++out) { /* rounding at the end */
    s->x[out] = xy[2 * n - 2];
    s->y[out] = xy[2 * n - 1];
  }
  return total;
}

/* Cosine similarity at the best rotation (Protractor), 0 .. 1. */
static double similarity(const shape *a, const shape *b) {
  double p = 0.0, q = 0.0;
  for (int k = 0; k < R; ++k) {
    p += a->x[k] * b->x[k] + a->y[k] * b->y[k];
    q += a->x[k] * b->y[k] - a->y[k] * b->x[k];
  }
  double s = sqrt(p * p + q * q);
  return s > 1.0 ? 1.0 : s;
}

/* A form's template resampled along its length like a stroke (cached). */
static const shape *template_shape(zb_form f) {
  static shape cache[ZB_FORMS];
  static bool made[ZB_FORMS];
  if (!made[f]) {
    static float dense[2 * 512];
    zb_gesture_template(f, dense, 512);
    resample(dense, 512, &cache[f]);
    made[f] = true;
  }
  return &cache[f];
}

/* The best match of a stroke with a form: either way round, mirrored, and
 * for closed forms from several starting points. */
static double best_match(const shape *in, zb_form f) {
  bool closed = f == ZB_FORM_CIRCLE || f == ZB_FORM_TRIANGLE || f == ZB_FORM_SQUARE;
  const shape *base = template_shape(f);
  double best = 0.0;
  for (int shift = 0; shift < (closed ? SHIFTS : 1); ++shift) {
    for (int variant = 0; variant < 4; ++variant) {
      shape t;
      for (int k = 0; k < R; ++k) {
        int idx = variant & 1 ? R - 1 - k : k;
        if (closed) { /* the first and last points are the same: cycle over R - 1 */
          idx = (idx + shift * (R - 1) / SHIFTS) % (R - 1);
        }
        t.x[k] = base->x[idx];
        t.y[k] = variant & 2 ? -base->y[idx] : base->y[idx];
      }
      if (normalise(&t)) {
        double sim = similarity(in, &t);
        best = sim > best ? sim : best;
      }
    }
  }
  return best;
}

void zb_gesture_recognize(const float *xy, int n, const zb_gesture_options *opt, zb_gesture *out) {
  memset(out, 0, sizeof *out);
  out->form = out->second = ZB_FORM_NONE;
  if (!xy || n < 1 || n > ZB_GESTURE_MAX_POINTS) {
    return;
  }
  double tap = opt && opt->tap_size > 0.0 ? opt->tap_size : 12.0;
  double span = opt && opt->assist ? LENIENT : STRICT;
  double lo_x = xy[0], hi_x = xy[0], lo_y = xy[1], hi_y = xy[1];
  for (int k = 1; k < n; ++k) {
    lo_x = xy[2 * k] < lo_x ? xy[2 * k] : lo_x;
    hi_x = xy[2 * k] > hi_x ? xy[2 * k] : hi_x;
    lo_y = xy[2 * k + 1] < lo_y ? xy[2 * k + 1] : lo_y;
    hi_y = xy[2 * k + 1] > hi_y ? xy[2 * k + 1] : hi_y;
  }
  out->cx = 0.5 * (lo_x + hi_x);
  out->cy = 0.5 * (lo_y + hi_y);
  out->size = 0.5 * ((hi_x - lo_x) > (hi_y - lo_y) ? hi_x - lo_x : hi_y - lo_y);
  out->angle = zb_datan2((double)xy[2 * n - 1] - xy[1], (double)xy[2 * n - 2] - xy[0]);
  shape in;
  double length = n > 1 ? resample(xy, n, &in) : 0.0;
  if (length < tap || !normalise(&in)) {
    out->form = ZB_FORM_TAP;
    out->accuracy = 1.0;
    return;
  }
  double best = -1.0, second = -1.0;
  for (int f = ZB_FORM_LINE; f < ZB_FORMS; ++f) {
    double s = best_match(&in, (zb_form)f);
    double acc = 1.0 - zb_dacos(s) / span;
    acc = acc < 0.0 ? 0.0 : acc;
    if (acc > best) {
      second = best;
      out->second = out->form;
      best = acc;
      out->form = (zb_form)f;
    } else if (acc > second) {
      second = acc;
      out->second = (zb_form)f;
    }
  }
  out->accuracy = best;
  out->second_accuracy = second > 0.0 ? second : 0.0;
  if (best < ZB_GESTURE_MIN_ACCURACY) {
    out->second = out->form;
    out->second_accuracy = best;
    out->form = ZB_FORM_NONE;
  }
}
