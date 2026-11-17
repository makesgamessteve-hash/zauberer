/* SPDX-License-Identifier: MIT */
/* OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2) render backend. Only features
 * common to both are used. GL entry points are loaded through a function
 * table, so this file needs no system GL headers. */
#include "render/backend.h"

#include "core/array.h"
#include "core/log.h"
#include "vfs/vfs.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(_WIN64)
#define ZB_GLAPI __stdcall
#else
#define ZB_GLAPI
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_LEQUAL 0x0203
#define GL_BACK 0x0405
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_STREAM_DRAW 0x88E0
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TRIANGLES 0x0004
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_FALSE 0
#define GL_TRUE 1

#define ZB_GL_FUNCS(X) \
  X(const GLubyte *, glGetString, (GLenum name)) \
  X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h)) \
  X(void, glEnable, (GLenum cap)) \
  X(void, glDisable, (GLenum cap)) \
  X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
  X(void, glClear, (GLbitfield mask)) \
  X(void, glDepthFunc, (GLenum func)) \
  X(void, glCullFace, (GLenum mode)) \
  X(void, glBlendFunc, (GLenum s, GLenum d)) \
  X(void, glGenTextures, (GLsizei n, GLuint * t)) \
  X(void, glDeleteTextures, (GLsizei n, const GLuint *t)) \
  X(void, glBindTexture, (GLenum target, GLuint t)) \
  X(void, glTexImage2D, \
    (GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type, \
     const void *data)) \
  X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param)) \
  X(void, glActiveTexture, (GLenum t)) \
  X(void, glPixelStorei, (GLenum pname, GLint param)) \
  X(void, glGenBuffers, (GLsizei n, GLuint * b)) \
  X(void, glDeleteBuffers, (GLsizei n, const GLuint *b)) \
  X(void, glBindBuffer, (GLenum target, GLuint b)) \
  X(void, glBufferData, (GLenum target, GLsizeiptr size, const void *data, GLenum usage)) \
  X(void, glGenVertexArrays, (GLsizei n, GLuint * a)) \
  X(void, glDeleteVertexArrays, (GLsizei n, const GLuint *a)) \
  X(void, glBindVertexArray, (GLuint a)) \
  X(void, glEnableVertexAttribArray, (GLuint i)) \
  X(void, glVertexAttribPointer, (GLuint i, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void *ptr)) \
  X(void, glVertexAttribDivisor, (GLuint i, GLuint divisor)) \
  X(GLuint, glCreateShader, (GLenum type)) \
  X(void, glShaderSource, (GLuint s, GLsizei count, const GLchar *const *src, const GLint *len)) \
  X(void, glCompileShader, (GLuint s)) \
  X(void, glGetShaderiv, (GLuint s, GLenum pname, GLint * v)) \
  X(void, glGetShaderInfoLog, (GLuint s, GLsizei max, GLsizei * len, GLchar * log)) \
  X(void, glDeleteShader, (GLuint s)) \
  X(GLuint, glCreateProgram, (void)) \
  X(void, glAttachShader, (GLuint p, GLuint s)) \
  X(void, glBindAttribLocation, (GLuint p, GLuint i, const GLchar *name)) \
  X(void, glLinkProgram, (GLuint p)) \
  X(void, glGetProgramiv, (GLuint p, GLenum pname, GLint * v)) \
  X(void, glGetProgramInfoLog, (GLuint p, GLsizei max, GLsizei * len, GLchar * log)) \
  X(void, glDeleteProgram, (GLuint p)) \
  X(void, glUseProgram, (GLuint p)) \
  X(GLint, glGetUniformLocation, (GLuint p, const GLchar *name)) \
  X(void, glUniform1i, (GLint loc, GLint v)) \
  X(void, glUniform2f, (GLint loc, GLfloat x, GLfloat y)) \
  X(void, glUniform3f, (GLint loc, GLfloat x, GLfloat y, GLfloat z)) \
  X(void, glUniformMatrix4fv, (GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)) \
  X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void *indices)) \
  X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count)) \
  X(void, glDrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instances)) \
  X(void, glGenFramebuffers, (GLsizei n, GLuint * f)) \
  X(void, glDeleteFramebuffers, (GLsizei n, const GLuint *f)) \
  X(void, glBindFramebuffer, (GLenum target, GLuint f)) \
  X(void, glFramebufferTexture2D, (GLenum target, GLenum att, GLenum tt, GLuint t, GLint level)) \
  X(GLenum, glCheckFramebufferStatus, (GLenum target)) \
  X(void, glGenRenderbuffers, (GLsizei n, GLuint * r)) \
  X(void, glDeleteRenderbuffers, (GLsizei n, const GLuint *r)) \
  X(void, glBindRenderbuffer, (GLenum target, GLuint r)) \
  X(void, glRenderbufferStorage, (GLenum target, GLenum fmt, GLsizei w, GLsizei h)) \
  X(void, glFramebufferRenderbuffer, (GLenum target, GLenum att, GLenum rt, GLuint r)) \
  X(void, glBlitFramebuffer, \
    (GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, \
     GLenum filter)) \
  X(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, void *data)) \
  X(void, glFinish, (void))

