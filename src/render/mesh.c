/* SPDX-License-Identifier: MIT */
#include "render/mesh.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cgltf.h"

void zb_mesh_free(const zb_allocator *a, zb_mesh_data *m) {
  for (size_t i = 0; i < m->image_count; ++i) {
    zb_image_free(a, &m->images[i]);
  }
  zb_free(a, m->images);
  zb_free(a, m->vertices);
  zb_free(a, m->indices);
  zb_free(a, m->submeshes);
  memset(m, 0, sizeof(*m));
}

void zb_mesh_compute_bounds(zb_mesh_data *m) {
  if (m->vertex_count == 0) {
    m->center = zb_v3(0, 0, 0);
    m->radius = 0.0f;
    return;
  }
  zb_vec3 lo = zb_v3(m->vertices[0].pos[0], m->vertices[0].pos[1], m->vertices[0].pos[2]);
  zb_vec3 hi = lo;
  for (size_t i = 1; i < m->vertex_count; ++i) {
    const float *p = m->vertices[i].pos;
    lo = zb_v3(fminf(lo.x, p[0]), fminf(lo.y, p[1]), fminf(lo.z, p[2]));
    hi = zb_v3(fmaxf(hi.x, p[0]), fmaxf(hi.y, p[1]), fmaxf(hi.z, p[2]));
  }
  m->center = zb_v3_scale(zb_v3_add(lo, hi), 0.5f);
  float r = 0.0f;
  for (size_t i = 0; i < m->vertex_count; ++i) {
    const float *p = m->vertices[i].pos;
    float d = zb_v3_length(zb_v3_sub(zb_v3(p[0], p[1], p[2]), m->center));
    r = fmaxf(r, d);
  }
  m->radius = r;
}

