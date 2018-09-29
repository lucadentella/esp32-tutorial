/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#define FRESHEN_VERSION "1.9"

#ifndef FRESHEN_H
#define FRESHEN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  MJSON_ERROR_INVALID_INPUT = -1,
  MJSON_ERROR_TOO_DEEP = -2,
};

enum mjson_tok {
  MJSON_TOK_INVALID = 0,
  MJSON_TOK_KEY = 1,
  MJSON_TOK_STRING = 11,
  MJSON_TOK_NUMBER = 12,
  MJSON_TOK_TRUE = 13,
  MJSON_TOK_FALSE = 14,
  MJSON_TOK_NULL = 15,
  MJSON_TOK_ARRAY = 91,
  MJSON_TOK_OBJECT = 123,
};
#define MJSON_TOK_IS_VALUE(t) ((t) > 10 && (t) < 20)

typedef void (*mjson_cb_t)(int ev, const char *s, int off, int len, void *ud);

#ifndef MJSON_MAX_DEPTH
#define MJSON_MAX_DEPTH 20
#endif

static int mjson_esc(int c, int esc) {
  const char *esc1 = "\b\f\n\r\t\\\"/", *esc2 = "bfnrt\\\"/";
  const char *p = strchr(esc ? esc1 : esc2, c);
  return !p ? 0 : esc ? esc2[p - esc1] : esc1[p - esc2];
}

static int mjson_pass_string(const char *s, int len) {
  for (int i = 0; i < len; i++) {
    if (s[i] == '\\' && i + 1 < len && mjson_esc(s[i + 1], 1)) {
      i++;
    } else if (s[i] == '\0') {
      return MJSON_ERROR_INVALID_INPUT;
    } else if (s[i] == '"') {
      return i;
    }
  }
  return MJSON_ERROR_INVALID_INPUT;
}