#define DECLARE(ret, name, args) ret(ZB_GLAPI *name) args;
typedef struct gl_funcs {
  ZB_GL_FUNCS(DECLARE)
} gl_funcs;
#undef DECLARE

typedef struct gl_mesh {
  GLuint vao, vbo, ibo;
} gl_mesh;

typedef struct gl_program {
  GLuint id;
  GLint view_proj, model, normal, light_dir, light_color, ambient, tex, right, up, screen;
} gl_program;

typedef struct sprite_instance {
  float pos[3];
  float size_pivot[4];
  float uv[4];
} sprite_instance;

typedef struct gl_ctx {
  zb_render_backend iface;
  const zb_allocator *a;
  gl_funcs gl;
  bool gles;
  char version[160];
  zb_array textures; /* GLuint, handle = index + 1, 0 = free slot */
  zb_array meshes;   /* gl_mesh */
  gl_program model, sprite, text;
  GLuint quad_vbo, inst_vbo, sprite_vao;
  GLuint text_vbo, text_vao;
  GLuint fbo, fbo_color, fbo_depth;
  int fbo_w, fbo_h;
  int last_dw, last_dh;
  zb_array instances; /* sprite_instance */
} gl_ctx;

/* ---- Version check ------------------------------------------------------------ */

bool zb_gl_parse_version(const char *s, bool *es, int *major, int *minor) {
  if (!s) {
    return false;
  }
  const char *es_tag = strstr(s, "OpenGL ES");
  const char *web_tag = strstr(s, "WebGL");
  /* Browsers report "WebGL 2.0 (OpenGL ES 3.0 ...)" or "OpenGL ES 3.0 (WebGL 2.0 ...)":
   * whichever comes first names the number that follows it. */
  bool webgl = web_tag && (!es_tag || web_tag < es_tag);
  *es = es_tag != NULL || web_tag != NULL;
  const char *p = s;
  while (*p && !isdigit((unsigned char)*p)) {
    p++;
  }
  if (!*p || sscanf(p, "%d.%d", major, minor) != 2) {
    return false;
  }
  if (webgl) {
    /* WebGL 2.0 corresponds to OpenGL ES 3.0, WebGL 1.0 to ES 2.0. */
    *major = *major + 1;
    *minor = 0;
  }
  return true;
}

bool zb_gl_version_ok(bool es, int major, int minor) {
  return es ? major >= 3 : (major > 3 || (major == 3 && minor >= 3));
}

/* ---- Helpers ------------------------------------------------------------------ */

static char *load_source(gl_ctx *c, zb_vfs *vfs, const char *name, const char *ext, char *err, size_t err_len) {
  char path[128];
  snprintf(path, sizeof path, "shaders/%s.%s", name, ext);
  char *body = NULL;
  size_t len = 0;
  if (zb_vfs_read(vfs, path, c->a, &body, &len) != ZB_OK) {
    snprintf(err, err_len, "shader '%s' was not found in the loaded packages", path);
    return NULL;
  }
  const char *header = c->gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n#define ZB_GLES 1\n"
                               : "#version 330 core\n";
  size_t hl = strlen(header);
  char *src = zb_alloc(c->a, hl + len + 1);
  if (src) {
    memcpy(src, header, hl);
    memcpy(src + hl, body, len + 1);
  }
  zb_free(c->a, body);
  return src;
}

