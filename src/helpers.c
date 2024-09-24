// ISC License
// 
// Copyright (c) 2024 Stephen Seo
// 
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
// 
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include "helpers.h"

// Standard library includes.
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int c_simple_http_internal_get_string_part_full_size(void *data, void *ud) {
  C_SIMPLE_HTTP_String_Part *part = data;
  size_t *count = ud;

  *count += part->size - 1;

  return 0;
}

int c_simple_http_internal_combine_string_parts_from_list(void *data,
                                                          void *ud) {
  C_SIMPLE_HTTP_String_Part *part = data;
  void **ptrs = ud;
  char *buf = ptrs[0];
  size_t *current_count = ptrs[1];
  const size_t *total_count = ptrs[2];

  if (!part->buf || part->size == 0) {
    // Empty string part, just continue.
    return 0;
  }

  if (*current_count + part->size - 1 > *total_count) {
    fprintf(stderr, "ERROR Invalid state combining string parts!\n");
    return 1;
  }

  memcpy(buf + *current_count, part->buf, part->size - 1);

  *current_count += part->size - 1;

  return 0;
}

void c_simple_http_cleanup_attr_string_part(C_SIMPLE_HTTP_String_Part **part) {
  if (part && *part) {
    if ((*part)->buf) {
      free((*part)->buf);
    }
    free(*part);
  }
  *part = NULL;
}

void c_simple_http_cleanup_string_part(void *data) {
  C_SIMPLE_HTTP_String_Part *part = data;
  if (part) {
    if (part->buf) {
      free(part->buf);
    }
    free(part);
  }
}

void c_simple_http_add_string_part(
    SDArchiverLinkedList *list, const char *c_string, size_t extra) {
  C_SIMPLE_HTTP_String_Part *string_part =
    malloc(sizeof(C_SIMPLE_HTTP_String_Part));

  string_part->size = strlen(c_string) + 1;
  string_part->buf = malloc(string_part->size);
  memcpy(string_part->buf, c_string, string_part->size);

  string_part->extra = extra;

  simple_archiver_list_add(
    list, string_part, c_simple_http_cleanup_string_part);
}

char *c_simple_http_combine_string_parts(const SDArchiverLinkedList *list) {
  if (!list || list->count == 0) {
    return NULL;
  }

  size_t count = 0;

  simple_archiver_list_get(
    list, c_simple_http_internal_get_string_part_full_size, &count);

  char *buf = malloc(count + 1);
  size_t current_count = 0;

  void **ptrs = malloc(sizeof(void*) * 3);
  ptrs[0] = buf;
  ptrs[1] = &current_count;
  ptrs[2] = &count;

  if (simple_archiver_list_get(
      list, c_simple_http_internal_combine_string_parts_from_list, ptrs)) {
    free(buf);
    free(ptrs);
    return NULL;
  }

  free(ptrs);

  buf[count] = 0;

  return buf;
}

void c_simple_http_helper_to_lowercase_in_place(char *buf, size_t size) {
  for (size_t idx = 0; idx < size; ++idx) {
    if (buf[idx] >= 'A' && buf[idx] <= 'Z') {
      buf[idx] += 32;
    }
  }
}

char *c_simple_http_helper_to_lowercase(const char *buf, size_t size) {
  char *ret_buf = malloc(size + 1);
  for (size_t idx = 0; idx < size; ++idx) {
    if (buf[idx] >= 'A' && buf[idx] <= 'Z') {
      ret_buf[idx] = buf[idx] + 32;
    } else {
      ret_buf[idx] = buf[idx];
    }
  }

  ret_buf[size] = 0;
  return ret_buf;
}

char c_simple_http_helper_hex_to_value(const char upper, const char lower) {
  char result = 0;

  if (upper >= '0' && upper <= '9') {
    result |= (char)(upper - '0') << 4;
  } else if (upper >= 'a' && upper <= 'f') {
    result |= (char)(upper - 'a' + 10) << 4;
  } else if (upper >= 'A' && upper <= 'F') {
    result |= (char)(upper - 'A' + 10) << 4;
  } else {
    return 0;
  }

  if (lower >= '0' && lower <= '9') {
    result |= lower - '0';
  } else if (lower >= 'a' && lower <= 'f') {
    result |= lower - 'a' + 10;
  } else if (lower >= 'A' && lower <= 'F') {
    result |= lower - 'A' + 10;
  } else {
    return 0;
  }

  return result;
}

char *c_simple_http_helper_unescape_uri(const char *uri) {
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *parts = simple_archiver_list_init();

  const size_t size = strlen(uri);
  size_t idx = 0;
  size_t prev_idx = 0;
  size_t buf_size;
  char *buf;

  for (; idx <= size; ++idx) {
    if (uri[idx] == '%' && idx + 2 < size) {
      if (idx > prev_idx) {
        buf_size = idx - prev_idx + 1;
        buf = malloc(buf_size);
        memcpy(buf, uri + prev_idx, buf_size - 1);
        buf[buf_size - 1] = 0;
        c_simple_http_add_string_part(parts, buf, 0);
        free(buf);
      }
      buf = malloc(2);
      buf[0] = c_simple_http_helper_hex_to_value(uri[idx + 1], uri[idx + 2]);
      buf[1] = 0;
      if (buf[0] == 0) {
        free(buf);
        return NULL;
      }
      c_simple_http_add_string_part(parts, buf, 0);
      free(buf);
      prev_idx = idx + 3;
      idx += 2;
    }
  }

  if (idx > prev_idx) {
    buf_size = idx - prev_idx + 1;
    buf = malloc(buf_size);
    memcpy(buf, uri + prev_idx, buf_size - 1);
    buf[buf_size - 1] = 0;
    c_simple_http_add_string_part(parts, buf, 0);
    free(buf);
  }

  return c_simple_http_combine_string_parts(parts);
}

// vim: et ts=2 sts=2 sw=2
