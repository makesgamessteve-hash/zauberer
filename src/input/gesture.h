/* SPDX-License-Identifier: MIT */
/* Gesture recognition (concept 3.3): the eight forms of a weave drawn with a
 * mouse, a finger or a stick.
 *
 * A stroke is a list of 2D points in drawing order. Strokes shorter than the
 * tap size are taps. Others are resampled, centred and scaled, then compared
 * with the forms' templates by their cosine distance at the best rotation
 * (Protractor), drawn either way round and mirrored, so a circle may start
 * anywhere and a spiral may turn either way. The accuracy (1 for a perfect
 * shape) becomes a weave's control; strokes like no form are refused. */
#ifndef ZB_INPUT_GESTURE_H
#define ZB_INPUT_GESTURE_H

#include "core/base.h"

typedef enum zb_form {
  ZB_FORM_NONE = -1,
  ZB_FORM_TAP,
  ZB_FORM_LINE,
  ZB_FORM_CIRCLE,
  ZB_FORM_SPIRAL,
  ZB_FORM_TRIANGLE,
  ZB_FORM_SQUARE,
  ZB_FORM_ZIGZAG,
  ZB_FORM_WAVE,
  ZB_FORMS
} zb_form;

#define ZB_GESTURE_MAX_POINTS 4096
#define ZB_GESTURE_MIN_ACCURACY 0.55 /* below this a stroke is no form */

typedef struct zb_gesture_options {
  double tap_size; /* strokes shorter than this (input units) are taps; <= 0: 12 */
  bool assist;     /* gesture assist (accessibility): accept sloppier shapes */
} zb_gesture_options;

typedef struct zb_gesture {
  zb_form form;    /* ZB_FORM_NONE when nothing matched well enough */
  double accuracy; /* 0 .. 1 */
  zb_form second;  /* the next best form */
  double second_accuracy;
  double size;   /* half the stroke's extent (input units) */
  double cx, cy; /* its centre */
  double angle;  /* radians from the first to the last point (atan2 of dy, dx) */
} zb_gesture;

/* Recognises a stroke of n points (xy holds x0, y0, x1, y1, ...). opt may be NULL. */
void zb_gesture_recognize(const float *xy, int n, const zb_gesture_options *opt, zb_gesture *out);
const char *zb_form_name(zb_form f);
/* The form of a name, or ZB_FORM_NONE. */
zb_form zb_form_by_name(const char *name);
/* Writes the template stroke of a form (count points, unit size, the way the
 * game draws it) to xy; for tests and the tutorial's tracing guide. */
void zb_gesture_template(zb_form f, float *xy, int count);

#endif