static GLuint compile(gl_ctx *c, GLenum type, const char *src, const char *what, char *err, size_t err_len) {
  GLuint s = c->gl.glCreateShader(type);
  c->gl.glShaderSource(s, 1, &src, NULL);
  c->gl.glCompileShader(s);
  GLint ok = 0;
  c->gl.glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    c->gl.glGetShaderInfoLog(s, sizeof log, NULL, log);
    snprintf(err, err_len, "shader %s failed to compile: %s", what, log);
    c->gl.glDeleteShader(s);
    return 0;
  }
  return s;
}

static bool build_program(gl_ctx *c, zb_vfs *vfs, const char *name, const char *const *attribs, gl_program *out,
                          char *err, size_t err_len) {
  char *vs_src = load_source(c, vfs, name, "vert", err, err_len);
  char *fs_src = vs_src ? load_source(c, vfs, name, "frag", err, err_len) : NULL;
  GLuint vs = fs_src ? compile(c, GL_VERTEX_SHADER, vs_src, name, err, err_len) : 0;
  GLuint fs = vs ? compile(c, GL_FRAGMENT_SHADER, fs_src, name, err, err_len) : 0;
  zb_free(c->a, vs_src);
  zb_free(c->a, fs_src);
  if (!fs) {
    if (vs) {
      c->gl.glDeleteShader(vs);
    }
    return false;
  }
  GLuint p = c->gl.glCreateProgram();
  c->gl.glAttachShader(p, vs);
  c->gl.glAttachShader(p, fs);
  for (GLuint i = 0; attribs[i]; ++i) {
    c->gl.glBindAttribLocation(p, i, attribs[i]);
  }
  c->gl.glLinkProgram(p);
  c->gl.glDeleteShader(vs);
  c->gl.glDeleteShader(fs);
  GLint ok = 0;
  c->gl.glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    c->gl.glGetProgramInfoLog(p, sizeof log, NULL, log);
    snprintf(err, err_len, "shader program %s failed to link: %s", name, log);
    c->gl.glDeleteProgram(p);
    return false;
  }
  out->id = p;
  out->view_proj = c->gl.glGetUniformLocation(p, "u_view_proj");
  out->model = c->gl.glGetUniformLocation(p, "u_model");
  out->normal = c->gl.glGetUniformLocation(p, "u_normal");
  out->light_dir = c->gl.glGetUniformLocation(p, "u_light_dir");
  out->light_color = c->gl.glGetUniformLocation(p, "u_light_color");
  out->ambient = c->gl.glGetUniformLocation(p, "u_ambient");
  out->tex = c->gl.glGetUniformLocation(p, "u_tex");
  out->right = c->gl.glGetUniformLocation(p, "u_right");
  out->up = c->gl.glGetUniformLocation(p, "u_up");
  out->screen = c->gl.glGetUniformLocation(p, "u_screen");
  return true;
}

static void attrib(gl_ctx *c, GLuint index, GLint size, GLsizei stride, size_t offset, GLuint divisor) {
  c->gl.glEnableVertexAttribArray(index);
  c->gl.glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
  c->gl.glVertexAttribDivisor(index, divisor);
}

static GLuint texture_id(const gl_ctx *c, uint32_t handle) {
  if (handle == 0 || handle > c->textures.len) {
    return 0;
  }
  return *(GLuint *)zb_array_at(&c->textures, handle - 1);
}

/* ---- Interface --------------------------------------------------------------- */

static uint32_t gl_create_texture(void *ctx, const zb_image *img) {
  gl_ctx *c = ctx;
  GLuint t = 0;
  c->gl.glGenTextures(1, &t);
  c->gl.glBindTexture(GL_TEXTURE_2D, t);
  c->gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  c->gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->rgba);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  if (!zb_array_push(&c->textures, &t)) {
    c->gl.glDeleteTextures(1, &t);
    return 0;
  }
  return (uint32_t)c->textures.len;
}

static void gl_destroy_texture(void *ctx, uint32_t handle) {
  gl_ctx *c = ctx;
  GLuint t = texture_id(c, handle);
  if (t) {
    c->gl.glDeleteTextures(1, &t);
    *(GLuint *)zb_array_at(&c->textures, handle - 1) = 0;
  }
}