static int mjson(const char *s, int len, mjson_cb_t cb, void *ud) {
  enum { S_VALUE, S_KEY, S_COLON, S_COMMA_OR_EOO } expecting = S_VALUE;
  unsigned char nesting[MJSON_MAX_DEPTH];
  int i, depth = 0;
#define MJSONCALL(ev) \
  if (cb) cb(ev, s, start, i - start + 1, ud)

// In the ascii table, the distance between `[` and `]` is 2.
// Ditto for `{` and `}`. Hence +2 in the code below.
#define MJSONEOO()                                                     \
  do {                                                                 \
    if (c != nesting[depth - 1] + 2) return MJSON_ERROR_INVALID_INPUT; \
    depth--;                                                           \
    if (depth == 0) {                                                  \
      MJSONCALL(tok);                                                  \
      return i + 1;                                                    \
    }                                                                  \
  } while (0)

  for (i = 0; i < len; i++) {
    int start = i;
    unsigned char c = ((unsigned char *) s)[i];
    int tok = c;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    // printf("- %c [%.*s] %d %d\n", c, i, s, depth, expecting);
    switch (expecting) {
      case S_VALUE:
        if (c == '{') {
          if (depth >= (int) sizeof(nesting)) return MJSON_ERROR_TOO_DEEP;
          nesting[depth++] = c;
          expecting = S_KEY;
          break;
        } else if (c == '[') {
          if (depth >= (int) sizeof(nesting)) return MJSON_ERROR_TOO_DEEP;
          nesting[depth++] = c;
          break;
        } else if (c == ']') {
          MJSONEOO();
        } else if (c == 't' && i + 3 < len && memcmp(&s[i], "true", 4) == 0) {
          i += 3;
          tok = MJSON_TOK_TRUE;
        } else if (c == 'n' && i + 3 < len && memcmp(&s[i], "null", 4) == 0) {
          i += 3;
          tok = MJSON_TOK_NULL;
        } else if (c == 'f' && i + 4 < len && memcmp(&s[i], "false", 5) == 0) {
          i += 4;
          tok = MJSON_TOK_FALSE;
        } else if (c == '-' || ((c >= '0' && c <= '9'))) {
          char *end = NULL;
          strtod(&s[i], &end);
          if (end != NULL) i += end - &s[i] - 1;
          tok = MJSON_TOK_NUMBER;
        } else if (c == '"') {
          int n = mjson_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          i += n + 1;
          tok = MJSON_TOK_STRING;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        if (depth == 0) {
          MJSONCALL(tok);
          return i + 1;
        }
        expecting = S_COMMA_OR_EOO;
        break;

      case S_KEY:
        if (c == '"') {
          int n = mjson_pass_string(&s[i + 1], len - i - 1);
          if (n < 0) return n;
          i += n + 1;
          tok = MJSON_TOK_KEY;
          expecting = S_COLON;
        } else if (c == '}') {
          MJSONEOO();
          expecting = S_COMMA_OR_EOO;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;

      case S_COLON:
        if (c == ':') {
          expecting = S_VALUE;
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;

      case S_COMMA_OR_EOO:
        if (depth <= 0) return MJSON_ERROR_INVALID_INPUT;
        if (c == ',') {
          expecting = (nesting[depth - 1] == '{') ? S_KEY : S_VALUE;
        } else if (c == ']' || c == '}') {
          MJSONEOO();
        } else {
          return MJSON_ERROR_INVALID_INPUT;
        }
        break;
    }
    MJSONCALL(tok);
  }
  return MJSON_ERROR_INVALID_INPUT;
}

struct msjon_find_data {
  const char *path;     // Lookup json path
  int pos;              // Current path index
  int d1;               // Current depth of traversal
  int d2;               // Expected depth of traversal
  int i1;               // Index in an array
  int i2;               // Expected index in an array
  int obj;              // If the value is array/object, offset where it starts
  const char **tokptr;  // Destination
  int *toklen;          // Destination length
  int tok;              // Returned token
};

static void mjson_find_cb(int tok, const char *s, int off, int len, void *ud) {
  struct msjon_find_data *data = (struct msjon_find_data *) ud;
  // printf("--> %2x %2d %2d %2d %2d\t'%s'\t'%.*s'\t\t'%.*s'\n", tok, data->d1,
  //        data->d2, data->i1, data->i2, data->path + data->pos, off, s, len,
  //        s + off);
  if (data->tok != MJSON_TOK_INVALID) return;  // Found

  if (tok == '{') {
    if (!data->path[data->pos] && data->d1 == data->d2) data->obj = off;
    data->d1++;
  } else if (tok == '[') {
    if (data->d1 == data->d2 && data->path[data->pos] == '[') {
      data->i1 = 0;
      data->i2 = strtod(&data->path[data->pos + 1], NULL);
      if (data->i1 == data->i2) {
        data->d2++;
        data->pos += 3;
      }
    }
    if (!data->path[data->pos] && data->d1 == data->d2) data->obj = off;
    data->d1++;
  } else if (tok == ',') {
    if (data->d1 == data->d2 + 1) {
      data->i1++;
      if (data->i1 == data->i2) {
        while (data->path[data->pos] != ']') data->pos++;
        data->pos++;
        data->d2++;
      }
    }
  } else if (tok == MJSON_TOK_KEY && data->d1 == data->d2 + 1 &&
             data->path[data->pos] == '.' &&
             !memcmp(s + off + 1, &data->path[data->pos + 1], len - 2)) {
    data->d2++;
    data->pos += len - 1;
  } else if (tok == '}' || tok == ']') {
    data->d1--;
    if (!data->path[data->pos] && data->d1 == data->d2 && data->obj != -1) {
      data->tok = tok - 2;
      if (data->tokptr) *data->tokptr = s + data->obj;
      if (data->toklen) *data->toklen = off - data->obj + 1;
    }
  } else if (MJSON_TOK_IS_VALUE(tok)) {
    // printf("TOK --> %d\n", tok);
    if (data->d1 == data->d2 && !data->path[data->pos]) {
      data->tok = tok;
      if (data->tokptr) *data->tokptr = s + off;
      if (data->toklen) *data->toklen = len;
    }
  }
}

enum mjson_tok mjson_find(const char *s, int len, const char *jp,
                          const char **tokptr, int *toklen) {
  struct msjon_find_data data = {jp, 1,  0,      0,      0,
                                 0,  -1, tokptr, toklen, MJSON_TOK_INVALID};
  if (jp[0] != '$') return MJSON_TOK_INVALID;
  if (mjson(s, len, mjson_find_cb, &data) < 0) return MJSON_TOK_INVALID;
  return (enum mjson_tok) data.tok;
}

double mjson_find_number(const char *s, int len, const char *path, double def) {
  const char *p;
  int n;
  double value = def;
  if (mjson_find(s, len, path, &p, &n) == MJSON_TOK_NUMBER) {
    value = strtod(p, NULL);
  }
  return value;
}

int mjson_find_bool(const char *s, int len, const char *path, int dflt) {
  int value = dflt, tok = mjson_find(s, len, path, NULL, NULL);
  if (tok == MJSON_TOK_TRUE) value = 1;
  if (tok == MJSON_TOK_FALSE) value = 0;
  return value;
}

static int mjson_unescape(const char *s, int len, char *to, int n) {
  int i, j;
  for (i = 0, j = 0; i < len && j < n; i++, j++) {
    if (s[i] == '\\' && i + 1 < len) {
      int c = mjson_esc(s[i + 1], 0);
      if (c == 0) return -1;
      to[j] = c;
      i++;
    } else {
      to[j] = s[i];
    }
  }
  if (j >= n) return -1;
  if (n > 0) to[j] = '\0';
  return j;
}

int mjson_find_string(const char *s, int len, const char *path, char *to,
                      int n) {
  const char *p;
  int sz;
  if (mjson_find(s, len, path, &p, &sz) != MJSON_TOK_STRING) return 0;
  return mjson_unescape(p + 1, sz - 2, to, n);
}

static int mjson_base64rev(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  } else if (c >= 'a' && c <= 'z') {
    return c + 26 - 'a';
  } else if (c >= '0' && c <= '9') {
    return c + 52 - '0';
  } else if (c == '+') {
    return 62;
  } else if (c == '/') {
    return 63;
  } else {
    return 64;
  }
}

static int mjson_base64_dec(const char *src, int n, char *dst, int dlen) {
  const char *end = src + n;
  int len = 0;
  while (src + 3 < end && len < dlen) {
    int a = mjson_base64rev(src[0]), b = mjson_base64rev(src[1]),
        c = mjson_base64rev(src[2]), d = mjson_base64rev(src[3]);
    dst[len++] = (a << 2) | (b >> 4);
    if (src[2] != '=' && len < dlen) {
      dst[len++] = (b << 4) | (c >> 2);
      if (src[3] != '=' && len < dlen) {
        dst[len++] = (c << 6) | d;
      }
    }
    src += 4;
  }
  if (len < dlen) dst[len] = '\0';
  return len;
}

int mjson_find_base64(const char *s, int len, const char *path, char *to,
                      int n) {
  const char *p;
  int sz;
  if (mjson_find(s, len, path, &p, &sz) != MJSON_TOK_STRING) return 0;
  return mjson_base64_dec(p + 1, sz - 2, to, n);
}

struct mjson_out {
  int (*print)(struct mjson_out *, const char *buf, int len);
  union {
    struct {
      char *ptr;
      int size, len, overflow;
    } fixed_buf;
    char **dynamic_buf;
    FILE *fp;
  } u;
};

#define MJSON_OUT_FIXED_BUF(buf, buflen) \
  {                                      \
    mjson_print_fixed_buf, {             \
      { (buf), (buflen), 0, 0 }          \
    }                                    \
  }

#define MJSON_OUT_DYNAMIC_BUF(buf) \
  {                                \
    mjson_print_dynamic_buf, {     \
      { (char *) (buf), 0, 0, 0 }  \
    }                              \
  }

#define MJSON_OUT_FILE(fp)       \
  {                              \
    mjson_print_file, {          \
      { (char *) (fp), 0, 0, 0 } \
    }                            \
  }

int mjson_print_fixed_buf(struct mjson_out *out, const char *ptr, int len) {
  int left = out->u.fixed_buf.size - out->u.fixed_buf.len;
  if (left < len) {
    out->u.fixed_buf.overflow = 1;
    len = left;
  }
  for (int i = 0; i < len; i++) {
    out->u.fixed_buf.ptr[out->u.fixed_buf.len + i] = ptr[i];
  }
  out->u.fixed_buf.len += len;
  return len;
}

int mjson_print_dynamic_buf(struct mjson_out *out, const char *ptr, int len) {
  char *s, *buf = *out->u.dynamic_buf;
  int curlen = buf == NULL ? 0 : strlen(buf);
  if ((s = (char *) realloc(buf, curlen + len + 1)) == NULL) {
    return 0;
  } else {
    memcpy(s + curlen, ptr, len);
    s[curlen + len] = '\0';
    *out->u.dynamic_buf = s;
    return len;
  }
}

int mjson_print_file(struct mjson_out *out, const char *ptr, int len) {
  return fwrite(ptr, 1, len, out->u.fp);
}

int mjson_print_buf(struct mjson_out *out, const char *buf, int len) {
  return out->print(out, buf, len);
}

int mjson_print_int(struct mjson_out *out, int value) {
  char buf[40];
  int len = snprintf(buf, sizeof(buf), "%d", value);
  return out->print(out, buf, len);
}

int mjson_print_dbl(struct mjson_out *out, double d) {
  char buf[40];
  int n = snprintf(buf, sizeof(buf), "%g", d);
  return out->print(out, buf, n);
}

int mjson_print_str(struct mjson_out *out, const char *s, int len) {
  int n = out->print(out, "\"", 1);
  for (int i = 0; i < len; i++) {
    char c = mjson_esc(s[i], 1);
    if (c) {
      n += out->print(out, "\\", 1);
      n += out->print(out, &c, 1);
    } else {
      n += out->print(out, &s[i], 1);
    }
  }
  return n + out->print(out, "\"", 1);
}

int mjson_print_b64(struct mjson_out *out, const unsigned char *s, int n) {
  const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int len = out->print(out, "\"", 1);
  for (int i = 0; i < n; i += 3) {
    int a = s[i], b = i + 1 < n ? s[i + 1] : 0, c = i + 2 < n ? s[i + 2] : 0;
    char buf[4] = {t[a >> 2], t[(a & 3) << 4 | (b >> 4)], '=', '='};
    if (i + 1 < n) buf[2] = t[(b & 15) << 2 | (c >> 6)];
    if (i + 2 < n) buf[3] = t[c & 63];
    len += out->print(out, buf, sizeof(buf));
  }
  return len + out->print(out, "\"", 1);
}

typedef int (*mjson_printf_fn_t)(struct mjson_out *, va_list *);

int mjson_vprintf(struct mjson_out *out, const char *fmt, va_list ap) {
  int n = 0;
  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      if (fmt[i + 1] == 'Q') {
        char *buf = va_arg(ap, char *);
        n += mjson_print_str(out, buf, strlen(buf));
      } else if (memcmp(&fmt[i + 1], ".*Q", 3) == 0) {
        int len = va_arg(ap, int);
        char *buf = va_arg(ap, char *);
        n += mjson_print_str(out, buf, len);
        i += 2;
      } else if (fmt[i + 1] == 'd') {
        int val = va_arg(ap, int);
        n += mjson_print_int(out, val);
      } else if (fmt[i + 1] == 'B') {
        const char *s = va_arg(ap, int) ? "true" : "false";
        n += mjson_print_buf(out, s, strlen(s));
      } else if (fmt[i + 1] == 's') {
        char *buf = va_arg(ap, char *);
        n += mjson_print_buf(out, buf, strlen(buf));
      } else if (memcmp(&fmt[i + 1], ".*s", 3) == 0) {
        int len = va_arg(ap, int);
        char *buf = va_arg(ap, char *);
        n += mjson_print_buf(out, buf, len);
        i += 2;
      } else if (fmt[i + 1] == 'f') {
        n += mjson_print_dbl(out, va_arg(ap, double));
      } else if (fmt[i + 1] == 'V') {
        int len = va_arg(ap, int);
        const char *buf = va_arg(ap, const char *);
        n += mjson_print_b64(out, (unsigned char *) buf, len);
      } else if (fmt[i + 1] == 'M') {
        va_list tmp;
        va_copy(tmp, ap);
        mjson_printf_fn_t fn = va_arg(tmp, mjson_printf_fn_t);
        n += fn(out, &tmp);
      }
      i++;
    } else {
      n += mjson_print_buf(out, &fmt[i], 1);
    }
  }
  return n;
}

