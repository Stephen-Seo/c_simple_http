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

int c_simple_http_internal_combine_string_parts_from_list(void *data, void *ud) {
  C_SIMPLE_HTTP_String_Part *part = data;
  void **ptrs = ud;
  char *buf = ptrs[0];
  size_t *current_count = ptrs[1];
  const size_t *total_count = ptrs[2];

  if (*current_count + part->size - 1 > *total_count) {
    fprintf(stderr, "ERROR Invalid state combining string parts!\n");
    return 1;
  }

  memcpy(buf + *current_count, part->buf, part->size - 1);

  *current_count += part->size - 1;

  return 0;
}

void c_simple_http_cleanup_string_part(void *data) {
  C_SIMPLE_HTTP_String_Part *part = data;
  if (part) {
    if (part->buf) {
      free(part->buf);
      part->buf = NULL;
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

// vim: et ts=2 sts=2 sw=2