static uint32_t gl_create_mesh(void *ctx, const zb_mesh_data *mesh) {
  gl_ctx *c = ctx;
  gl_mesh m;
  c->gl.glGenVertexArrays(1, &m.vao);
  c->gl.glBindVertexArray(m.vao);
  c->gl.glGenBuffers(1, &m.vbo);
  c->gl.glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  c->gl.glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * sizeof(zb_vertex)), mesh->vertices,
                     GL_STATIC_DRAW);
  c->gl.glGenBuffers(1, &m.ibo);
  c->gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  c->gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)), mesh->indices,
                     GL_STATIC_DRAW);
  GLsizei stride = (GLsizei)sizeof(zb_vertex);
  attrib(c, 0, 3, stride, offsetof(zb_vertex, pos), 0);
  attrib(c, 1, 3, stride, offsetof(zb_vertex, normal), 0);
  attrib(c, 2, 4, stride, offsetof(zb_vertex, color), 0);
  attrib(c, 3, 2, stride, offsetof(zb_vertex, uv), 0);
  c->gl.glBindVertexArray(0);
  if (!zb_array_push(&c->meshes, &m)) {
    return 0;
  }
  return (uint32_t)c->meshes.len;
}

static void free_mesh(gl_ctx *c, gl_mesh *m) {
  if (m->vao) {
    c->gl.glDeleteVertexArrays(1, &m->vao);
    c->gl.glDeleteBuffers(1, &m->vbo);
    c->gl.glDeleteBuffers(1, &m->ibo);
    memset(m, 0, sizeof(*m));
  }
}

static void gl_destroy_mesh(void *ctx, uint32_t handle) {
  gl_ctx *c = ctx;
  if (handle && handle <= c->meshes.len) {
    free_mesh(c, zb_array_at(&c->meshes, handle - 1));
  }
}

static void ensure_fbo(gl_ctx *c, int w, int h) {
  if (c->fbo && c->fbo_w == w && c->fbo_h == h) {
    return;
  }
  if (c->fbo) {
    c->gl.glDeleteFramebuffers(1, &c->fbo);
    c->gl.glDeleteTextures(1, &c->fbo_color);
    c->gl.glDeleteRenderbuffers(1, &c->fbo_depth);
  }
  c->gl.glGenTextures(1, &c->fbo_color);
  c->gl.glBindTexture(GL_TEXTURE_2D, c->fbo_color);
  c->gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  c->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  c->gl.glGenRenderbuffers(1, &c->fbo_depth);
  c->gl.glBindRenderbuffer(GL_RENDERBUFFER, c->fbo_depth);
  c->gl.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  c->gl.glGenFramebuffers(1, &c->fbo);
  c->gl.glBindFramebuffer(GL_FRAMEBUFFER, c->fbo);
  c->gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c->fbo_color, 0);
  c->gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, c->fbo_depth);
  if (c->gl.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    zb_error("internal framebuffer %dx%d is incomplete", w, h);
  }
  c->fbo_w = w;
  c->fbo_h = h;
}

static void draw_models(gl_ctx *c, const zb_frame *f, int view) {
  const gl_program *p = &c->model;
  c->gl.glUseProgram(p->id);
  c->gl.glUniformMatrix4fv(p->view_proj, 1, GL_FALSE, f->views[view].view_proj.m);
  c->gl.glUniform3f(p->light_dir, f->light_dir.x, f->light_dir.y, f->light_dir.z);
  c->gl.glUniform3f(p->light_color, f->light_color.x, f->light_color.y, f->light_color.z);
  c->gl.glUniform3f(p->ambient, f->ambient.x, f->ambient.y, f->ambient.z);
  c->gl.glUniform1i(p->tex, 0);
  c->gl.glActiveTexture(GL_TEXTURE0);
  for (size_t i = 0; i < f->item_count; ++i) {
    const zb_draw_item *it = &f->items[i];
    if (it->kind != ZB_DRAW_MODEL || it->view != view || it->mesh == 0 || it->mesh > c->meshes.len) {
      continue;
    }
    const gl_mesh *m = zb_array_at(&c->meshes, it->mesh - 1);
    zb_mat4 nm = zb_m4_normal_matrix(it->model);
    c->gl.glUniformMatrix4fv(p->model, 1, GL_FALSE, it->model.m);
    c->gl.glUniformMatrix4fv(p->normal, 1, GL_FALSE, nm.m);
    c->gl.glBindTexture(GL_TEXTURE_2D, texture_id(c, it->texture));
    c->gl.glBindVertexArray(m->vao);
    c->gl.glDrawElements(GL_TRIANGLES, (GLsizei)it->count, GL_UNSIGNED_INT,
                         (const void *)((size_t)it->first * sizeof(uint32_t)));
  }
}

