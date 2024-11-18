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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// libc includes.
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>

int c_simple_http_internal_get_string_part_full_size(void *data, void *ud) {
  C_SIMPLE_HTTP_String_Part *part = data;
  size_t *count = ud;

  if (part->size > 0) {
    *count += part->size - 1;
  }

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
    SDArchiverLinkedList *list, const char *c_string, uintptr_t extra) {
  C_SIMPLE_HTTP_String_Part *string_part =
    malloc(sizeof(C_SIMPLE_HTTP_String_Part));

  if (c_string) {
    string_part->size = strlen(c_string) + 1;
    string_part->buf = malloc(string_part->size);
    memcpy(string_part->buf, c_string, string_part->size);
  } else {
    string_part->size = 0;
    string_part->buf = NULL;
  }

  string_part->extra = extra;
  simple_archiver_list_add(
    list, string_part, c_simple_http_cleanup_string_part);
}

void c_simple_http_add_string_part_sized(
    SDArchiverLinkedList *list,
    const char *buffer,
    size_t size,
    uintptr_t extra) {
  C_SIMPLE_HTTP_String_Part *string_part =
    malloc(sizeof(C_SIMPLE_HTTP_String_Part));

  if (buffer && size > 0) {
    string_part->size = size;
    string_part->buf = malloc(string_part->size - 1);
    memcpy(string_part->buf, buffer, string_part->size - 1);
  } else {
    string_part->size = 0;
    string_part->buf = NULL;
  }

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

int c_simple_http_helper_mkdir_tree(const char *path) {
  // Check if dir already exists.
  DIR *dir_ptr = opendir(path);
  if (dir_ptr) {
    // Directory already exists.
    closedir(dir_ptr);
    return 1;
  } else if (errno == ENOENT) {
    // Directory doesn't exist, create dir tree.
    size_t buf_size = strlen(path) + 1;
    char *buf = malloc(buf_size);
    memcpy(buf, path, buf_size - 1);
    buf[buf_size - 1] = 0;

    char *dirname_buf = dirname(buf);
    // Recursive call to ensure parent directories are created.
    int ret = c_simple_http_helper_mkdir_tree(dirname_buf);
    free(buf);
    if (ret == 1 || ret == 0) {
      // Parent directory should be created by now.
      ret = mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
      if (ret != 0) {
        return 4;
      }
    } else {
      return 3;
    }

    return 0;
  } else {
    // Other directory error.
    return 2;
  }
}

void c_simple_http_cleanup_DIR(DIR **fd) {
  if (fd && *fd) {
    closedir(*fd);
    *fd = NULL;
  }
}

char *c_simple_http_FILE_to_c_str(const char *filename, uint64_t *size_out) {
  FILE *fd = fopen(filename, "rb");
  if (!fd) {
    fprintf(stderr, "ERROR Failed to open %s!\n", filename);
    return NULL;
  } else if (fseek(fd, 0, SEEK_END) != 0) {
    fprintf(stderr, "ERROR Failed to seek to end of %s!\n", filename);
    fclose(fd);
    return NULL;
  }

  long size = ftell(fd);
  if (size < 0) {
    fprintf(stderr, "ERROR Failed to get seek pos of end of %s!\n", filename);
    fclose(fd);
    return NULL;
  } else if (size == 0) {
    fprintf(stderr, "ERROR Size of file \"%s\" is zero!\n", filename);
    fclose(fd);
    return NULL;
  }
  if (size_out) {
    *size_out = (uint64_t)size;
  }
  char *buf = malloc((uint64_t)size + 1);
  if (fseek(fd, 0, SEEK_SET) != 0) {
    fprintf(stderr, "ERROR Failed to seek to beginning of %s!\n", filename);
    free(buf);
    fclose(fd);
    return NULL;
  } else if (fread(buf, 1, (uint64_t)size, fd) != (uint64_t)size) {
    fprintf(stderr, "ERROR Failed to read from file %s!\n", filename);
    free(buf);
    fclose(fd);
    return NULL;
  }

  buf[size] = 0;

  fclose(fd);
  return buf;
}

size_t c_simple_http_trim_end_whitespace(char *c_str) {
  size_t trimmed = 0;

  uint64_t idx= strlen(c_str);
  for (; idx-- > 0;) {
    if (c_str[idx] == ' '
        || c_str[idx] == '\n'
        || c_str[idx] == '\r'
        || c_str[idx] == '\t') {
      c_str[idx] = 0;
      ++trimmed;
    } else {
      break;
    }
  }

  return trimmed;
}

// vim: et ts=2 sts=2 sw=2