int mjson_printf(struct mjson_out *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = mjson_vprintf(out, fmt, ap);
  va_end(ap);
  return len;
}

/* Common JSON-RPC error codes */
#define FRESHEN_ERROR_INVALID -32700    /* Invalid JSON was received */
#define FRESHEN_ERROR_NOT_FOUND -32601  /* The method does not exist */
#define FRESHEN_ERROR_BAD_PARAMS -32602 /* Invalid params passed */
#define FRESHEN_ERROR_INTERNAL -32603   /* Internal JSON-RPC error */

struct freshen_method {
  const char *method;
  int method_sz;
  int (*cb)(char *, int, struct mjson_out *, void *);
  void *cbdata;
  struct freshen_method *next;
};

/*
 * Main Freshen context, stores current request information and a list of
 * exported RPC methods.
 */
struct freshen_ctx {
  struct freshen_method *methods;
  void *privdata;
  int (*sender)(char *buf, int len, void *privdata);
};

#define FRESHEN_CTX_INTIALIZER \
  { NULL, NULL, NULL }

/* Registers function fn under the given name within the given RPC context */
#define freshen_ctx_export(ctx, name, fn, ud)                               \
  do {                                                                      \
    static struct freshen_method m = {(name), sizeof(name) - 1, (fn), NULL, \
                                      NULL};                                \
    m.cbdata = (ud);                                                        \
    m.next = (ctx)->methods;                                                \
    (ctx)->methods = &m;                                                    \
  } while (0)

int freshen_ctx_notify(struct freshen_ctx *ctx, char *buf, int len) {
  return ctx->sender == NULL ? 0 : ctx->sender(buf, len, ctx->privdata);
}

static struct freshen_ctx freshen_default_context = FRESHEN_CTX_INTIALIZER;

#define freshen_export(name, fn, ud) \
  freshen_ctx_export(&freshen_default_context, (name), (fn), (ud))

#define freshen_notify(buf, len) \
  freshen_ctx_notify(&freshen_default_context, (buf), (len))

#define freshen_loop(version, pass)                                          \
  freshen_poll(&freshen_default_context, "wss://dash.freshen.cc/api/v2/rpc", \
               (version), (pass))

#ifndef FRESHEN_ENABLE_DASH
#define FRESHEN_ENABLE_DASH 1
#endif

#if !defined(FRESHEN_DECLARATIONS_ONLY)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#define FLOGI(...) ESP_LOGI("freshen", __VA_ARGS__)
#else
#define FLOGI(...)       \
  do {                   \
    printf(__VA_ARGS__); \
    putchar('\n');       \
  } while (0)
#endif
#define FLOGE FLOGI

#if defined(NOSTDLIB)
int memcmp(const void *p1, const void *p2, size_t n) {
  const unsigned char *s1 = (unsigned char *) p1, *s2 = (unsigned char *) p2;
  for (; n--; s1++, s2++) {
    if (*s1 != *s2) return *s1 - *s2;
  }
  return 0;
}

void *memmove(void *dst, void const *src, size_t n) {
  unsigned char *dp = dst;
  unsigned char const *sp = src;
  if (dp < sp) {
    while (n-- > 0) *dp++ = *sp++;
  } else {
    dp += n;
    sp += n;
    while (n-- > 0) *--dp = *--sp;
  }

  return dst;
}

void *memset(void *p, int c, size_t n) {
  unsigned char *s = (unsigned char *) p;
  for (; n--; s++) *s = c;
  return p;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') n++;
  return n;
}
#endif

#ifdef ESP_PLATFORM

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <unistd.h>

#define FRESHEN_TAG "freshen"
#define FRESHEN_NVS_NAMESPACE "freshen"
#define ROLLBACK_KV_KEY "__rollback"
#define FRESHEN_OTA_ENABLE

static struct {
  int can_rollback;
  const esp_partition_t *update_partition;
  esp_ota_handle_t update_handle;
} s_ota;

static int freshen_kv_set(const char *key, const char *value) {
  nvs_handle nvs_handle;
  esp_err_t err;
  err = nvs_open(FRESHEN_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_open: err=%d", err);
    return -1;
  }
  err = nvs_set_str(nvs_handle, key, value == NULL ? "" : value);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_set_str: err=%d", err);
    return -1;
  }
  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_commit: err=%d", err);
    return -1;
  }
  nvs_close(nvs_handle);
  return 0;
}

static const char *freshen_kv_get(const char *key) {
  nvs_handle nvs_handle = 0;
  size_t size;
  char *value = NULL;
  esp_err_t err;

  if ((err = nvs_open(FRESHEN_NVS_NAMESPACE, NVS_READONLY, &nvs_handle)) !=
      ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_open: err=%d", err);
  } else if ((err = nvs_get_str(nvs_handle, key, NULL, &size)) != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_get_str: err=%d", err);
  } else if ((value = (char *) calloc(1, size)) == NULL) {
    ESP_LOGE(FRESHEN_TAG, "OOM: value is %zu", size);
  } else if ((err = nvs_get_str(nvs_handle, key, value, &size)) != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "nvs_get_str: err=%d", err);
  }
  nvs_close(nvs_handle);

  return value;
}

static int freshen_ota_init(struct freshen_ctx *ctx) {
  const char *label = freshen_kv_get(ROLLBACK_KV_KEY);
  if (label == NULL) {
    ESP_LOGI(FRESHEN_TAG,
             "no rollback partition is given, booting up normally");
    return 0;
  }
  const esp_partition_t *p = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
  if (p == NULL) {
    ESP_LOGI(FRESHEN_TAG, "can't find partition %s for rollback", label);
    return 0;
  }
  ESP_LOGI(FRESHEN_TAG, "use partition %s for rollback", label);
  freshen_kv_set(ROLLBACK_KV_KEY, NULL);
  esp_ota_set_boot_partition(p);
  s_ota.can_rollback = 1;
  return 0;
  (void) ctx;
}

static int freshen_ota_begin(struct freshen_ctx *ctx) {
  if (s_ota.update_partition != NULL) {
    ESP_LOGE(FRESHEN_TAG, "another OTA is already in-progress");
    return -1;
  }
  s_ota.update_partition = esp_ota_get_next_update_partition(NULL);
  ESP_LOGI(FRESHEN_TAG, "Starting OTA. update_partition=%p",
           s_ota.update_partition);
  esp_err_t err = esp_ota_begin(s_ota.update_partition, OTA_SIZE_UNKNOWN,
                                &s_ota.update_handle);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "esp_ota_begin failed, err=%#x", err);
    return -1;
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_write(struct freshen_ctx *ctx, void *buf, size_t bufsz) {
  esp_err_t err = esp_ota_write(s_ota.update_handle, buf, bufsz);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "esp_ota_write failed, err=%#x", err);
    return -1;
  }
  ESP_LOGD(FRESHEN_TAG, "written %d bytes", bufsz);
  return 0;
  (void) ctx;
}