static void draw_sprites(gl_ctx *c, const zb_frame *f, int view) {
  const gl_program *p = &c->sprite;
  const zb_view *v = &f->views[view];
  bool started = false;
  size_t i = 0;
  while (i < f->item_count) {
    const zb_draw_item *it = &f->items[i];
    if (it->kind != ZB_DRAW_SPRITE || it->view != view) {
      ++i;
      continue;
    }
    /* Batch consecutive sprites with the same texture (items are sorted). */
    zb_array_clear(&c->instances);
    uint32_t tex = it->texture;
    while (i < f->item_count && f->items[i].kind == ZB_DRAW_SPRITE && f->items[i].view == view &&
           f->items[i].texture == tex) {
      const zb_draw_item *s = &f->items[i];
      sprite_instance inst = {{s->position.x, s->position.y, s->position.z},
                              {s->size[0], s->size[1], s->pivot[0], s->pivot[1]},
                              {s->uv[0], s->uv[1], s->uv[2], s->uv[3]}};
      zb_array_push(&c->instances, &inst);
      ++i;
    }
    if (!started) {
      c->gl.glUseProgram(p->id);
      c->gl.glUniformMatrix4fv(p->view_proj, 1, GL_FALSE, v->view_proj.m);
      c->gl.glUniform3f(p->right, v->right.x, v->right.y, v->right.z);
      c->gl.glUniform3f(p->up, v->up.x, v->up.y, v->up.z);
      c->gl.glUniform1i(p->tex, 0);
      c->gl.glDisable(GL_CULL_FACE);
      c->gl.glBindVertexArray(c->sprite_vao);
      started = true;
    }
    c->gl.glBindTexture(GL_TEXTURE_2D, texture_id(c, tex));
    c->gl.glBindBuffer(GL_ARRAY_BUFFER, c->inst_vbo);
    c->gl.glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c->instances.len * sizeof(sprite_instance)), c->instances.data,
                       GL_STREAM_DRAW);
    c->gl.glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)c->instances.len);
  }
  if (started) {
    c->gl.glEnable(GL_CULL_FACE);
  }
}

static void draw_text(gl_ctx *c, const zb_frame *f) {
  if (f->text_count == 0) {
    return;
  }
  const gl_program *p = &c->text;
  c->gl.glViewport(0, 0, f->layout.internal_w, f->layout.internal_h);
  c->gl.glDisable(GL_DEPTH_TEST);
  c->gl.glDisable(GL_CULL_FACE);
  c->gl.glUseProgram(p->id);
  c->gl.glUniform2f(p->screen, (GLfloat)f->layout.internal_w, (GLfloat)f->layout.internal_h);
  c->gl.glUniform1i(p->tex, 0);
  c->gl.glBindVertexArray(c->text_vao);
  c->gl.glBindBuffer(GL_ARRAY_BUFFER, c->text_vbo);
  c->gl.glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(f->text_count * sizeof(zb_text_vertex)), f->text, GL_STREAM_DRAW);
  for (size_t b = 0; b < f->text_batch_count; ++b) {
    c->gl.glBindTexture(GL_TEXTURE_2D, texture_id(c, f->text_batches[b].texture));
    c->gl.glDrawArrays(GL_TRIANGLES, (GLint)f->text_batches[b].first, (GLsizei)f->text_batches[b].count);
  }
  c->gl.glEnable(GL_DEPTH_TEST);
  c->gl.glEnable(GL_CULL_FACE);
}

