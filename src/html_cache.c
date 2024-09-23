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

// Standard library includes.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Third-party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "http.h"
#include "helpers.h"

char *c_simple_http_path_to_cache_filename(const char *path) {
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *stripped_path = c_simple_http_strip_path(path, strlen(path));

  if (!stripped_path) {
    return NULL;
  }

  if (strcmp(stripped_path, "/") == 0) {
    char *buf = malloc(5);
    memcpy(buf, "ROOT", 5);
    return buf;
  }

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *parts = simple_archiver_list_init();

  size_t idx = 0;
  size_t prev_idx = 0;
  const size_t path_len = strlen(stripped_path);
  size_t size;

  for (; idx < path_len && stripped_path[idx] != 0; ++idx) {
    if (stripped_path[idx] == '/') {
      size = idx - prev_idx + 1;
      char *temp_buf = malloc(size);
      memcpy(temp_buf, stripped_path + prev_idx, size - 1);
      temp_buf[size - 1] = 0;
      c_simple_http_add_string_part(parts, temp_buf, 0);
      free(temp_buf);

      temp_buf = malloc(5);
      memcpy(temp_buf, "0x2F", 5);
      c_simple_http_add_string_part(parts, temp_buf, 0);
      free(temp_buf);

      prev_idx = idx + 1;
    }
  }

  if (idx > prev_idx) {
    size = idx - prev_idx + 1;
    char *temp_buf = malloc(size);
    memcpy(temp_buf, stripped_path + prev_idx, size - 1);
    temp_buf[size - 1] = 0;
    c_simple_http_add_string_part(parts, temp_buf, 0);
    free(temp_buf);
  }

  if (prev_idx == 0) {
    return stripped_path;
  } else {
    return c_simple_http_combine_string_parts(parts);
  }
}

int c_simple_http_cache_path(
    const char *path,
    const char *config_filename,
    const char *cache_dir,
    char **buf_out) {
  // TODO
  return 0;
}

// vim: et ts=2 sts=2 sw=2