static int freshen_ota_end(struct freshen_ctx *ctx, int success) {
  const esp_partition_t *p = s_ota.update_partition;
  esp_err_t err = esp_ota_end(s_ota.update_handle);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "err=0x%x", err);
    return -1;
  }
  s_ota.update_partition = NULL;
  if (success) {
    const esp_partition_t *rollback = esp_ota_get_running_partition();
    ESP_LOGI(FRESHEN_TAG, "use partition %s for the next boot",
             rollback->label);
    if (freshen_kv_set(ROLLBACK_KV_KEY, rollback->label) < 0) return -1;
    esp_err_t err = esp_ota_set_boot_partition(p);
    if (err != ESP_OK) {
      ESP_LOGE(FRESHEN_TAG, "esp_ota_set_boot_partition: err=0x%d", err);
      return -1;
    }
    ESP_LOGI(FRESHEN_TAG, "restarting...");
    sleep(2);
    esp_restart();
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_commit(struct freshen_ctx *ctx) {
  if (!s_ota.can_rollback) return 0;
  const esp_partition_t *p = esp_ota_get_running_partition();
  esp_err_t err = esp_ota_set_boot_partition(p);
  if (err != ESP_OK) {
    ESP_LOGE(FRESHEN_TAG, "esp_ota_set_boot_partition: err=0x%d", err);
    return -1;
  }
  s_ota.can_rollback = 0;
  return 0;
  (void) ctx;
}

#elif(defined(__linux__) || defined(__APPLE__))

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FRESHEN_OTA_FILE "./tmp"
#define FRESHEN_OTA_ENABLE

#ifdef __APPLE__
/* https://groups.google.com/forum/#!topic/comp.unix.programmer/JlTIncgY-vg */
int sigtimedwait(const sigset_t *set, siginfo_t *info,
                 const struct timespec *timeout) {
  struct timespec elapsed = {0, 0}, rem;
  sigset_t pending;
  int signo;
  long ns;

  do {
    sigpending(&pending); /* doesn't clear pending queue */
    for (signo = 1; signo < NSIG; signo++) {
      if (sigismember(set, signo) && sigismember(&pending, signo)) {
        if (info) {
          memset(info, 0, sizeof *info);
          info->si_signo = signo;
        }

        return signo;
      }
    }
    ns = 200000000L; /* 2/10th second */
    nanosleep(&(struct timespec){0, ns}, &rem);
    ns -= rem.tv_nsec;
    elapsed.tv_sec += (elapsed.tv_nsec + ns) / 1000000000L;
    elapsed.tv_nsec = (elapsed.tv_nsec + ns) % 1000000000L;
  } while (elapsed.tv_sec < timeout->tv_sec ||
           (elapsed.tv_sec == timeout->tv_sec &&
            elapsed.tv_nsec < timeout->tv_nsec));
  errno = EAGAIN;
  return -1;
}
#endif

static struct {
  int can_rollback;
  FILE *ota_file;
} s_ota = {0, NULL};

static const char *freshen_arg(int n) {
#if defined(__linux__)
  static char cmdline[8192];
  FILE *f = fopen("/proc/self/cmdline", "r");
  if (f == NULL) {
    return NULL;
  }
  int sz = fread(cmdline, 1, sizeof(cmdline), f);
  if (sz < 0) {
    return NULL;
  }
  fclose(f);
  int i = 0;
  char *arg = &cmdline[0];
  for (i = 0; i < n && *arg; i++) {
    arg = arg + strlen(arg) + 1;
  }
  if (*arg == '\0') {
    return NULL;
  }
  return arg;
#elif defined(__APPLE__)
  extern int *_NSGetArgc(void);
  extern char ***_NSGetArgv(void);
  if (n < *_NSGetArgc()) {
    return (*_NSGetArgv())[n];
  }
  return NULL;
#endif
}

static void freshen_ota_restart(void) {
  FLOGI("called system restart..");
  int exit_code = 0;
  struct stat st;
  if (strcmp(freshen_arg(0), FRESHEN_OTA_FILE) != 0 &&
      stat(FRESHEN_OTA_FILE, &st) == 0) {
    FLOGI("update is available");
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
      FLOGE("sigprocmask(): errno=%d", errno);
      goto error;
    }
    int pid = fork();
    FLOGI("pid=%d, ppid=%d", pid, getppid());
    if (pid < 0) {
      FLOGE("fork(): errno=%d", errno);
      goto error;
    }
    if (pid == 0) {
      int argc = 0;
      for (; freshen_arg(argc) != NULL; argc++)
        ;
      char **argv = (char **) calloc(argc + 1, sizeof(char *));
      if (argv == NULL) {
        exit(EXIT_FAILURE);
      }
      for (int i = 0; i < argc + 1; i++) {
        argv[i] = (char *) freshen_arg(i);
      }
      argv[0] = FRESHEN_OTA_FILE;
      chmod(FRESHEN_OTA_FILE, 0755);
      execv(FRESHEN_OTA_FILE, argv);
      exit(EXIT_FAILURE);
    }

    siginfo_t sig;
    int status;
    struct timespec t;
    t.tv_sec = 10;
    t.tv_nsec = 0;
    for (;;) {
      if (sigtimedwait(&mask, &sig, &t) < 0) {
        if (errno == EINTR) {
          FLOGI("interruped by another signal, continue");
          continue;
        } else if (errno == EAGAIN) {
          FLOGE("timeout, killing child process");
          kill(pid, SIGKILL);
        } else {
          FLOGE("sigtimedwait() failed: errno=%d", errno);
        }
        break;
      }
      if (sig.si_signo == SIGUSR1) {
        FLOGI("child process succeeded!");
        rename(FRESHEN_OTA_FILE, freshen_arg(0));
      } else {
        FLOGI("child process exited!");
      }
      break;
    }
    if (waitpid(pid, &status, 0) < 0) {
      FLOGE("waitpid(): errno=%d", errno);
      return;
    }
    exit_code = (WEXITSTATUS(status));
    FLOGI("child process exited with %d", exit_code);
    goto error;
  }
  return;
error:
  unlink(FRESHEN_OTA_FILE);
}