static void gl_execute(void *ctx, const zb_frame *f) {
  gl_ctx *c = ctx;
  const zb_layout *l = &f->layout;
  ensure_fbo(c, l->internal_w, l->internal_h);
  c->gl.glBindFramebuffer(GL_FRAMEBUFFER, c->fbo);
  c->gl.glViewport(0, 0, l->internal_w, l->internal_h);
  c->gl.glClearColor(f->clear[0], f->clear[1], f->clear[2], 1.0f);
  c->gl.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  c->gl.glEnable(GL_DEPTH_TEST);
  c->gl.glDepthFunc(GL_LEQUAL);
  c->gl.glEnable(GL_CULL_FACE);
  c->gl.glCullFace(GL_BACK);
  for (int v = 0; v < f->view_count; ++v) {
    const zb_view *view = &f->views[v];
    c->gl.glViewport(view->x, view->y, view->width, view->height);
    draw_models(c, f, v);
    draw_sprites(c, f, v);
  }
  draw_text(c, f);
  c->gl.glBindVertexArray(0);
  /* Upscale to the window with nearest-neighbour filtering. */
  c->gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, c->fbo);
  c->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  c->gl.glViewport(0, 0, l->drawable_w, l->drawable_h);
  c->gl.glClearColor(f->clear[0], f->clear[1], f->clear[2], 1.0f);
  c->gl.glClear(GL_COLOR_BUFFER_BIT);
  c->gl.glBlitFramebuffer(0, 0, l->internal_w, l->internal_h, l->out_x, l->out_y, l->out_x + l->out_w,
                          l->out_y + l->out_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  c->gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
  c->last_dw = l->drawable_w;
  c->last_dh = l->drawable_h;
}

static zb_result gl_capture(void *ctx, bool internal, const zb_allocator *a, zb_image *out) {
  gl_ctx *c = ctx;
  int w = internal ? c->fbo_w : c->last_dw;
  int h = internal ? c->fbo_h : c->last_dh;
  if (w <= 0 || h <= 0) {
    return ZB_ERR_STATE;
  }
  out->width = w;
  out->height = h;
  out->rgba = zb_alloc(a, (size_t)w * (size_t)h * 4u);
  if (!out->rgba) {
    return ZB_ERR_NOMEM;
  }
  c->gl.glFinish();
  c->gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, internal ? c->fbo : 0);
  c->gl.glPixelStorei(GL_PACK_ALIGNMENT, 1);
  c->gl.glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out->rgba);
  c->gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  zb_image_flip_rows(out);
  return ZB_OK;
}

static void gl_destroy(void *ctx) {
  gl_ctx *c = ctx;
  if (c->model.id)
    c->gl.glDeleteProgram(c->model.id);
  if (c->sprite.id)
    c->gl.glDeleteProgram(c->sprite.id);
  if (c->text.id)
    c->gl.glDeleteProgram(c->text.id);
  if (c->sprite_vao) {
    c->gl.glDeleteVertexArrays(1, &c->sprite_vao);
    c->gl.glDeleteBuffers(1, &c->quad_vbo);
    c->gl.glDeleteBuffers(1, &c->inst_vbo);
  }
  if (c->text_vao) {
    c->gl.glDeleteVertexArrays(1, &c->text_vao);
    c->gl.glDeleteBuffers(1, &c->text_vbo);
  }
  if (c->fbo) {
    c->gl.glDeleteFramebuffers(1, &c->fbo);
    c->gl.glDeleteTextures(1, &c->fbo_color);
    c->gl.glDeleteRenderbuffers(1, &c->fbo_depth);
  }
  for (size_t i = 0; i < c->textures.len; ++i) {
    GLuint t = *(GLuint *)zb_array_at(&c->textures, i);
    if (t) {
      c->gl.glDeleteTextures(1, &t);
    }
  }
  for (size_t i = 0; i < c->meshes.len; ++i) {
    free_mesh(c, zb_array_at(&c->meshes, i));
  }
  zb_array_free(&c->textures);
  zb_array_free(&c->meshes);
  zb_array_free(&c->instances);
  zb_free(c->a, c);
}

