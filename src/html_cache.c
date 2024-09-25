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

  // Check if path has "0x2F" inside of it to determine if delimeters will be
  // "0x2F" or "%2F".
  uint_fast8_t is_normal_delimeter = 1;
  const size_t path_len = strlen(stripped_path);
  for (size_t idx = 0; stripped_path[idx] != 0; ++idx) {
    if (idx + 4 <= path_len && strncmp(stripped_path + idx, "0x2F", 4) == 0) {
      is_normal_delimeter = 0;
      break;
    }
  }

  // Create the cache-filename piece by piece.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *parts = simple_archiver_list_init();

  size_t idx = 0;
  size_t prev_idx = 0;
  size_t size;

  for (; idx < path_len && stripped_path[idx] != 0; ++idx) {
    if (stripped_path[idx] == '/') {
      size = idx - prev_idx + 1;
      char *temp_buf = malloc(size);
      memcpy(temp_buf, stripped_path + prev_idx, size - 1);
      temp_buf[size - 1] = 0;
      c_simple_http_add_string_part(parts, temp_buf, 0);
      free(temp_buf);

      if (is_normal_delimeter) {
        temp_buf = malloc(5);
        memcpy(temp_buf, "0x2F", 5);
        c_simple_http_add_string_part(parts, temp_buf, 0);
        free(temp_buf);
      } else {
        temp_buf = malloc(4);
        memcpy(temp_buf, "%2F", 4);
        c_simple_http_add_string_part(parts, temp_buf, 0);
        free(temp_buf);
      }

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
    // Prevent string from being free'd by moving it to another variable.
    char *temp = stripped_path;
    stripped_path = NULL;
    return temp;
  } else {
    return c_simple_http_combine_string_parts(parts);
  }
}

char *c_simple_http_cache_filename_to_path(const char *cache_filename) {
  uint_fast8_t is_percent_encoded = 0;
  if (!cache_filename) {
    return NULL;
  } else if (strcmp(cache_filename, "ROOT") == 0) {
    char *buf = malloc(2);
    buf[0] = '/';
    buf[1] = 0;
    return buf;
  } else if (cache_filename[0] == '%') {
    is_percent_encoded = 1;
  }

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *parts = simple_archiver_list_init();

  size_t idx = 0;
  size_t prev_idx = 0;
  const size_t size = strlen(cache_filename);
  char *buf;
  size_t buf_size;

  for(; idx < size && cache_filename[idx] != 0; ++idx) {
    if (is_percent_encoded && strncmp(cache_filename + idx, "%2F", 3) == 0) {
      if (prev_idx < idx) {
        buf_size = idx - prev_idx + 2;
        buf = malloc(buf_size);
        memcpy(buf, cache_filename + prev_idx, buf_size - 2);
        buf[buf_size - 2] = '/';
        buf[buf_size - 1] = 0;
        c_simple_http_add_string_part(parts, buf, 0);
        free(buf);
      } else {
        buf_size = 2;
        buf = malloc(buf_size);
        buf[0] = '/';
        buf[1] = 0;
        c_simple_http_add_string_part(parts, buf, 0);
        free(buf);
      }
      idx += 2;
      prev_idx = idx + 1;
    } else if (!is_percent_encoded
        && strncmp(cache_filename + idx, "0x2F", 4) == 0) {
      if (prev_idx < idx) {
        buf_size = idx - prev_idx + 2;
        buf = malloc(buf_size);
        memcpy(buf, cache_filename + prev_idx, buf_size - 2);
        buf[buf_size - 2] = '/';
        buf[buf_size - 1] = 0;
        c_simple_http_add_string_part(parts, buf, 0);
        free(buf);
      } else {
        buf_size = 2;
        buf = malloc(buf_size);
        buf[0] = '/';
        buf[1] = 0;
        c_simple_http_add_string_part(parts, buf, 0);
        free(buf);
      }
      idx += 3;
      prev_idx = idx + 1;
    }
  }

  if (prev_idx < idx) {
    buf_size = idx - prev_idx + 1;
    buf = malloc(buf_size);
    memcpy(buf, cache_filename + prev_idx, buf_size - 1);
    buf[buf_size - 1] = 0;
    c_simple_http_add_string_part(parts, buf, 0);
    free(buf);
  }

  return c_simple_http_combine_string_parts(parts);
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