static int freshen_ota_init(struct freshen_ctx *ctx) {
  FLOGI("argv0=%s", freshen_arg(0));
  if (strcmp(freshen_arg(0), FRESHEN_OTA_FILE) == 0) {
    s_ota.can_rollback = 1;
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_begin(struct freshen_ctx *ctx) {
  FLOGI("ctx: %p", (void *) ctx);
  if (s_ota.ota_file != NULL) {
    FLOGE("another OTA process is in-progress");
    return -1;
  }
  s_ota.ota_file = fopen(FRESHEN_OTA_FILE, "wb");
  if (s_ota.ota_file == NULL) {
    FLOGE("failed to open temporary file");
    return -1;
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_end(struct freshen_ctx *ctx, int success) {
  FLOGI("ctx: %p, success=%d", (void *) ctx, success);
  if (s_ota.ota_file == NULL) {
    FLOGE("ota_write: OTA process is not started");
    return -1;
  }
  fclose(s_ota.ota_file);
  s_ota.ota_file = NULL;
  if (!success) {
    FLOGI("not succeeded, remove temporary file");
    unlink(FRESHEN_OTA_FILE);
  } else {
    freshen_ota_restart();
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_write(struct freshen_ctx *ctx, void *buf, size_t bufsz) {
  FLOGI("ctx: %p, bufsz=%zu", (void *) ctx, bufsz);
  if (s_ota.ota_file == NULL) {
    FLOGE("OTA process is not started");
    return -1;
  }
  if (fwrite(buf, bufsz, 1, s_ota.ota_file) != 1) {
    FLOGE("failed to write chunk");
    return -1;
  }
  return 0;
  (void) ctx;
}

static int freshen_ota_commit(struct freshen_ctx *ctx) {
  if (!s_ota.can_rollback) {
    return 0;
  }
  FLOGI("ctx: %p, argv0=%s", (void *) ctx, freshen_arg(0));
  s_ota.can_rollback = 0;
  return kill(getppid(), SIGUSR1);
  (void) ctx;
}
#else
#define freshen_ota_init(x)
#endif

int freshen_ctx_process(struct freshen_ctx *ctx, char *req, int req_sz) {
  const char *id = NULL, *params = NULL;
  char method[50];
  int id_sz = 0, method_sz = 0, params_sz = 0, code = FRESHEN_ERROR_NOT_FOUND;
  struct freshen_method *m;

  /* Method must exist and must be a string. */
  if ((method_sz = mjson_find_string(req, req_sz, "$.method", method,
                                     sizeof(method))) <= 0) {
    return FRESHEN_ERROR_INVALID;
  }

  /* id and params are optional. */
  mjson_find(req, req_sz, "$.id", &id, &id_sz);
  mjson_find(req, req_sz, "$.params", &params, &params_sz);

  char *res = NULL, *frame = NULL;
  struct mjson_out rout = MJSON_OUT_DYNAMIC_BUF(&res);
  struct mjson_out fout = MJSON_OUT_DYNAMIC_BUF(&frame);

  for (m = ctx->methods; m != NULL; m = m->next) {
    if (m->method_sz == method_sz && !memcmp(m->method, method, method_sz)) {
      if (params == NULL) params = "";
      int code = m->cb((char *) params, params_sz, &rout, m->cbdata);
      if (id == NULL) {
        /* No id, not sending any reply. */
        free(res);
        return code;
      } else if (code == 0) {
        mjson_printf(&fout, "{%Q:%.*s,%Q:%s}", "id", id_sz, id, "result",
                     res == NULL ? "null" : res);
      } else {
        mjson_printf(&fout, "{%Q:%.*s,%Q:{%Q:%d,%Q:%s}}", "id", id_sz, id,
                     "error", "code", code, "message",
                     res == NULL ? "null" : res);
      }
      break;
    }
  }
  if (m == NULL) {
    mjson_printf(&fout, "{%Q:%.*s,%Q:{%Q:%d,%Q:%Q}}", "id", id_sz, id, "error",
                 "code", code, "message", "method not found");
  }
  ctx->sender(frame, strlen(frame), ctx->privdata);
  free(frame);
  free(res);
  return code;
}
#if defined(__APPLE__) || defined(__linux__)
#define FRESHEN_FS_ENABLE
#endif

static int info(char *args, int len, struct mjson_out *out, void *userdata) {
#if defined(__APPLE__)
  const char *arch = "darwin";
#elif defined(__linux__)
  const char *arch = "linux";
#elif defined(ESP_PLATFORM)
  const char *arch = "esp32";
#elif defined(ESP8266) || defined(MG_ESP8266)
  const char *arch = "esp8266";
#elif defined(MBED_LIBRARY_VERSION)
  const char *arch = "mbedOS";
#else
  const char *arch = "unknown";
#endif

#if !defined(FRESHEN_APP)
#define FRESHEN_APP "posix_device"
#endif

  (void) args;
  (void) len;
  mjson_printf(out, "{%Q:%Q, %Q:%Q, %Q:%Q, %Q:%Q}", "fw_version", userdata,
               "arch", arch, "fw_id", __DATE__ " " __TIME__, "app",
               FRESHEN_APP);
  return 0;
}

static int rpclist(char *in, int in_len, struct mjson_out *out, void *ud) {
  struct freshen_ctx *ctx = (struct freshen_ctx *) ud;
  mjson_print_buf(out, "[", 1);
  for (struct freshen_method *m = ctx->methods; m != NULL; m = m->next) {
    if (m != ctx->methods) mjson_print_buf(out, ",", 1);
    mjson_print_str(out, m->method, strlen(m->method));
  }
  mjson_print_buf(out, "]", 1);
  (void) in;
  (void) in_len;
  return 0;
}

#if defined(FRESHEN_OTA_ENABLE)
#include <stdio.h>
#include <stdlib.h>
/*
 * Common OTA code
 */
static int freshen_rpc_ota_begin(char *in, int in_len, struct mjson_out *out,
                                 void *userdata) {
  struct freshen_ctx *ctx = (struct freshen_ctx *) userdata;
  int r = freshen_ota_begin(ctx);
  mjson_printf(out, "%s", r == 0 ? "true" : "false");
  return r;
  (void) in;
  (void) in_len;
}

static int freshen_rpc_ota_end(char *in, int in_len, struct mjson_out *out,
                               void *userdata) {
  struct freshen_ctx *ctx = (struct freshen_ctx *) userdata;
  int success = mjson_find_number(in, in_len, "$.success", -1);
  if (success < 0) {
    mjson_printf(out, "%Q", "bad args");
    return FRESHEN_ERROR_BAD_PARAMS;
  } else if (freshen_ota_end(ctx, success) != 0) {
    mjson_printf(out, "%Q", "failed");
    return 500;
  } else {
    mjson_printf(out, "%s", "true");
    return 0;
  }
}

static int freshen_rpc_ota_write(char *in, int len, struct mjson_out *out,
                                 void *userdata) {
  struct freshen_ctx *ctx = (struct freshen_ctx *) userdata;
  char *p;
  int n, result = 0;
  if (mjson_find(in, len, "$", (const char **) &p, &n) != MJSON_TOK_STRING) {
    mjson_printf(out, "%Q", "expecting base64 encoded data");
    result = FRESHEN_ERROR_BAD_PARAMS;
  } else {
    int dec_len = mjson_base64_dec(p, n, p, n);
    if (freshen_ota_write(ctx, p, dec_len) != 0) {
      mjson_printf(out, "%Q", "write failed");
      result = 500;
    } else {
      mjson_printf(out, "%s", "true");
    }
  }
  return result;
}
#endif

#if defined(FRESHEN_FS_ENABLE)
#include <dirent.h>
static int fslist(char *in, int in_len, struct mjson_out *out, void *ud) {
  DIR *dirp;
  mjson_print_buf(out, "[", 1);
  if ((dirp = opendir(".")) != NULL) {
    struct dirent *dp;
    int i = 0;
    while ((dp = readdir(dirp)) != NULL) {
      /* Do not show current and parent dirs */
      if (strcmp((const char *) dp->d_name, ".") == 0 ||
          strcmp((const char *) dp->d_name, "..") == 0) {
        continue;
      }
      if (i > 0) mjson_print_buf(out, ",", 1);
      mjson_print_str(out, dp->d_name, strlen(dp->d_name));
      i++;
    }
    closedir(dirp);
  }
  mjson_print_buf(out, "]", 1);
  (void) ud;
  (void) in;
  (void) in_len;
  return 0;
}

static int fsremove(char *in, int in_len, struct mjson_out *out, void *ud) {
  char fname[50];
  int result = 0;
  if (mjson_find_string(in, in_len, "$.filename", fname, sizeof(fname)) <= 0) {
    mjson_printf(out, "%Q", "filename is missing");
    result = FRESHEN_ERROR_BAD_PARAMS;
  } else if (remove(fname) != 0) {
    mjson_printf(out, "%Q", "remove() failed");
    result = -1;
  } else {
    mjson_printf(out, "%s", "true");
  }
  (void) ud;
  return result;
}

static int fsrename(char *in, int in_len, struct mjson_out *out, void *ud) {
  char src[50], dst[50];
  int result = 0;
  if (mjson_find_string(in, in_len, "$.src", src, sizeof(src)) <= 0 ||
      mjson_find_string(in, in_len, "$.dst", dst, sizeof(dst)) <= 0) {
    mjson_printf(out, "%Q", "src and dst are required");
    result = FRESHEN_ERROR_BAD_PARAMS;
  } else if (rename(src, dst) != 0) {
    mjson_printf(out, "%Q", "rename() failed");
    result = -1;
  } else {
    mjson_printf(out, "%s", "true");
  }
  (void) ud;
  return result;
}

static int fsget(char *in, int in_len, struct mjson_out *out, void *ud) {
  char fname[50], *chunk = NULL;
  int offset = mjson_find_number(in, in_len, "$.offset", 0);
  int len = mjson_find_number(in, in_len, "$.len", 512);
  int result = 0;
  FILE *fp = NULL;
  if (mjson_find_string(in, in_len, "$.filename", fname, sizeof(fname)) <= 0) {
    mjson_printf(out, "%Q", "filename is required");
    result = FRESHEN_ERROR_BAD_PARAMS;
  } else if ((chunk = malloc(len)) == NULL) {
    mjson_printf(out, "%Q", "chunk alloc failed");
    result = -1;
  } else if ((fp = fopen(fname, "rb")) == NULL) {
    mjson_printf(out, "%Q", "fopen failed");
    result = -2;
  } else {
    fseek(fp, offset, SEEK_SET);
    int n = fread(chunk, 1, len, fp);
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    mjson_printf(out, "{%Q:%V,%Q:%d}", "data", n, chunk, "left",
                 size - (n + offset));
  }
  if (chunk != NULL) free(chunk);
  if (fp != NULL) fclose(fp);
  (void) ud;
  return result;
}

static int fsput(char *in, int in_len, struct mjson_out *out, void *ud) {
  char fname[50], *data = NULL;
  FILE *fp = NULL;
  int n, result = 0;
  int append = mjson_find_bool(in, in_len, "$.append", 0);
  if (mjson_find(in, in_len, "$.data", (const char **) &data, &n) !=
          MJSON_TOK_STRING ||
      mjson_find_string(in, in_len, "$.filename", fname, sizeof(fname)) <= 0) {
    mjson_printf(out, "%Q", "data and filename are required");
    result = FRESHEN_ERROR_BAD_PARAMS;
  } else if ((fp = fopen(fname, append ? "ab" : "wb")) == NULL) {
    mjson_printf(out, "%Q", "fopen failed");
    result = 500;
  } else {
    /* Decode in-place */
    int dec_len = mjson_base64_dec(data + 1, n - 2, data, n);
    if ((int) fwrite(data, 1, dec_len, fp) != dec_len) {
      mjson_printf(out, "%Q", "write failed");
      result = 500;
    } else {
      mjson_printf(out, "{%Q:%d}", "written", dec_len);
    }
  }
  if (fp != NULL) fclose(fp);
  (void) ud;
  return result;
}
#endif

static void freshen_rpc_init(struct freshen_ctx *ctx, const char *version) {
  freshen_ctx_export(ctx, "Sys.GetInfo", info, (void *) version);
  freshen_ctx_export(ctx, "RPC.List", rpclist, ctx);

#if defined(FRESHEN_FS_ENABLE)
  freshen_ctx_export(ctx, "FS.List", fslist, ctx);
  freshen_ctx_export(ctx, "FS.Remove", fsremove, ctx);
  freshen_ctx_export(ctx, "FS.Rename", fsrename, ctx);
  freshen_ctx_export(ctx, "FS.Get", fsget, ctx);
  freshen_ctx_export(ctx, "FS.Put", fsput, ctx);
#endif

#if defined(FRESHEN_OTA_ENABLE)
  freshen_ctx_export(ctx, "OTA.Begin", freshen_rpc_ota_begin, ctx);
  freshen_ctx_export(ctx, "OTA.Write", freshen_rpc_ota_write, ctx);
  freshen_ctx_export(ctx, "OTA.End", freshen_rpc_ota_end, ctx);
#endif
}

#if FRESHEN_ENABLE_DASH && defined(MG_ENABLE_SSL)
#include "mongoose.h"

#define PROTO "dash.freshen.cc"
#define ORIGIN "Origin: https://freshen.cc\r\n"
#define RECONNECTION_INTERVAL_SECONDS 3.0

#ifndef OUT_BUF_SIZE
#define OUT_BUF_SIZE 1024
#endif

#ifndef QUEUE_BUF_SIZE
#define QUEUE_BUF_SIZE OUT_BUF_SIZE
#endif

struct privdata {
  struct mg_mgr mgr;
  struct mg_connection *c;
  double disconnection_time;
  struct freshen_ctx *ctx;
};

static void freshen_ws_handler(struct mg_connection *c, int ev, void *arg,
                               void *userdata) {
  struct privdata *pd = (struct privdata *) userdata;
  // FLOGI("%p %d %p", c, ev, arg);
  switch (ev) {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
      // Successfully connected. Clear off rollback timer and commit
      mg_set_timer(c, 0);
      //   if (ctx->ota.can_rollback) freshen_ota_commit(&ctx->ota);
      break;
    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = arg;
      int len = wm->size;
      char *out = (char *) malloc(OUT_BUF_SIZE);
      if (out == NULL) break;
      while (len > 0 && wm->data[len - 1] != '}') len--;
      out[OUT_BUF_SIZE - 1] = '\0';
      freshen_core_process(pd->ctx, wm->data, len, out, OUT_BUF_SIZE - 1);
      if (strlen(out) > 0) {
        mg_printf_websocket_frame(c, WEBSOCKET_OP_TEXT, "%s", out);
      }
      FLOGI("GOT RPC: [%.*s] [%s]", len, wm->data, out);
      free(out);
      break;
    }
    case MG_EV_CLOSE:
      FLOGI("%s", "disconnected");
      mg_set_timer(c, 0);
      pd->c = NULL;
      pd->disconnection_time = mg_time();
      break;
  }
}

static void freshen_reconnect(struct privdata *pd, const char *url,
                              const char *token) {
  char buf[200];
  snprintf(buf, sizeof(buf), "Authorization: Bearer %s\r\n" ORIGIN, token);
  pd->c = mg_connect_ws(&pd->mgr, freshen_ws_handler, pd, url, PROTO, buf);
}

static void freshen_poll(struct freshen_ctx *ctx, const char *url,
                         const char *version, const char *token) {
  struct privdata *pd = (struct privdata *) ctx->privdata;
  if (pd == NULL) {
    pd = (struct privdata *) calloc(1, sizeof(*pd));
    ctx->privdata = pd;
    pd->ctx = ctx;
    mg_mgr_init(&pd->mgr, pd);
    freshen_ota_init(ctx);
    freshen_rpc_init(ctx, version);
  }
  if (pd == NULL) {
    FLOGE("OOM %d bytes", (int) sizeof(*pd));
  } else if (pd->c == NULL) {
    freshen_reconnect(pd, url, token);
  } else {
    mg_mgr_poll(&pd->mgr, 0);
  }
}

static void freshen_poll_dash(struct freshen_ctx *ctx, const char *version,
                              const char *token) {
  freshen_poll(ctx, "wss://dash.freshen.cc/api/v1/rpc", version, token);
}
#endif
#if FRESHEN_ENABLE_DASH && !defined(MG_ENABLE_SSL)

#define FRESHEN_ENABLE_MBEDTLS

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__) || defined(ESP_PLATFORM)
#define FRESHEN_ENABLE_SOCKET
#include <signal.h>
#include <unistd.h>
#endif

#if defined(FRESHEN_ENABLE_SOCKET)
#include <netdb.h>
#include <sys/socket.h>
#endif

#if defined(MBED_LIBRARY_VERSION)
#include "mbed.h"
#endif

#if defined(FRESHEN_ENABLE_MBEDTLS)
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#endif

struct conn {
  int sock;
  bool is_tls;

#if defined(MBED_LIBRARY_VERSION)
  TCPSocket *mbedsock;
#endif

#if defined(FRESHEN_ENABLE_MBEDTLS)
  mbedtls_net_context server_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
#endif
};

static void freshen_net_init_conn(struct conn *c) {
  memset(c, 0, sizeof(*c));
  c->sock = -1;
}

static bool freshen_net_is_disconnected(const struct conn *c) {
#if defined(MBED_LIBRARY_VERSION)
  return c->mbedsock == NULL;
#else
  return c->sock == -1;
#endif
}

static int freshen_net_recv(struct conn *c, void *buf, size_t len) {
  int n = 0;

#if defined(FRESHEN_ENABLE_SOCKET)
  /* If socket has no data, return immediately */
  struct timeval tv = {1, 0};
  fd_set rset;
  FD_ZERO(&rset);
  FD_SET(c->sock, &rset);
  n = select(c->sock + 1, &rset, NULL, NULL, &tv);
  if (n <= 0) return n;
#endif

#if defined(FRESHEN_ENABLE_MBEDTLS)
  if (c->is_tls) {
    n = mbedtls_ssl_read(&c->ssl, (unsigned char *) buf, len);
  } else
#endif
  {
#if defined(FRESHEN_ENABLE_SOCKET)
    n = recv(c->sock, buf, len, 0);
#endif
  }
  return n <= 0 ? -1 : n;
}

static int freshen_net_send(struct conn *c, const void *buf, size_t len) {
#if defined(FRESHEN_ENABLE_MBEDTLS)
  if (c->is_tls) {
    return mbedtls_ssl_write(&c->ssl, (const unsigned char *) buf, len);
  } else
#endif
  {
#if defined(FRESHEN_ENABLE_SOCKET)
    return send(c->sock, buf, len, 0);
#endif
  }
}

static void freshen_net_close(struct conn *c) {
#if defined(MBED_LIBRARY_VERSION)
  if (c->mbedsock != NULL) c->mbedsock->close();
  c->mbedsock = NULL;
  FLOGI("closed mbedsock");
#endif

#if defined(FRESHEN_ENABLE_MBEDTLS)
  if (c->is_tls) {
#if defined(FRESHEN_ENABLE_SOCKET)
    mbedtls_net_free(&c->server_fd);
#endif
    mbedtls_ssl_free(&c->ssl);
    mbedtls_ssl_config_free(&c->conf);
    mbedtls_ctr_drbg_free(&c->drbg);
    mbedtls_entropy_free(&c->entropy);
    mbedtls_x509_crt_free(&c->cacert);
  }
#endif

  if (c->sock != -1) close(c->sock);
  c->sock = -1;
}

#if defined(MBED_LIBRARY_VERSION)
NetworkInterface *freshen_net;

static int tls_net_recv(void *ctx, unsigned char *buf, size_t len) {
  TCPSocket *socket = static_cast<TCPSocket *>(ctx);
  int n = socket->recv(buf, len);
  return n;
}

static int tls_net_send(void *ctx, const unsigned char *buf, size_t len) {
  TCPSocket *socket = static_cast<TCPSocket *>(ctx);
  int n = socket->send(buf, len);
  return n;
}

void mbedtls_net_init(mbedtls_net_context *ctx) {
  (void) ctx;
}

void mbedtls_net_free(mbedtls_net_context *ctx) {
  (void) ctx;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen) {
  memset(output, 0, len);
  return 0;
}
#endif

static void freshen_parse_url(const char *url, char *proto, char *host,
                              char *port, char *uri) {
  proto[0] = host[0] = port[0] = uri[0] = '\0';
  if (sscanf(url, "%9[^:]://%99[^:]:%9[0-9]%19s", proto, host, port, uri) !=
          4 &&
      sscanf(url, "%9[^:]://%99[^/]%19s", proto, host, uri) < 2) {
    proto[0] = host[0] = port[0] = uri[0] = '\0';
  }
  if (port[0] == '\0') strcpy(port, "443");
}

static bool freconnect(const char *url, struct conn *c) {
#if defined(FRESHEN_ENABLE_SOCKET)
  struct sockaddr_in sin;
  struct hostent *he;
#endif
  char proto[10], host[100], port[10], uri[20];
  int sock;

  if (!freshen_net_is_disconnected(c)) return true;

  sock = c->sock = -1;
  freshen_parse_url(url, proto, host, port, uri);
  c->is_tls = strcmp(proto, "wss") == 0 || strcmp(proto, "mqtts") == 0;
  FLOGI("dst [%s][%s][%s][%s]", proto, host, port, uri);

#if defined(FRESHEN_ENABLE_SOCKET)
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons((uint16_t) atoi(port));
  if ((he = gethostbyname(host)) == NULL) {
    FLOGE("gethostbyname(%s): %d", host, errno);
  } else if (!memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr))) {
  } else if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    FLOGE("socket: %d", errno);
  } else if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
    FLOGE("connect: %d", errno);
    close(sock);
  } else if (!c->is_tls) {
    c->sock = sock;
    return true;
  } else {
#elif defined(MBED_LIBRARY_VERSION)
  if (freshen_net == NULL) {
    FLOGE("freshen_net pointer is not set");
  } else if ((c->mbedsock = new TCPSocket(freshen_net)) == NULL) {
    FLOGE("cant create tcp socket");
  } else if (c->mbedsock->connect(host, atoi(port)) != NSAPI_ERROR_OK) {
    FLOGE("mbed connect(%s:%d): %d", host, atoi(port), errno);
  } else {
#endif
#if defined(FRESHEN_ENABLE_MBEDTLS)

#if !defined(FRESHEN_CA_PEM)
#define FRESHEN_CA_PEM                                                 \
  "-----BEGIN CERTIFICATE-----\n"                                      \
  "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n" \
  "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
  "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n" \
  "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n" \
  "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" \
  "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n" \
  "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n" \
  "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n" \
  "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n" \
  "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n" \
  "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n" \
  "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n" \
  "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n" \
  "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n" \
  "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n" \
  "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n" \
  "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n" \
  "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n" \
  "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n" \
  "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n" \
  "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n" \
  "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n" \
  "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n" \
  "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n" \
  "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n"                                 \
  "-----END CERTIFICATE-----"
#endif

    const char *ca_pem = FRESHEN_CA_PEM;
    int res;
    mbedtls_ssl_init(&c->ssl);
    mbedtls_ssl_config_init(&c->conf);
    mbedtls_entropy_init(&c->entropy);
    mbedtls_ctr_drbg_init(&c->drbg);
    mbedtls_x509_crt_init(&c->cacert);
    mbedtls_ctr_drbg_seed(&c->drbg, mbedtls_entropy_func, &c->entropy, 0, 0);
    mbedtls_ssl_config_defaults(&c->conf, MBEDTLS_SSL_IS_CLIENT,
                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->drbg);
    res = mbedtls_x509_crt_parse(&c->cacert, (const unsigned char *) ca_pem,
                                 strlen(ca_pem) + 1); /* must contain  \0 */
    if (res != 0) FLOGE("crt_parse: -%#x", -res);
    mbedtls_ssl_conf_ca_chain(&c->conf, &c->cacert, NULL);
    mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_REQUIRED);

    res = mbedtls_ssl_setup(&c->ssl, &c->conf);
    if (res != 0) FLOGE("ssl_setup: -%#x", -res);
    mbedtls_ssl_set_hostname(&c->ssl, host);
#if defined(FRESHEN_ENABLE_SOCKET)
    mbedtls_net_init(&c->server_fd);
    c->server_fd.fd = c->sock = sock;
    mbedtls_ssl_set_bio(&c->ssl, &c->server_fd, mbedtls_net_send,
                        mbedtls_net_recv, NULL);
#elif defined(MBED_LIBRARY_VERSION)
    mbedtls_ssl_set_bio(&c->ssl, static_cast<void *>(c->mbedsock), tls_net_send,
                        tls_net_recv, NULL);
#endif
    if ((res = mbedtls_ssl_handshake(&c->ssl)) == 0) {
      return true;
    }
    FLOGE("handshake error: -%#x", -res);
#else
    FLOGE("TLS support is not enabled");
#endif
  }

  freshen_net_close(c);
  return false;
}

