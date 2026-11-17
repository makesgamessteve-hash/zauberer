/* SPDX-License-Identifier: MIT */
/* The planet: a cube-sphere with an equi-angular mapping, divided into chunks
 * of nominally 256 m. Positions are (face, chunk x, chunk y, local offset,
 * altitude); a floating origin turns them into precise float coordinates
 * relative to an anchor near the camera. All functions are deterministic
 * (core/dmath) and identical on every platform. */
#ifndef ZB_WORLD_PLANET_H
#define ZB_WORLD_PLANET_H

#include "core/base.h"
#include "core/math.h"

#define ZB_CHUNK_SIZE 256.0 /* nominal chunk width in metres */
#define ZB_PLANET_MIN_RADIUS 50000.0
#define ZB_PLANET_MAX_RADIUS 6371000.0
#define ZB_PLANET_DEFAULT_RADIUS 2000000.0
#define ZB_PLANET_FACES 6

/* Face order and axes are part of the save format. +Z is the north pole. */
enum { ZB_FACE_PX, ZB_FACE_NX, ZB_FACE_PY, ZB_FACE_NY, ZB_FACE_PZ, ZB_FACE_NZ };

typedef struct zb_dvec3 {
  double x, y, z;
} zb_dvec3;

typedef struct zb_planet {
  double radius;   /* sea level, metres */
  int32_t n;       /* chunks per face edge */
  double chunk_uv; /* face-space width of one chunk (2 / n) */
} zb_planet;

typedef struct zb_chunk {
  int32_t face, x, y;
} zb_chunk;

typedef struct zb_planet_pos {
  zb_chunk chunk;
  float lx, ly; /* offset inside the chunk in nominal metres, 0..256 */
  float alt;    /* metres above sea level */
} zb_planet_pos;

/* Fails (false, message in err) when the radius is outside 50 km..6,371 km. */
bool zb_planet_init(zb_planet *p, double radius, char *err, size_t err_len);

bool zb_chunk_valid(const zb_planet *p, zb_chunk c);
bool zb_chunk_equal(zb_chunk a, zb_chunk b);
/* face (3 bits) << 60 | x (30 bits) << 30 | y (30 bits). */
uint64_t zb_chunk_key(zb_chunk c);
zb_chunk zb_chunk_from_key(uint64_t key);

/* Unit direction from the planet centre through a face point (u, v in -1..1). */
zb_dvec3 zb_planet_face_dir(int face, double u, double v);
/* The face owning a direction (largest absolute component; ties in face order). */
int zb_planet_dir_face(zb_dvec3 d);
/* Face coordinates of a direction on the given face (may exceed -1..1 when the
 * direction belongs to a neighbouring face). */
void zb_planet_dir_uv(int face, zb_dvec3 d, double *u, double *v);

/* Planet-centred Cartesian position (metres) and back. */
zb_dvec3 zb_planet_to_cart(const zb_planet *p, const zb_planet_pos *pos);
zb_planet_pos zb_planet_from_cart(const zb_planet *p, zb_dvec3 c);
/* Unit direction of a chunk's centre. */
zb_dvec3 zb_planet_chunk_dir(const zb_planet *p, zb_chunk c);
zb_planet_pos zb_planet_chunk_center(zb_chunk c);

/* The chunk at offset (dx, dy) in {-1, 0, 1} in face-grid steps, crossing face
 * seams; false for the missing diagonal at a cube corner. */
bool zb_planet_neighbor(const zb_planet *p, zb_chunk c, int dx, int dy, zb_chunk *out);
/* All distinct chunks sharing an edge or corner (8, or 7 at a cube corner). */
int zb_planet_neighbors(const zb_planet *p, zb_chunk c, zb_chunk out[8]);

/* Local frame at a direction: up, geographic east and north (+Z is north;
 * at the poles east is +Y). */
void zb_planet_frame(zb_dvec3 dir, zb_dvec3 *east, zb_dvec3 *north, zb_dvec3 *up);
/* Great-circle distance at sea level, metres. */
double zb_planet_distance(const zb_planet *p, const zb_planet_pos *a, const zb_planet_pos *b);
/* Approximate edge lengths of a chunk on the sphere (metres, sea level). */
void zb_planet_chunk_size(const zb_planet *p, zb_chunk c, double *width, double *height);

/* ---- Floating origin --------------------------------------------------------
 * Local axes: x = east, y = up, z = south (right-handed, y up). */
typedef struct zb_origin {
  zb_chunk chunk;
  zb_dvec3 center; /* anchor: chunk centre at sea level */
  zb_dvec3 east, north, up;
} zb_origin;

#define ZB_ORIGIN_REBASE_DISTANCE 1024.0f

void zb_origin_init(zb_origin *o, const zb_planet *p, zb_chunk anchor);
zb_vec3 zb_origin_from_cart(const zb_origin *o, zb_dvec3 c);
zb_dvec3 zb_origin_to_cart(const zb_origin *o, zb_vec3 local);
zb_vec3 zb_origin_to_local(const zb_origin *o, const zb_planet *p, const zb_planet_pos *pos);
zb_planet_pos zb_origin_to_pos(const zb_origin *o, const zb_planet *p, zb_vec3 local);
/* True when a local position (the camera) is farther than distance from the anchor. */
bool zb_origin_should_rebase(zb_vec3 local, float distance);
/* Moves the anchor to a new chunk; returns the transform from old to new local
 * coordinates (apply it to every float position kept in the old frame). */
zb_mat4 zb_origin_rebase(zb_origin *o, const zb_planet *p, zb_chunk anchor);

#endif
