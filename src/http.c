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

#include "http.h"

// Standard library includes.
#include <stdlib.h>
#include <string.h>

// Local includes
#include <helpers.h>
#include "constants.h"

typedef struct HashMapWrapper {
  SDArchiverHashMap *hash_map;
} HashMapWrapper;

void c_simple_http_hash_map_wrapper_cleanup_hashmap_fn(void *data) {
  HashMapWrapper *wrapper = data;
  simple_archiver_hash_map_free(&wrapper->hash_map);
  free(wrapper);
}

void c_simple_http_hash_map_wrapper_cleanup(HashMapWrapper *wrapper) {
  simple_archiver_hash_map_free(&wrapper->hash_map);
  free(wrapper);
}

void c_simple_http_hash_map_cleanup_helper(SDArchiverHashMap *hash_map) {
  simple_archiver_hash_map_free(&hash_map);
}

HTTPTemplates c_simple_http_set_up_templates(const char *config_filename) {
  HTTPTemplates templates;
  templates.paths = simple_archiver_hash_map_init();

  FILE *f = fopen(config_filename, "r");
  unsigned char key_buf[C_SIMPLE_HTTP_CONFIG_BUF_SIZE];
  unsigned char value_buf[C_SIMPLE_HTTP_CONFIG_BUF_SIZE];
  unsigned int key_idx = 0;
  unsigned int value_idx = 0;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *current_path = NULL;
  unsigned int current_path_size = 0;

  // xxx0 - reading key
  // xxx1 - reading value
  // xx0x - does not have "HTML" key
  // xx1x - does have "HTML" key
  unsigned int state = 0;
  int c;

  while (feof(f) == 0) {
    c = fgetc(f);
    if (c == EOF) {
      break;
    } else if (c == ' ' || c == '\t') {
      // Ignore whitespace.
      continue;
    } else if ((state & 1) == 0 && (c == '\r' || c == '\n')) {
      // Ignore newlines when parsing for key.
      continue;
    }
    if ((state & 1) == 0) {
      if (c != '=') {
        key_buf[key_idx++] = c;
        if (key_idx >= C_SIMPLE_HTTP_CONFIG_BUF_SIZE) {
          fprintf(stderr,
                  "ERROR: config file \"key\" is larger than %u bytes!\n",
                  C_SIMPLE_HTTP_CONFIG_BUF_SIZE - 1);
          c_simple_http_clean_up_templates(&templates);
          templates.paths = NULL;
          return templates;
        }
      } else {
        if (key_idx < C_SIMPLE_HTTP_CONFIG_BUF_SIZE) {
          key_buf[key_idx++] = 0;
        } else {
          fprintf(stderr,
                  "ERROR: config file \"key\" is larger than %u bytes!\n",
                  C_SIMPLE_HTTP_CONFIG_BUF_SIZE - 1);
          c_simple_http_clean_up_templates(&templates);
          templates.paths = NULL;
          return templates;
        }
        state |= 1;
      }
    } else if ((state & 1) == 1) {
      if (c != '\n' && c != '\r') {
        value_buf[value_idx++] = c;
        if (value_idx >= C_SIMPLE_HTTP_CONFIG_BUF_SIZE) {
          fprintf(stderr,
                  "ERROR: config file \"value\" is larger than %u bytes!\n",
                  C_SIMPLE_HTTP_CONFIG_BUF_SIZE - 1);
          c_simple_http_clean_up_templates(&templates);
          templates.paths = NULL;
          return templates;
        }
      } else {
        if (value_idx < C_SIMPLE_HTTP_CONFIG_BUF_SIZE) {
          value_buf[value_idx++] = 0;
        } else {
          fprintf(stderr,
                  "ERROR: config file \"value\" is larger than %u bytes!\n",
                  C_SIMPLE_HTTP_CONFIG_BUF_SIZE - 1);
          c_simple_http_clean_up_templates(&templates);
          templates.paths = NULL;
          return templates;
        }
        state &= 0xFFFFFFFE;

        // Check if key is "PATH".
        if (strcmp((char*)key_buf, "PATH") == 0) {
          if (current_path) {
            if ((state & 0x2) == 0) {
              fprintf(stderr,
                      "ERROR: config file did not have \"HTML\" key for PATH!"
                      "\n");
              c_simple_http_clean_up_templates(&templates);
              templates.paths = NULL;
              return templates;
            }
            state &= 0xFFFFFFFD;
            free(current_path);
          }
          current_path = malloc(value_idx);
          memcpy(current_path, value_buf, value_idx);
          current_path_size = value_idx;
          // At this point, key is "PATH".
          SDArchiverHashMap *hash_map = simple_archiver_hash_map_init();
          unsigned char *key = malloc(5);
          key[0] = 'P';
          key[1] = 'A';
          key[2] = 'T';
          key[3] = 'H';
          key[4] = 0;
          unsigned char *value = malloc(value_idx);
          memcpy(value, value_buf, value_idx);
          if (simple_archiver_hash_map_insert(&hash_map,
                                              value,
                                              key,
                                              5,
                                              NULL,
                                              NULL) != 0) {
            fprintf(stderr,
                "ERROR: Failed to create hash map for new PATH block!\n");
            c_simple_http_clean_up_templates(&templates);
            templates.paths = NULL;
            free(key);
            free(value);
            return templates;
          }

          HashMapWrapper *wrapper = malloc(sizeof(HashMapWrapper));
          wrapper->hash_map = hash_map;

          if (simple_archiver_hash_map_insert(
              &templates.paths,
              wrapper,
              value,
              value_idx,
              simple_archiver_helper_datastructure_cleanup_nop,
              c_simple_http_hash_map_wrapper_cleanup_hashmap_fn) != 0) {
            fprintf(stderr,
                "ERROR: Failed to insert new hash map for new PATH block!\n");
            c_simple_http_clean_up_templates(&templates);
            templates.paths = NULL;
            c_simple_http_hash_map_wrapper_cleanup(wrapper);
            return templates;
          }
        } else if (!current_path) {
          fprintf(
              stderr,
              "ERROR: config file has invalid key: No preceding \"PATH\" "
              "key!\n");
          c_simple_http_clean_up_templates(&templates);
          templates.paths = NULL;
          return templates;
        } else {
          // Non-"PATH" key.
          if (strcmp((char*)key_buf, "HTML") == 0
              || strcmp((char*)key_buf, "HTML_FILE") == 0) {
            state |= 0x2;
          }
          HashMapWrapper *hash_map_wrapper = simple_archiver_hash_map_get(
            templates.paths,
            current_path,
            current_path_size);
          if (!hash_map_wrapper) {
            fprintf(stderr,
              "ERROR: Internal error failed to get existing hash map with path "
              "\"%s\"!", current_path);
            c_simple_http_clean_up_templates(&templates);
            templates.paths = NULL;
            return templates;
          }

          unsigned char *key = malloc(key_idx);
          memcpy(key, key_buf, key_idx);
          unsigned char *value;
          if (strcmp((char*)key_buf, "HTML_FILE") == 0) {
            __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
            FILE *html_file = fopen((char*)value_buf, "r");
            if (!html_file) {
              fprintf(stderr,
                "ERROR: Internal error failed to open HTML_FILE \"%s\"!",
                value_buf);
              c_simple_http_clean_up_templates(&templates);
              templates.paths = NULL;
              return templates;
            }

            fseek(html_file, 0, SEEK_END);
            long file_size = ftell(html_file);
            if (file_size <= 0) {
              fprintf(stderr,
                "ERROR: Internal error HTML_FILE \"%s\" is invalid size!",
                value_buf);
              c_simple_http_clean_up_templates(&templates);
              templates.paths = NULL;
              return templates;
            }
            fseek(html_file, 0, SEEK_SET);
            unsigned long file_size_u = file_size;

            unsigned char *read_buf = malloc(file_size_u);
            size_t read_amount = 0;
            read_amount = fread(read_buf, 1, file_size_u, html_file);
            if (read_amount != file_size_u) {
              fprintf(stderr, "ERROR: Failed to read HTML_FILE \"%s\"!\n",
                value_buf);
              free(read_buf);
              c_simple_http_clean_up_templates(&templates);
              templates.paths = NULL;
              return templates;
            }
            value = read_buf;
          } else {
            value = malloc(value_idx);
            memcpy(value, value_buf, value_idx);
          }

          if (simple_archiver_hash_map_insert(&hash_map_wrapper->hash_map,
                                              value,
                                              key,
                                              key_idx,
                                              NULL,
                                              NULL) != 0) {
            fprintf(stderr,
              "ERROR: Internal error failed to insert into hash map with path "
              "\"%s\"!", current_path);
            c_simple_http_clean_up_templates(&templates);
            templates.paths = NULL;
            free(key);
            free(value);
            return templates;
          }
        }
        key_idx = 0;
        value_idx = 0;
      }
    }
  }
  fclose(f);

  if (!current_path) {
    fprintf(stderr, "ERROR: Never got \"PATH\" key in config!\n");
    c_simple_http_clean_up_templates(&templates);
    templates.paths = NULL;
  } else if ((state & 0x2) == 0) {
    fprintf(stderr,
      "ERROR: Current \"PATH\" did not get \"HTML\" key in config!\n");
    c_simple_http_clean_up_templates(&templates);
    templates.paths = NULL;
  }

  return templates;
}

void c_simple_http_clean_up_templates(HTTPTemplates *templates) {
  simple_archiver_hash_map_free(&templates->paths);
}

char *c_simple_http_request_response(const char *request, unsigned int size,
                                     const HTTPTemplates *templates) {
  // TODO
  return NULL;
}

// vim: ts=2 sts=2 sw=2