static void freshen_sleep(int seconds) {
#if defined(MBED_LIBRARY_VERSION)
  wait(seconds);
#else
  sleep(seconds);
#endif
}
#endif
#if FRESHEN_ENABLE_DASH && !defined(MG_ENABLE_SSL)
// WEBSOCKET
#define WEBSOCKET_OP_CONTINUE 0
#define WEBSOCKET_OP_TEXT 1
#define WEBSOCKET_OP_BINARY 2
#define WEBSOCKET_OP_CLOSE 8
#define WEBSOCKET_OP_PING 9
#define WEBSOCKET_OP_PONG 10

#define FLAGS_MASK_FIN (1 << 7)
#define FLAGS_MASK_OP 0x0f

struct ws_msg {
  int header_len;
  int data_len;
  int flags;
};

struct privdata {
  struct conn conn;
  char *in;
  int in_len;
  bool handshake_sent;
  bool is_ws;
};

#ifndef IN_BUF_SIZE
#define IN_BUF_SIZE 4096
#endif

#ifndef OUT_BUF_SIZE
#define OUT_BUF_SIZE IN_BUF_SIZE
#endif

#ifndef QUEUE_BUF_SIZE
#define QUEUE_BUF_SIZE OUT_BUF_SIZE
#endif

static int parse_ws_frame(char *buf, int len, struct ws_msg *msg) {
  int i, n = 0, mask_len = 0;
  msg->header_len = msg->data_len = 0;
  if (len >= 2) {
    n = buf[1] & 0x7f;
    mask_len = buf[1] & FLAGS_MASK_FIN ? 4 : 0;
    msg->flags = *(unsigned char *) buf;
    if (n < 126 && len >= mask_len) {
      msg->data_len = n;
      msg->header_len = 2 + mask_len;
    } else if (n == 126 && len >= 4 + mask_len) {
      msg->header_len = 4 + mask_len;
      msg->data_len = ntohs(*(uint16_t *) &buf[2]);
    } else if (len >= 10 + mask_len) {
      msg->header_len = 10 + mask_len;
      msg->data_len = (((uint64_t) ntohl(*(uint32_t *) &buf[2])) << 32) +
                      ntohl(*(uint32_t *) &buf[6]);
    }
  }
  if (msg->header_len + msg->data_len > len) return 0;
  /* Apply mask  */
  if (mask_len > 0) {
    for (i = 0; i < msg->data_len; i++) {
      buf[i + msg->header_len] ^= (buf + msg->header_len - mask_len)[i % 4];
    }
  }
  return msg->header_len + msg->data_len;
}

