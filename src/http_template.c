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

#include "http_template.h"

// Standard library includes.
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Third party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "config.h"
#include "helpers.h"

/// Returns 0 if "c_string" ends with "_FILE".
int c_simple_http_internal_ends_with_FILE(const char *c_string) {
  if (!c_string) {
    return 1;
  }

  const char *comparison_string = "_FILE";

  const size_t c_string_size = strlen(c_string);

  if (c_string_size >= 5 && strcmp(
      comparison_string,
      c_string + (c_string_size - strlen(comparison_string)))
        == 0) {
    return 0;
  }

  return 2;
}

char *c_simple_http_path_to_generated(
    const char *path,
    const C_SIMPLE_HTTP_HTTPTemplates *templates,
    size_t *output_buf_size,
    SDArchiverLinkedList **files_list_out) {
  if (output_buf_size) {
    *output_buf_size = 0;
  }
  if (files_list_out) {
    *files_list_out = simple_archiver_list_init();
  }
  size_t path_len_size_t = strlen(path) + 1;
  if (path_len_size_t > 0xFFFFFFFF) {
    fprintf(stderr, "ERROR: Path string is too large!\n");
    return NULL;
  }
  uint32_t path_len = (uint32_t)path_len_size_t;
  C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map =
    simple_archiver_hash_map_get(templates->hash_map, path, path_len);
  if (!wrapped_hash_map) {
    return NULL;
  }
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *html_buf = NULL;
  size_t html_buf_size = 0;
  C_SIMPLE_HTTP_ConfigValue *html_file_value =
    simple_archiver_hash_map_get(wrapped_hash_map->hash_map, "HTML_FILE", 10);
  if (html_file_value && html_file_value->value) {
    __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
    FILE *f = fopen(html_file_value->value, "r");
    if (!f) {
      return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
      return NULL;
    }
    long html_file_size = ftell(f);
    if (html_file_size <= 0) {
      return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
      return NULL;
    }
    html_buf = malloc((size_t)html_file_size + 1);
    size_t ret = fread(html_buf, 1, (size_t)html_file_size, f);
    if (ret != (size_t)html_file_size) {
      return NULL;
    }
    html_buf[html_file_size] = 0;
    html_buf_size = (size_t)html_file_size;
    if (files_list_out) {
      char *html_filename_copy = malloc(strlen(html_file_value->value) + 1);
      strcpy(html_filename_copy, html_file_value->value);
      simple_archiver_list_add(*files_list_out, html_filename_copy, NULL);
    }
  } else {
    C_SIMPLE_HTTP_ConfigValue *stored_html_config_value =
      simple_archiver_hash_map_get(wrapped_hash_map->hash_map, "HTML", 5);
    if (!stored_html_config_value || !stored_html_config_value->value) {
      return NULL;
    }
    size_t stored_html_size = strlen(stored_html_config_value->value) + 1;
    html_buf = malloc(stored_html_size);
    memcpy(html_buf, stored_html_config_value->value, stored_html_size);
    html_buf_size = stored_html_size - 1;
  }

  // At this point, html_buf contains the raw HTML as a C-string.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *string_part_list = simple_archiver_list_init();

  size_t idx = 0;
  size_t last_template_idx = 0;

  C_SIMPLE_HTTP_String_Part string_part;

  size_t delimeter_count = 0;

  // xxxx xxx0 - Initial state, no delimeter reached.
  // xxxx xxx1 - Three left-curly-brace delimeters reached.
  uint32_t state = 0;

  for (; idx < html_buf_size; ++idx) {
    if ((state & 1) == 0) {
      // Using 0x7B instead of left curly-brace due to bug in vim navigation.
      if (html_buf[idx] == 0x7B) {
        ++delimeter_count;
        if (delimeter_count >= 3) {
          delimeter_count = 0;
          state |= 1;
          if (string_part_list->count != 0) {
            C_SIMPLE_HTTP_String_Part *last_part =
              string_part_list->tail->prev->data;
            last_template_idx = last_part->extra;
          }
          string_part.size = idx - last_template_idx - 1;
          string_part.buf = malloc(string_part.size);
          memcpy(
            string_part.buf,
            html_buf + last_template_idx,
            string_part.size);
          string_part.buf[string_part.size - 1] = 0;
          string_part.extra = idx + 1;
          c_simple_http_add_string_part(string_part_list,
                                        string_part.buf,
                                        string_part.extra);
          free(string_part.buf);
        }
      } else {
        delimeter_count = 0;
      }
    } else {
      // (state & 1) is 1
      // Using 0x7D instead of right curly-brace due to bug in vim navigation.
      if (html_buf[idx] == 0x7D) {
        ++delimeter_count;
        if (delimeter_count >= 3) {
          delimeter_count = 0;
          state &= 0xFFFFFFFE;
          C_SIMPLE_HTTP_String_Part *last_part =
            string_part_list->tail->prev->data;
          size_t var_size = idx - 2 - last_part->extra;
          __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
          char *var = malloc(var_size + 1);
          memcpy(
            var,
            html_buf + last_part->extra,
            var_size);
          var[var_size] = 0;
          C_SIMPLE_HTTP_ConfigValue *config_value =
            simple_archiver_hash_map_get(
              wrapped_hash_map->hash_map,
              var,
              (uint32_t)var_size + 1);
          if (config_value && config_value->value) {
            if (c_simple_http_internal_ends_with_FILE(var) == 0) {
              // Load from file specified by "config_value->value".
              __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
              FILE *f = fopen(config_value->value, "r");
              if (!f) {
                fprintf(stderr, "ERROR Failed to open file \"%s\"!\n",
                        config_value->value);
                return NULL;
              } else if (fseek(f, 0, SEEK_END) != 0) {
                fprintf(stderr, "ERROR Failed to seek to end of file \"%s\"!\n",
                        config_value->value);
                return NULL;
              }
              long file_size = ftell(f);
              if (file_size <= 0) {
                fprintf(stderr, "ERROR Size of file \"%s\" is invalid!\n",
                        config_value->value);
                return NULL;
              } else if (fseek(f, 0, SEEK_SET) != 0) {
                fprintf(stderr, "ERROR Failed to seek to start of file "
                        "\"%s\"!\n",
                        config_value->value);
                return NULL;
              }
              string_part.size = (size_t)file_size + 1;
              string_part.buf = malloc(string_part.size);
              string_part.extra = idx + 1;

              if (fread(string_part.buf,
                        string_part.size - 1,
                        1,
                        f)
                    != 1) {
                fprintf(stderr, "ERROR Failed to read from file \"%s\"!\n",
                        config_value->value);
                return NULL;
              }
              string_part.buf[string_part.size - 1] = 0;
              if (files_list_out) {
                char *variable_filename = malloc(strlen(config_value->value) + 1);
                strcpy(variable_filename, config_value->value);
                simple_archiver_list_add(
                    *files_list_out, variable_filename, NULL);
              }
            } else {
              // Variable data is "config_value->value".
              string_part.size = strlen(config_value->value) + 1;
              string_part.buf = malloc(string_part.size);
              memcpy(string_part.buf, config_value->value, string_part.size);
              string_part.buf[string_part.size - 1] = 0;
              string_part.extra = idx + 1;
            }
          } else {
            string_part.buf = NULL;
            string_part.size = 0;
            string_part.extra = idx + 1;
          }
          c_simple_http_add_string_part(string_part_list,
                                        string_part.buf,
                                        string_part.extra);
          free(string_part.buf);
        }
      } else {
        delimeter_count = 0;
      }
    }
  }

  if (string_part_list->count != 0) {
    C_SIMPLE_HTTP_String_Part *last_part =
      string_part_list->tail->prev->data;
    if (idx > last_part->extra) {
      string_part.size = idx - last_part->extra + 1;
      string_part.buf = malloc(string_part.size);
      memcpy(string_part.buf, html_buf + last_part->extra, string_part.size);
      string_part.buf[string_part.size - 1] = 0;
      string_part.extra = idx + 1;
      c_simple_http_add_string_part(string_part_list,
                                    string_part.buf,
                                    string_part.extra);
      free(string_part.buf);

      last_part = string_part_list->tail->prev->data;
    }

    char *combined_buf = c_simple_http_combine_string_parts(string_part_list);
    if (output_buf_size) {
      *output_buf_size = strlen(combined_buf);
    }
    return combined_buf;
  } else {
    // Prevent cleanup fn from "free"ing html_buf and return it verbatim.
    char *buf = html_buf;
    html_buf = NULL;
    if (output_buf_size) {
      *output_buf_size = html_buf_size;
    }
    return buf;
  }
}

// vim: et ts=2 sts=2 sw=2
