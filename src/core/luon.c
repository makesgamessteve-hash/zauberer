/* SPDX-License-Identifier: MIT */
#include "core/luon.h"

#include "core/array.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct parser {
  const char *src;
  size_t len;
  size_t pos;
  int line;
  size_t line_start;
  int depth;
  zb_luon_doc *doc;
  const zb_allocator *a;
  zb_luon_error *err;
  bool failed;
} parser;

static void fail(parser *p, const char *fmt, ...) ZB_PRINTF(2, 3);

static void fail(parser *p, const char *fmt, ...) {
  if (p->failed) {
    return;
  }
  p->failed = true;
  if (p->err) {
    p->err->line = p->line;
    p->err->column = (int)(p->pos - p->line_start) + 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->err->message, sizeof p->err->message, fmt, ap);
    va_end(ap);
  }
}

static int peek(const parser *p, size_t ahead) {
  return p->pos + ahead < p->len ? (unsigned char)p->src[p->pos + ahead] : -1;
}

static void advance(parser *p) {
  if (p->pos < p->len) {
    if (p->src[p->pos] == '\n') {
      p->line++;
      p->line_start = p->pos + 1;
    }
    p->pos++;
  }
}

/* Long bracket level at pos ("[[" -> 0, "[==[" -> 2), or -1 if none. */
static int long_bracket_level(const parser *p) {
  if (peek(p, 0) != '[') {
    return -1;
  }
  size_t i = 1;
  while (peek(p, i) == '=') {
    i++;
  }
  return peek(p, i) == '[' ? (int)(i - 1) : -1;
}

/* Reads a long string body after the opening bracket; returns false on EOF. */
static bool read_long(parser *p, int level, const char **start, size_t *out_len) {
  for (int i = 0; i < level + 2; ++i) {
    advance(p);
  }
  if (peek(p, 0) == '\r') {
    advance(p);
  }
  if (peek(p, 0) == '\n') {
    advance(p);
  }
  *start = p->src + p->pos;
  size_t begin = p->pos;
  while (p->pos < p->len) {
    if (peek(p, 0) == ']') {
      int i = 1;
      while (peek(p, (size_t)i) == '=') {
        i++;
      }
      if (i - 1 == level && peek(p, (size_t)i) == ']') {
        *out_len = p->pos - begin;
        for (int k = 0; k <= i; ++k) {
          advance(p);
        }
        return true;
      }
    }
    advance(p);
  }
  return false;
}

static void skip_space(parser *p) {
  for (;;) {
    int c = peek(p, 0);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
      advance(p);
    } else if (c == '-' && peek(p, 1) == '-') {
      advance(p);
      advance(p);
      int level = long_bracket_level(p);
      if (level >= 0) {
        const char *s;
        size_t n;
        if (!read_long(p, level, &s, &n)) {
          fail(p, "unfinished long comment");
          return;
        }
      } else {
        while (p->pos < p->len && peek(p, 0) != '\n') {
          advance(p);
        }
      }
    } else {
      return;
    }
  }
}