static void ws_send(struct conn *c, int op, char *buf, int len) {
  unsigned char header[10];
  int i, header_len = 0;
  uint8_t mask[4] = {0x71, 0x3e, 0x5a, 0xcc}; /* it is random, i bet ya */
  header[0] = op | FLAGS_MASK_FIN;

  if (len < 126) {
    header[1] = (unsigned char) len;
    header_len = 2;
  } else if (len < 65535) {
    uint16_t tmp = htons((uint16_t) len);
    header[1] = 126;
    memcpy(&header[2], &tmp, sizeof(tmp));
    header_len = 4;
  } else {
    uint32_t tmp;
    header[1] = 127;
    tmp = htonl((uint32_t)((uint64_t) len >> 32));
    memcpy(&header[2], &tmp, sizeof(tmp));
    tmp = htonl((uint32_t)(len & 0xffffffff));
    memcpy(&header[6], &tmp, sizeof(tmp));
    header_len = 10;
  }
  header[1] |= 1 << 7; /* set masking flag */
  freshen_net_send(c, header, header_len);
  freshen_net_send(c, mask, sizeof(mask));
  for (i = 0; i < len; i++) {
    buf[i] ^= mask[i % sizeof(mask)];
  }
  freshen_net_send(c, buf, len);
}

static bool freshen_ws_handshake(struct conn *c, const char *url,
                                 const char *pass) {
  int n = 0, sent = 0;
  char buf[512];
  char proto[10], host[100], port[10], uri[20];
  freshen_parse_url(url, proto, host, port, uri);
  if ((n = snprintf(buf, sizeof(buf),
                    "GET %s HTTP/1.1\r\n"
                    "Host: %s:%s\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "Sec-WebSocket-Key: p0EAAPE61hDZrLdgKgy1Og==\r\n"
                    "Sec-WebSocket-Protocol: dash.freshen.cc\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Origin: http://%s\r\n"
                    "\r\n",
                    uri, host, port, pass, host)) > (int) sizeof(buf) ||
      n < 0 || (sent = freshen_net_send(c, buf, n)) != n) {
    FLOGE("send([%s] %d) = %d : %d", buf, n, sent, errno);
    return false;
  }
  return true;
}

