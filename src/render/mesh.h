/* SPDX-License-Identifier: MIT */
/* CPU-side mesh data and the glTF 2.0 loader (static meshes). */
#ifndef ZB_RENDER_MESH_H
#define ZB_RENDER_MESH_H

#include "core/alloc.h"
#include "core/math.h"
#include "render/image.h"

typedef struct zb_vertex {
  float pos[3];
  float normal[3];
  float color[4];
  float uv[2];
} zb_vertex;

typedef struct zb_submesh {
  uint32_t first; /* first index */
  uint32_t count; /* index count */
  int image;      /* index into zb_mesh_data.images, or -1 */
} zb_submesh;

typedef struct zb_mesh_data {
  zb_vertex *vertices;
  size_t vertex_count;
  uint32_t *indices;
  size_t index_count;
  zb_submesh *submeshes;
  size_t submesh_count;
  zb_image *images;
  size_t image_count;
  zb_vec3 center;
  float radius;
} zb_mesh_data;

/* Reads external resources referenced by a .gltf (buffers, images). */
typedef zb_result (*zb_mesh_read_fn)(void *user, const char *relative_path, const zb_allocator *a, char **data,
                                     size_t *len);

/* Parses .glb or .gltf from memory. Node transforms are baked into the
 * vertices; vertex colors are multiplied by the material's base color. */
zb_result zb_mesh_load_gltf(const zb_allocator *a, const void *data, size_t len, zb_mesh_read_fn read, void *read_user,
                            zb_mesh_data *out, char *err, size_t err_len);
/* A unit cube centred at the origin (the missing-model placeholder). */
zb_result zb_mesh_cube(const zb_allocator *a, zb_mesh_data *out);
void zb_mesh_compute_bounds(zb_mesh_data *m);
void zb_mesh_free(const zb_allocator *a, zb_mesh_data *m);

#endif