zb_result zb_mesh_cube(const zb_allocator *a, zb_mesh_data *out) {
  memset(out, 0, sizeof(*out));
  static const float n[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  out->vertices = zb_calloc(a, 24 * sizeof(zb_vertex));
  out->indices = zb_alloc(a, 36 * sizeof(uint32_t));
  out->submeshes = zb_calloc(a, sizeof(zb_submesh));
  if (!out->vertices || !out->indices || !out->submeshes) {
    zb_mesh_free(a, out);
    return ZB_ERR_NOMEM;
  }
  for (int f = 0; f < 6; ++f) {
    zb_vec3 nn = zb_v3(n[f][0], n[f][1], n[f][2]);
    zb_vec3 u = fabsf(nn.y) > 0.5f ? zb_v3(1, 0, 0) : zb_v3(0, 1, 0);
    zb_vec3 v = zb_v3_cross(nn, u);
    for (int k = 0; k < 4; ++k) {
      float su = (k == 1 || k == 2) ? 0.5f : -0.5f;
      float sv = (k >= 2) ? 0.5f : -0.5f;
      zb_vec3 p = zb_v3_add(zb_v3_scale(nn, 0.5f), zb_v3_add(zb_v3_scale(u, su), zb_v3_scale(v, sv)));
      zb_vertex *vx = &out->vertices[f * 4 + k];
      vx->pos[0] = p.x;
      vx->pos[1] = p.y;
      vx->pos[2] = p.z;
      vx->normal[0] = nn.x;
      vx->normal[1] = nn.y;
      vx->normal[2] = nn.z;
      vx->color[0] = 1.0f;
      vx->color[1] = 0.0f;
      vx->color[2] = 1.0f;
      vx->color[3] = 1.0f;
      vx->uv[0] = su + 0.5f;
      vx->uv[1] = sv + 0.5f;
    }
    /* winding: counter-clockwise seen from outside */
    uint32_t base = (uint32_t)f * 4u;
    zb_vec3 c = zb_v3_cross(u, v);
    bool flip = zb_v3_dot(c, nn) < 0.0f;
    uint32_t tri[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) {
      out->indices[f * 6 + k] = base + (flip ? tri[5 - k] : tri[k]);
    }
  }
  out->vertex_count = 24;
  out->index_count = 36;
  out->submesh_count = 1;
  out->submeshes[0].first = 0;
  out->submeshes[0].count = 36;
  out->submeshes[0].image = -1;
  zb_mesh_compute_bounds(out);
  return ZB_OK;
}

/* ---- glTF -------------------------------------------------------------------- */

typedef struct read_ctx {
  zb_mesh_read_fn read;
  void *user;
  const zb_allocator *a;
} read_ctx;

static cgltf_result cg_read(const struct cgltf_memory_options *mem, const struct cgltf_file_options *file,
                            const char *path, cgltf_size *size, void **data) {
  (void)mem;
  read_ctx *rc = file->user_data;
  if (!rc->read) {
    return cgltf_result_file_not_found;
  }
  char *buf;
  size_t len;
  if (rc->read(rc->user, path, rc->a, &buf, &len) != ZB_OK) {
    return cgltf_result_file_not_found;
  }
  *size = len;
  *data = buf;
  return cgltf_result_success;
}

static void cg_release(const struct cgltf_memory_options *mem, const struct cgltf_file_options *file, void *data) {
  (void)mem;
  read_ctx *rc = file->user_data;
  zb_free(rc->a, data);
}

static int find_image(const cgltf_data *g, const cgltf_image *img) {
  for (cgltf_size i = 0; i < g->images_count; ++i) {
    if (&g->images[i] == img) {
      return (int)i;
    }
  }
  return -1;
}

static zb_result decode_images(const zb_allocator *a, const cgltf_data *g, read_ctx *rc, zb_mesh_data *out, char *err,
                               size_t err_len) {
  if (g->images_count == 0) {
    return ZB_OK;
  }
  out->images = zb_calloc(a, g->images_count * sizeof(zb_image));
  if (!out->images) {
    return ZB_ERR_NOMEM;
  }
  out->image_count = g->images_count;
  for (cgltf_size i = 0; i < g->images_count; ++i) {
    const cgltf_image *img = &g->images[i];
    const void *bytes = NULL;
    size_t len = 0;
    char *owned = NULL;
    if (img->buffer_view && img->buffer_view->buffer->data) {
      bytes = (const unsigned char *)img->buffer_view->buffer->data + img->buffer_view->offset;
      len = img->buffer_view->size;
    } else if (img->uri && rc->read && rc->read(rc->user, img->uri, a, &owned, &len) == ZB_OK) {
      bytes = owned;
    }
    zb_result r = bytes ? zb_image_decode_png(a, bytes, len, &out->images[i]) : ZB_ERR_NOT_FOUND;
    zb_free(a, owned);
    if (r != ZB_OK) {
      snprintf(err, err_len, "image %u: %s", (unsigned)i, zb_result_str(r));
      return r;
    }
  }
  return ZB_OK;
}

static void count_primitives(const cgltf_data *g, size_t *nv, size_t *ni, size_t *np) {
  *nv = *ni = *np = 0;
  for (cgltf_size n = 0; n < g->nodes_count; ++n) {
    const cgltf_mesh *mesh = g->nodes[n].mesh;
    for (cgltf_size p = 0; mesh && p < mesh->primitives_count; ++p) {
      const cgltf_primitive *prim = &mesh->primitives[p];
      if (prim->type != cgltf_primitive_type_triangles) {
        continue;
      }
      for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
        if (prim->attributes[k].type == cgltf_attribute_type_position) {
          *nv += prim->attributes[k].data->count;
          *ni += prim->indices ? prim->indices->count : prim->attributes[k].data->count;
          (*np)++;
        }
      }
    }
  }
}