static void freshen_close_conn(struct privdata *pd, const char *msg) {
  FLOGE("%s", msg);
  pd->handshake_sent = pd->is_ws = false;
  pd->in_len = 0;
  freshen_net_close(&pd->conn);
  freshen_sleep(1);
}

static int freshen_sender(char *buf, int len, void *privdata) {
  struct privdata *pd = (struct privdata *) privdata;
  FLOGI("WS out: %d [%.*s]", len, len, buf);
  ws_send(&pd->conn, WEBSOCKET_OP_TEXT, buf, len);
  return len;
}

static void freshen_poll(struct freshen_ctx *ctx, const char *url,
                         const char *version, const char *pass) {
  struct privdata *pd = (struct privdata *) ctx->privdata;
  if (pd == NULL) {
    ctx->privdata = pd = (struct privdata *) calloc(1, sizeof(*pd));
    ctx->sender = freshen_sender;
#if defined(__unix__)
    signal(SIGPIPE, SIG_IGN);
#endif
    pd->in = (char *) malloc(IN_BUF_SIZE);
    freshen_net_init_conn(&pd->conn);
    freshen_ota_init(ctx);
    freshen_rpc_init(ctx, version);
  }

  if (!freconnect(url, &pd->conn)) {
    freshen_close_conn(pd, "connect error");
    return;
  }

  if (!pd->handshake_sent && !freshen_ws_handshake(&pd->conn, url, pass)) {
    freshen_close_conn(pd, "handshake error");
    return;
  }
  pd->handshake_sent = true;

  if (pd->in_len >= IN_BUF_SIZE) {
    freshen_close_conn(pd, "input buffer overflow");
    return;
  }

  int n = freshen_net_recv(&pd->conn, pd->in + pd->in_len,
                           IN_BUF_SIZE - pd->in_len);
  if (n < 0) {
    freshen_close_conn(pd, "read error");
    return;
  }
  pd->in_len += n;

  // Still in the WS handshake mode. Buffer and skip HTTP reply
  if (!pd->is_ws) {
    for (int i = 4; i <= pd->in_len; i++) {
      if (memcmp(pd->in + i - 4, "\r\n\r\n", 4) == 0) {
        memmove(pd->in, pd->in + i, pd->in_len - i);
        pd->in_len -= i;
        pd->is_ws = true;
      }
    }
    if (!pd->is_ws) return;
  }
  freshen_ota_commit(ctx);

  struct ws_msg msg;
  if (parse_ws_frame(pd->in, pd->in_len, &msg)) {
    char *data = pd->in + msg.header_len;
    int data_len = msg.data_len;
    while (data_len > 0 && data[data_len - 1] != '}') data_len--;

    switch (msg.flags & FLAGS_MASK_OP) {
      case WEBSOCKET_OP_PING:
        FLOGI("WS PONG");
        ws_send(&pd->conn, WEBSOCKET_OP_PONG, data, data_len);
        break;
      case WEBSOCKET_OP_CLOSE:
        freshen_close_conn(pd, "WS close received");
        return;
    }

    FLOGI("WS in: %d [%.*s]", data_len, data_len, data);
    freshen_ctx_process(ctx, data, data_len);

    int framelen = msg.header_len + msg.data_len;
    memmove(pd->in, pd->in + framelen, pd->in_len - framelen);
    pd->in_len -= framelen;
  }
}

#endif
#endif /* FRESHEN_DECLARATIONS_ONLY */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FRESHEN_H */