static bool is_name_start(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static bool is_name_char(int c) { return is_name_start(c) || (c >= '0' && c <= '9'); }

static size_t name_length(const parser *p) {
  size_t n = 0;
  if (!is_name_start(peek(p, 0))) {
    return 0;
  }
  while (is_name_char(peek(p, n))) {
    n++;
  }
  return n;
}

static bool match_keyword(parser *p, const char *kw) {
  size_t n = strlen(kw);
  if (name_length(p) == n && strncmp(p->src + p->pos, kw, n) == 0) {
    for (size_t i = 0; i < n; ++i) {
      advance(p);
    }
    return true;
  }
  return false;
}

static int hex_digit(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static void buf_push(zb_array *buf, unsigned char c) { zb_array_push(buf, &c); }

static void buf_push_utf8(zb_array *buf, uint32_t cp) {
  if (cp < 0x80) {
    buf_push(buf, (unsigned char)cp);
  } else if (cp < 0x800) {
    buf_push(buf, (unsigned char)(0xC0 | (cp >> 6)));
    buf_push(buf, (unsigned char)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    buf_push(buf, (unsigned char)(0xE0 | (cp >> 12)));
    buf_push(buf, (unsigned char)(0x80 | ((cp >> 6) & 0x3F)));
    buf_push(buf, (unsigned char)(0x80 | (cp & 0x3F)));
  } else {
    buf_push(buf, (unsigned char)(0xF0 | (cp >> 18)));
    buf_push(buf, (unsigned char)(0x80 | ((cp >> 12) & 0x3F)));
    buf_push(buf, (unsigned char)(0x80 | ((cp >> 6) & 0x3F)));
    buf_push(buf, (unsigned char)(0x80 | (cp & 0x3F)));
  }
}

static bool parse_escape(parser *p, zb_array *buf) {
  int c = peek(p, 0);
  advance(p);
  switch (c) {
    case 'n': buf_push(buf, '\n'); return true;
    case 't': buf_push(buf, '\t'); return true;
    case 'r': buf_push(buf, '\r'); return true;
    case 'a': buf_push(buf, '\a'); return true;
    case 'b': buf_push(buf, '\b'); return true;
    case 'f': buf_push(buf, '\f'); return true;
    case 'v': buf_push(buf, '\v'); return true;
    case '\\': buf_push(buf, '\\'); return true;
    case '"': buf_push(buf, '"'); return true;
    case '\'': buf_push(buf, '\''); return true;
    case '\n': buf_push(buf, '\n'); return true;
    case 'z':
      while (p->pos < p->len && strchr(" \t\r\n\f\v", peek(p, 0))) {
        advance(p);
      }
      return true;
    case 'x': {
      int h1 = hex_digit(peek(p, 0)), h2 = hex_digit(peek(p, 1));
      if (h1 < 0 || h2 < 0) {
        fail(p, "invalid \\x escape");
        return false;
      }
      advance(p);
      advance(p);
      buf_push(buf, (unsigned char)(h1 * 16 + h2));
      return true;
    }
    case 'u': {
      if (peek(p, 0) != '{') {
        fail(p, "invalid \\u escape");
        return false;
      }
      advance(p);
      uint32_t cp = 0;
      int digits = 0;
      while (hex_digit(peek(p, 0)) >= 0 && digits < 8) {
        cp = cp * 16u + (uint32_t)hex_digit(peek(p, 0));
        advance(p);
        digits++;
      }
      if (digits == 0 || peek(p, 0) != '}' || cp > 0x10FFFFu) {
        fail(p, "invalid \\u escape");
        return false;
      }
      advance(p);
      buf_push_utf8(buf, cp);
      return true;
    }
    default:
      if (c >= '0' && c <= '9') {
        int v = c - '0';
        for (int i = 0; i < 2 && peek(p, 0) >= '0' && peek(p, 0) <= '9'; ++i) {
          v = v * 10 + (peek(p, 0) - '0');
          advance(p);
        }
        if (v > 255) {
          fail(p, "decimal escape too large");
          return false;
        }
        buf_push(buf, (unsigned char)v);
        return true;
      }
      fail(p, "invalid escape sequence");
      return false;
  }
}

static bool make_string(parser *p, zb_value *out, const char *s, size_t n) {
  char *copy = zb_arena_strndup(&p->doc->arena, s, n);
  if (!copy) {
    fail(p, "out of memory");
    return false;
  }
  out->type = ZB_VAL_STRING;
  out->as.str.ptr = copy;
  out->as.str.len = n;
  return true;
}

static bool parse_quoted(parser *p, zb_value *out) {
  int quote = peek(p, 0);
  advance(p);
  zb_array buf;
  zb_array_init(&buf, p->a, 1);
  bool ok = true;
  for (;;) {
    int c = peek(p, 0);
    if (c < 0 || c == '\n') {
      fail(p, "unfinished string");
      ok = false;
      break;
    }
    if (c == quote) {
      advance(p);
      break;
    }
    if (c == '\\') {
      advance(p);
      if (!parse_escape(p, &buf)) {
        ok = false;
        break;
      }
      continue;
    }
    buf_push(&buf, (unsigned char)c);
    advance(p);
  }
  if (ok) {
    ok = make_string(p, out, buf.len ? (const char *)buf.data : "", buf.len);
  }
  zb_array_free(&buf);
  return ok;
}

static bool parse_number(parser *p, zb_value *out, bool negative) {
  size_t start = p->pos;
  double value;
  if (peek(p, 0) == '0' && (peek(p, 1) == 'x' || peek(p, 1) == 'X')) {
    advance(p);
    advance(p);
    uint64_t v = 0;
    int digits = 0;
    while (hex_digit(peek(p, 0)) >= 0) {
      v = v * 16u + (uint64_t)hex_digit(peek(p, 0));
      advance(p);
      digits++;
    }
    if (digits == 0) {
      fail(p, "malformed number");
      return false;
    }
    value = (double)v;
  } else {
    while ((peek(p, 0) >= '0' && peek(p, 0) <= '9') || peek(p, 0) == '.') {
      advance(p);
    }
    if (peek(p, 0) == 'e' || peek(p, 0) == 'E') {
      advance(p);
      if (peek(p, 0) == '+' || peek(p, 0) == '-') {
        advance(p);
      }
      while (peek(p, 0) >= '0' && peek(p, 0) <= '9') {
        advance(p);
      }
    }
    size_t n = p->pos - start;
    char tmp[64];
    if (n == 0 || n >= sizeof tmp) {
      fail(p, "malformed number");
      return false;
    }
    memcpy(tmp, p->src + start, n);
    tmp[n] = '\0';
    char *end;
    value = strtod(tmp, &end);
    if (*end != '\0') {
      fail(p, "malformed number");
      return false;
    }
  }
  if (is_name_char(peek(p, 0))) {
    fail(p, "malformed number");
    return false;
  }
  out->type = ZB_VAL_NUMBER;
  out->as.num = negative ? -value : value;
  return true;
}

static bool parse_value(parser *p, zb_value *out);

static bool push_entry(parser *p, zb_array *entries, zb_value key, zb_value value) {
  zb_table_entry e = {key, value};
  if (!zb_array_push(entries, &e)) {
    fail(p, "out of memory");
    return false;
  }
  return true;
}

static bool parse_table(parser *p, zb_value *out) {
  if (++p->depth > ZB_LUON_MAX_DEPTH) {
    fail(p, "tables nested too deeply");
    return false;
  }
  advance(p); /* '{' */
  zb_array entries;
  zb_array_init(&entries, p->a, sizeof(zb_table_entry));
  double next_index = 1.0;
  bool ok = true;
  for (;;) {
    skip_space(p);
    if (p->failed) {
      ok = false;
      break;
    }
    if (peek(p, 0) == '}') {
      advance(p);
      break;
    }
    zb_value key, value;
    if (peek(p, 0) == '[' && long_bracket_level(p) < 0) {
      advance(p);
      skip_space(p);
      if (!parse_value(p, &key)) {
        ok = false;
        break;
      }
      if (key.type == ZB_VAL_NIL) {
        fail(p, "table index is nil");
        ok = false;
        break;
      }
      skip_space(p);
      if (peek(p, 0) != ']') {
        fail(p, "']' expected");
        ok = false;
        break;
      }
      advance(p);
      skip_space(p);
      if (peek(p, 0) != '=') {
        fail(p, "'=' expected");
        ok = false;
        break;
      }
      advance(p);
      skip_space(p);
      if (!parse_value(p, &value)) {
        ok = false;
        break;
      }
    } else {
      size_t n = name_length(p);
      size_t save_pos = p->pos;
      int save_line = p->line;
      size_t save_ls = p->line_start;
      bool keyed = false;
      if (n > 0) {
        for (size_t i = 0; i < n; ++i) {
          advance(p);
        }
        skip_space(p);
        if (peek(p, 0) == '=' && peek(p, 1) != '=') {
          keyed = true;
          if (!make_string(p, &key, p->src + save_pos, n)) {
            ok = false;
            break;
          }
          advance(p);
          skip_space(p);
        } else {
          p->pos = save_pos;
          p->line = save_line;
          p->line_start = save_ls;
        }
      }
      if (!parse_value(p, &value)) {
        ok = false;
        break;
      }
      if (!keyed) {
        key.type = ZB_VAL_NUMBER;
        key.as.num = next_index;
        next_index += 1.0;
      }
    }
    if (!push_entry(p, &entries, key, value)) {
      ok = false;
      break;
    }
    skip_space(p);
    if (peek(p, 0) == ',' || peek(p, 0) == ';') {
      advance(p);
    } else if (peek(p, 0) != '}') {
      fail(p, "'}' expected");
      ok = false;
      break;
    }
  }
  if (ok) {
    zb_table *t = zb_arena_alloc(&p->doc->arena, sizeof(*t), sizeof(void *));
    zb_table_entry *copy =
        entries.len ? zb_arena_alloc(&p->doc->arena, entries.len * sizeof(zb_table_entry), sizeof(double)) : NULL;
    if (!t || (entries.len && !copy)) {
      fail(p, "out of memory");
      ok = false;
    } else {
      if (entries.len) {
        memcpy(copy, entries.data, entries.len * sizeof(zb_table_entry));
      }
      t->entries = copy;
      t->count = entries.len;
      out->type = ZB_VAL_TABLE;
      out->as.table = t;
    }
  }
  zb_array_free(&entries);
  p->depth--;
  return ok;
}

static bool parse_value(parser *p, zb_value *out) {
  skip_space(p);
  if (p->failed) {
    return false;
  }
  int c = peek(p, 0);
  if (c == '{') {
    return parse_table(p, out);
  }
  if (c == '"' || c == '\'') {
    return parse_quoted(p, out);
  }
  int level = long_bracket_level(p);
  if (level >= 0) {
    const char *s;
    size_t n;
    if (!read_long(p, level, &s, &n)) {
      fail(p, "unfinished long string");
      return false;
    }
    return make_string(p, out, s, n);
  }
  if (c == '-') {
    advance(p);
    skip_space(p);
    if (!((peek(p, 0) >= '0' && peek(p, 0) <= '9') || peek(p, 0) == '.')) {
      fail(p, "number expected after '-'");
      return false;
    }
    return parse_number(p, out, true);
  }
  if ((c >= '0' && c <= '9') || (c == '.' && peek(p, 1) >= '0' && peek(p, 1) <= '9')) {
    return parse_number(p, out, false);
  }
  if (match_keyword(p, "nil")) {
    out->type = ZB_VAL_NIL;
    return true;
  }
  if (match_keyword(p, "true")) {
    out->type = ZB_VAL_BOOL;
    out->as.b = true;
    return true;
  }
  if (match_keyword(p, "false")) {
    out->type = ZB_VAL_BOOL;
    out->as.b = false;
    return true;
  }
  if (c < 0) {
    fail(p, "unexpected end of input");
  } else {
    fail(p, "unexpected symbol '%c'", (char)c);
  }
  return false;
}

zb_result zb_luon_parse(zb_luon_doc *doc, const zb_allocator *a, const char *text, size_t len, zb_luon_error *err) {
  memset(doc, 0, sizeof(*doc));
  zb_arena_init(&doc->arena, a, 4096);
  parser p = {text, len, 0, 1, 0, 0, doc, a, err, false};
  if (err) {
    memset(err, 0, sizeof(*err));
  }
  /* Skip a UTF-8 byte order mark. */
  if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
    p.pos = 3;
    p.line_start = 3;
  }
  skip_space(&p);
  if (!p.failed) {
    match_keyword(&p, "return");
    if (parse_value(&p, &doc->root)) {
      skip_space(&p);
      if (peek(&p, 0) == ';') {
        advance(&p);
        skip_space(&p);
      }
      if (!p.failed && p.pos < p.len) {
        fail(&p, "unexpected trailing input");
      }
    }
  }
  if (p.failed) {
    zb_arena_destroy(&doc->arena);
    doc->root.type = ZB_VAL_NIL;
    return (err && strcmp(err->message, "out of memory") == 0) ? ZB_ERR_NOMEM : ZB_ERR_PARSE;
  }
  return ZB_OK;
}

void zb_luon_free(zb_luon_doc *doc) {
  zb_arena_destroy(&doc->arena);
  doc->root.type = ZB_VAL_NIL;
}

const zb_value *zb_table_get(const zb_value *t, const char *key) {
  if (!t || t->type != ZB_VAL_TABLE) {
    return NULL;
  }
  size_t n = strlen(key);
  for (size_t i = 0; i < t->as.table->count; ++i) {
    const zb_table_entry *e = &t->as.table->entries[i];
    if (e->key.type == ZB_VAL_STRING && e->key.as.str.len == n && memcmp(e->key.as.str.ptr, key, n) == 0) {
      return &e->value;
    }
  }
  return NULL;
}

const zb_value *zb_table_index(const zb_value *t, size_t index) {
  if (!t || t->type != ZB_VAL_TABLE) {
    return NULL;
  }
  for (size_t i = 0; i < t->as.table->count; ++i) {
    const zb_table_entry *e = &t->as.table->entries[i];
    if (e->key.type == ZB_VAL_NUMBER && e->key.as.num == (double)index) {
      return &e->value;
    }
  }
  return NULL;
}

size_t zb_table_len(const zb_value *t) {
  size_t n = 0;
  while (zb_table_index(t, n + 1)) {
    n++;
  }
  return n;
}

size_t zb_table_count(const zb_value *t) { return (t && t->type == ZB_VAL_TABLE) ? t->as.table->count : 0; }

const zb_table_entry *zb_table_entry_at(const zb_value *t, size_t i) {
  if (i >= zb_table_count(t)) {
    return NULL;
  }
  return &t->as.table->entries[i];
}

bool zb_value_is(const zb_value *v, zb_value_type type) { return v && v->type == type; }

const char *zb_value_str(const zb_value *v, const char *def) {
  return zb_value_is(v, ZB_VAL_STRING) ? v->as.str.ptr : def;
}

double zb_value_num(const zb_value *v, double def) { return zb_value_is(v, ZB_VAL_NUMBER) ? v->as.num : def; }

bool zb_value_bool(const zb_value *v, bool def) { return zb_value_is(v, ZB_VAL_BOOL) ? v->as.b : def; }

const char *zb_value_type_name(zb_value_type type) {
  switch (type) {
    case ZB_VAL_NIL: return "nil";
    case ZB_VAL_BOOL: return "boolean";
    case ZB_VAL_NUMBER: return "number";
    case ZB_VAL_STRING: return "string";
    case ZB_VAL_TABLE: return "table";
  }
  return "?";
}