static void bake_primitive(const cgltf_node *node, const cgltf_primitive *prim, zb_mesh_data *out,
                           const cgltf_data *g) {
  float world[16];
  cgltf_node_transform_world(node, world);
  zb_mat4 m;
  memcpy(m.m, world, sizeof world);
  zb_mat4 nm = zb_m4_normal_matrix(m);
  const cgltf_accessor *pos = NULL, *nrm = NULL, *col = NULL, *uv = NULL;
  for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
    const cgltf_attribute *at = &prim->attributes[k];
    if (at->type == cgltf_attribute_type_position)
      pos = at->data;
    else if (at->type == cgltf_attribute_type_normal)
      nrm = at->data;
    else if (at->type == cgltf_attribute_type_color && at->index == 0)
      col = at->data;
    else if (at->type == cgltf_attribute_type_texcoord && at->index == 0)
      uv = at->data;
  }
  float base[4] = {1, 1, 1, 1};
  int image = -1;
  if (prim->material && prim->material->has_pbr_metallic_roughness) {
    memcpy(base, prim->material->pbr_metallic_roughness.base_color_factor, sizeof base);
    const cgltf_texture *tex = prim->material->pbr_metallic_roughness.base_color_texture.texture;
    if (tex && tex->image) {
      image = find_image(g, tex->image);
    }
  }
  uint32_t first_vertex = (uint32_t)out->vertex_count;
  for (cgltf_size i = 0; i < pos->count; ++i) {
    zb_vertex *v = &out->vertices[out->vertex_count++];
    float p[4] = {0, 0, 0, 1};
    cgltf_accessor_read_float(pos, i, p, 3);
    zb_vec3 wp = zb_m4_transform_point(m, zb_v3(p[0], p[1], p[2]));
    v->pos[0] = wp.x;
    v->pos[1] = wp.y;
    v->pos[2] = wp.z;
    float n[3] = {0, 1, 0};
    if (nrm) {
      cgltf_accessor_read_float(nrm, i, n, 3);
    }
    zb_vec4 wn4 = zb_m4_mul_v4(nm, (zb_vec4){n[0], n[1], n[2], 0.0f});
    zb_vec3 wn = zb_v3_normalize(zb_v3(wn4.x, wn4.y, wn4.z));
    v->normal[0] = wn.x;
    v->normal[1] = wn.y;
    v->normal[2] = wn.z;
    float c[4] = {1, 1, 1, 1};
    if (col) {
      cgltf_accessor_read_float(col, i, c, cgltf_num_components(col->type));
    }
    for (int k = 0; k < 4; ++k) {
      v->color[k] = c[k] * base[k];
    }
    float t[2] = {0, 0};
    if (uv) {
      cgltf_accessor_read_float(uv, i, t, 2);
    }
    v->uv[0] = t[0];
    v->uv[1] = t[1];
  }
  zb_submesh *sm = &out->submeshes[out->submesh_count++];
  sm->first = (uint32_t)out->index_count;
  sm->image = image;
  if (prim->indices) {
    for (cgltf_size i = 0; i < prim->indices->count; ++i) {
      out->indices[out->index_count++] = first_vertex + (uint32_t)cgltf_accessor_read_index(prim->indices, i);
    }
  } else {
    for (cgltf_size i = 0; i < pos->count; ++i) {
      out->indices[out->index_count++] = first_vertex + (uint32_t)i;
    }
  }
  sm->count = (uint32_t)out->index_count - sm->first;
}

zb_result zb_mesh_load_gltf(const zb_allocator *a, const void *data, size_t len, zb_mesh_read_fn read, void *read_user,
                            zb_mesh_data *out, char *err, size_t err_len) {
  memset(out, 0, sizeof(*out));
  read_ctx rc = {read, read_user, a};
  cgltf_options opts;
  memset(&opts, 0, sizeof opts);
  opts.file.read = cg_read;
  opts.file.release = cg_release;
  opts.file.user_data = &rc;
  cgltf_data *g = NULL;
  cgltf_result cr = cgltf_parse(&opts, data, len, &g);
  if (cr != cgltf_result_success) {
    snprintf(err, err_len, "not a valid glTF file (cgltf error %d)", (int)cr);
    return ZB_ERR_PARSE;
  }
  cr = cgltf_load_buffers(&opts, g, "");
  if (cr == cgltf_result_success) {
    cr = cgltf_validate(g);
  }
  if (cr != cgltf_result_success) {
    snprintf(err, err_len, "glTF buffers missing or invalid (cgltf error %d)", (int)cr);
    cgltf_free(g);
    return ZB_ERR_PARSE;
  }
  size_t nv, ni, np;
  count_primitives(g, &nv, &ni, &np);
  if (np == 0) {
    snprintf(err, err_len, "glTF file contains no triangle meshes");
    cgltf_free(g);
    return ZB_ERR_PARSE;
  }
  out->vertices = zb_calloc(a, nv * sizeof(zb_vertex));
  out->indices = zb_alloc(a, ni * sizeof(uint32_t));
  out->submeshes = zb_calloc(a, np * sizeof(zb_submesh));
  zb_result r = (out->vertices && out->indices && out->submeshes) ? ZB_OK : ZB_ERR_NOMEM;
  if (r == ZB_OK) {
    r = decode_images(a, g, &rc, out, err, err_len);
  }
  if (r == ZB_OK) {
    for (cgltf_size n = 0; n < g->nodes_count; ++n) {
      const cgltf_mesh *mesh = g->nodes[n].mesh;
      for (cgltf_size p = 0; mesh && p < mesh->primitives_count; ++p) {
        const cgltf_primitive *prim = &mesh->primitives[p];
        if (prim->type == cgltf_primitive_type_triangles &&
            cgltf_find_accessor(prim, cgltf_attribute_type_position, 0)) {
          bake_primitive(&g->nodes[n], prim, out, g);
        }
      }
    }
    zb_mesh_compute_bounds(out);
  } else if (r == ZB_ERR_NOMEM) {
    snprintf(err, err_len, "out of memory");
  }
  cgltf_free(g);
  if (r != ZB_OK) {
    zb_mesh_free(a, out);
  }
  return r;
}