static void setup_geometry(gl_ctx *c) {
  static const float quad[12] = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
  c->gl.glGenVertexArrays(1, &c->sprite_vao);
  c->gl.glBindVertexArray(c->sprite_vao);
  c->gl.glGenBuffers(1, &c->quad_vbo);
  c->gl.glBindBuffer(GL_ARRAY_BUFFER, c->quad_vbo);
  c->gl.glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  attrib(c, 0, 2, 2 * sizeof(float), 0, 0);
  c->gl.glGenBuffers(1, &c->inst_vbo);
  c->gl.glBindBuffer(GL_ARRAY_BUFFER, c->inst_vbo);
  GLsizei st = (GLsizei)sizeof(sprite_instance);
  attrib(c, 1, 3, st, offsetof(sprite_instance, pos), 1);
  attrib(c, 2, 4, st, offsetof(sprite_instance, size_pivot), 1);
  attrib(c, 3, 4, st, offsetof(sprite_instance, uv), 1);
  c->gl.glGenVertexArrays(1, &c->text_vao);
  c->gl.glBindVertexArray(c->text_vao);
  c->gl.glGenBuffers(1, &c->text_vbo);
  c->gl.glBindBuffer(GL_ARRAY_BUFFER, c->text_vbo);
  GLsizei ts = (GLsizei)sizeof(zb_text_vertex);
  attrib(c, 0, 2, ts, offsetof(zb_text_vertex, x), 0);
  attrib(c, 1, 2, ts, offsetof(zb_text_vertex, u), 0);
  attrib(c, 2, 4, ts, offsetof(zb_text_vertex, rgba), 0);
  c->gl.glBindVertexArray(0);
}

zb_render_backend *zb_render_gl_backend(const zb_allocator *a, zb_gl_loader load_proc, void *loader_ctx, bool gles,
                                        zb_vfs *vfs, char *err, size_t err_len) {
  gl_ctx *c = zb_calloc(a, sizeof(*c));
  if (!c) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  c->a = a;
  c->gles = gles;
  zb_array_init(&c->textures, a, sizeof(GLuint));
  zb_array_init(&c->meshes, a, sizeof(gl_mesh));
  zb_array_init(&c->instances, a, sizeof(sprite_instance));
  /* Check the version first: an old driver (e.g. Windows' GDI OpenGL 1.1)
   * lacks most functions, and the version is the useful message then. */
  typedef const GLubyte *(ZB_GLAPI * get_string_fn)(GLenum name);
  get_string_fn get_string = (get_string_fn)load_proc(loader_ctx, "glGetString");
  const char *version = get_string ? (const char *)get_string(GL_VERSION) : NULL;
  snprintf(c->version, sizeof c->version, "%s", version ? version : "unknown");
  bool es = gles;
  int major = 0, minor = 0;
  if (!zb_gl_parse_version(version, &es, &major, &minor) || !zb_gl_version_ok(es || gles, major, minor)) {
    const char *renderer = get_string ? (const char *)get_string(GL_RENDERER) : NULL;
    snprintf(err, err_len, "%s is required, but the detected version is %s (%s)",
             gles ? "OpenGL ES 3.0 / WebGL 2" : "OpenGL 3.3 core profile", c->version,
             renderer ? renderer : "unknown renderer");
    gl_destroy(c);
    return NULL;
  }
#define LOAD(ret, name, args) \
  c->gl.name = (ret(ZB_GLAPI *) args)load_proc(loader_ctx, #name); \
  if (!c->gl.name) { \
    snprintf(err, err_len, "OpenGL function %s is missing (OpenGL %s)", #name, c->version); \
    gl_destroy(c); \
    return NULL; \
  }
  ZB_GL_FUNCS(LOAD)
#undef LOAD
  static const char *const model_attribs[] = {"a_pos", "a_normal", "a_color", "a_uv", NULL};
  static const char *const sprite_attribs[] = {"a_corner", "i_pos", "i_size_pivot", "i_uv", NULL};
  static const char *const text_attribs[] = {"a_pos", "a_uv", "a_color", NULL};
  if (!build_program(c, vfs, "model", model_attribs, &c->model, err, err_len) ||
      !build_program(c, vfs, "sprite", sprite_attribs, &c->sprite, err, err_len) ||
      !build_program(c, vfs, "text", text_attribs, &c->text, err, err_len)) {
    gl_destroy(c);
    return NULL;
  }
  setup_geometry(c);
  zb_info("renderer: %s (%s)", c->version, (const char *)c->gl.glGetString(GL_RENDERER));
  c->iface.ctx = c;
  c->iface.name = "gl";
  c->iface.destroy = gl_destroy;
  c->iface.create_texture = gl_create_texture;
  c->iface.destroy_texture = gl_destroy_texture;
  c->iface.create_mesh = gl_create_mesh;
  c->iface.destroy_mesh = gl_destroy_mesh;
  c->iface.execute = gl_execute;
  c->iface.capture = gl_capture;
  return &c->iface;
}
