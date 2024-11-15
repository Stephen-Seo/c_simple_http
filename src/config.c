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

#include "config.h"

// Standard library includes.
#include <stdlib.h>
#include <string.h>

// Third party includes.
#include <SimpleArchiver/src/helpers.h>

// Local includes
#include "constants.h"

typedef struct C_SIMPLE_HTTP_INTERNAL_RequiredIter {
  SDArchiverHashMap *hash_map;
  const char *path;
} C_SIMPLE_HTTP_INTERNAL_RequiredIter;

typedef struct C_SIMPLE_HTTP_INTERNAL_RequiredCheck {
  const SDArchiverHashMap *map_of_paths_and_their_vars;
  const SDArchiverLinkedList *required;
} C_SIMPLE_HTTP_INTERNAL_RequiredCheck;

void c_simple_http_cleanup_config_value(
    C_SIMPLE_HTTP_ConfigValue *config_value) {
  if (config_value) {
    if (config_value->next) {
      c_simple_http_cleanup_config_value(config_value->next);
    }
    if(config_value->value) {
      free(config_value->value);
    }
    free(config_value);
  }
}

void c_simple_http_cleanup_config_value_void_ptr(void *config_value) {
  c_simple_http_cleanup_config_value(config_value);
}

int c_simple_http_required_iter_fn(void *data, void *ud) {
  C_SIMPLE_HTTP_INTERNAL_RequiredIter *req_iter_struct = ud;
  uint32_t data_str_length = (uint32_t)strlen(data) + 1;
  if (simple_archiver_hash_map_get(
      req_iter_struct->hash_map,
      data,
      data_str_length) == NULL) {
    if (req_iter_struct->path) {
      fprintf(
        stderr,
        "WARNING: Map of PATH \"%s\" does not have required key \"%s\"!\n",
        req_iter_struct->path,
        (char*)data);
      return 1;
    } else {
      fprintf(
        stderr,
        "WARNING: Map does not have required key \"%s\"!\n",
        (char*)data);
      return 1;
    }
  }
  return 0;
}

void c_simple_http_hash_map_wrapper_cleanup_hashmap_fn(void *data) {
  C_SIMPLE_HTTP_HashMapWrapper *wrapper = data;
  simple_archiver_hash_map_free(&wrapper->paths);
  free(wrapper);
}

void c_simple_http_hash_map_wrapper_cleanup(
    C_SIMPLE_HTTP_HashMapWrapper *wrapper) {
  simple_archiver_hash_map_free(&wrapper->paths);
  free(wrapper);
}

void internal_single_quote_decrement(uint_fast8_t *single_quote_count,
                                     uint32_t *state,
                                     char **key_buf,
                                     uint32_t *key_capacity,
                                     uint32_t *key_idx,
                                     char **value_buf,
                                     uint32_t *value_capacity,
                                     uint32_t *value_idx) {
  for(; (*single_quote_count) > 0; --(*single_quote_count)) {
    if (((*state) & 1) == 0) {
      (*key_buf)[(*key_idx)++] = '\'';
      if (*key_idx >= *key_capacity) {
        (*key_capacity) *= 2;
        (*key_buf) = realloc(*key_buf, *key_capacity);
      }
    } else {
      (*value_buf)[(*value_idx)++] = '\'';
      if ((*value_idx) >= *value_capacity) {
        (*value_capacity) *= 2;
        (*value_buf) = realloc(*value_buf, *value_capacity);
      }
    }
  }
}

void internal_double_quote_decrement(uint_fast8_t *double_quote_count,
                                     uint32_t *state,
                                     char **key_buf,
                                     uint32_t *key_capacity,
                                     uint32_t *key_idx,
                                     char **value_buf,
                                     uint32_t *value_capacity,
                                     uint32_t *value_idx) {
  for(; (*double_quote_count) > 0; --(*double_quote_count)) {
    if (((*state) & 1) == 0) {
      (*key_buf)[(*key_idx)++] = '"';
      if ((*key_idx) >= (*key_capacity)) {
        (*key_capacity) *= 2;
        (*key_buf) = realloc(*key_buf, *key_capacity);
      }
    } else {
      (*value_buf)[(*value_idx)++] = '"';
      if ((*value_idx) >= (*value_capacity)) {
        (*value_capacity) *= 2;
        (*value_buf) = realloc(*value_buf, *value_capacity);
      }
    }
  }
}

/// Returns non-zero if config should be returned.
int internal_check_add_value(uint32_t *state,
                             char **key_buf,
                             uint32_t *key_idx,
                             char **value_buf,
                             uint32_t *value_capacity,
                             uint32_t *value_idx,
                             C_SIMPLE_HTTP_ParsedConfig *config,
                             const char *separating_key,
                             const SDArchiverLinkedList *required_names,
                             const uint32_t separating_key_size,
                             SDArchiverLinkedList *paths,
                             char **current_separating_key_value,
                             uint32_t *current_separating_key_value_size) {
  if ((*value_idx) < (*value_capacity)) {
    (*value_buf)[(*value_idx)++] = 0;
  } else {
    (*value_capacity) *= 2;
    (*value_buf) = realloc(*value_buf, *value_capacity);
    (*value_buf)[(*value_idx)++] = 0;
  }
  (*state) &= 0xFFFFFFFE;

  /* Check if key is separating_key. */
  if (strcmp((*key_buf), separating_key) == 0) {
    if (*current_separating_key_value) {
      if (required_names) {
        C_SIMPLE_HTTP_HashMapWrapper *hash_map_wrapper =
          simple_archiver_hash_map_get(
            config->hash_map,
            (*current_separating_key_value),
            (*current_separating_key_value_size));
        C_SIMPLE_HTTP_INTERNAL_RequiredIter req_iter_struct;
        req_iter_struct.hash_map = hash_map_wrapper->hash_map;
        if (paths->count != 0) {
          req_iter_struct.path = paths->tail->prev->data;
        } else {
          req_iter_struct.path = NULL;
        }
        const char *missing_key = simple_archiver_list_get(
          required_names,
          c_simple_http_required_iter_fn,
          &req_iter_struct);
        if (missing_key) {
          fprintf(stderr,
                  "WARNING: config file did not have required key \"%s\"!"
                  " Returning NULL map!\n",
                  missing_key);
          c_simple_http_clean_up_parsed_config(config);
          config->hash_map = NULL;
          return 1;
        }
      }

      (*state) &= 0xFFFFFFFD;
      free((*current_separating_key_value));
    }
    (*current_separating_key_value) = malloc((*value_idx));
    memcpy((*current_separating_key_value), *value_buf, (*value_idx));
    (*current_separating_key_value_size) = (*value_idx);
    /* At this point, key is separating_key. */
    SDArchiverHashMap *hash_map = simple_archiver_hash_map_init();
    unsigned char *key = malloc(separating_key_size);
    strncpy((char*)key, separating_key, separating_key_size);
    C_SIMPLE_HTTP_ConfigValue *config_value =
      malloc(sizeof(C_SIMPLE_HTTP_ConfigValue));
    config_value->value = malloc(*value_idx);
    config_value->next = NULL;
    memcpy(config_value->value, *value_buf, (*value_idx));
    if (simple_archiver_hash_map_insert(
        hash_map,
        config_value,
        key,
        separating_key_size,
        c_simple_http_cleanup_config_value_void_ptr,
        NULL) != 0) {
      fprintf(stderr,
        "ERROR: Failed to create hash map for new separating_key "
        "block!\n");
      c_simple_http_clean_up_parsed_config(config);
      config->hash_map = NULL;
      free(key);
      free(config_value->value);
      free(config_value);
      return 1;
    }

    C_SIMPLE_HTTP_HashMapWrapper *wrapper =
      malloc(sizeof(C_SIMPLE_HTTP_HashMapWrapper));
    wrapper->paths = hash_map;

    if (simple_archiver_hash_map_insert(
        config->hash_map,
        wrapper,
        config_value->value,
        (*value_idx),
        c_simple_http_hash_map_wrapper_cleanup_hashmap_fn,
        simple_archiver_helper_datastructure_cleanup_nop) != 0) {
      fprintf(stderr,
          "ERROR: Failed to insert new hash map for new PATH block!\n");
      c_simple_http_clean_up_parsed_config(config);
      config->hash_map = NULL;
      c_simple_http_hash_map_wrapper_cleanup(wrapper);
      return 1;
    }
    simple_archiver_list_add(paths, config_value->value,
        simple_archiver_helper_datastructure_cleanup_nop);
  } else if (!(*current_separating_key_value)) {
    fprintf(
        stderr,
        "ERROR: config file has invalid key: No preceding \"%s\" "
        "key!\n", separating_key);
    c_simple_http_clean_up_parsed_config(config);
    config->hash_map = NULL;
    return 1;
  } else {
    /* Non-separating_key key. */
    C_SIMPLE_HTTP_HashMapWrapper *hash_map_wrapper =
      simple_archiver_hash_map_get(
        config->hash_map,
        (*current_separating_key_value),
        (*current_separating_key_value_size));
    if (!hash_map_wrapper) {
      fprintf(stderr,
        "ERROR: Internal error failed to get existing hash map with path "
        "\"%s\"!", (*current_separating_key_value));
      c_simple_http_clean_up_parsed_config(config);
      config->hash_map = NULL;
      return 1;
    }

    unsigned char *key = malloc(*key_idx);
    memcpy(key, *key_buf, *key_idx);
    unsigned char *value = malloc(*value_idx);
    memcpy(value, *value_buf, (*value_idx));

    // Check if key already exists in wrapped hash-map.
    C_SIMPLE_HTTP_ConfigValue *config_value =
      simple_archiver_hash_map_get(hash_map_wrapper->paths, key, *key_idx);
    if (config_value) {
      while(config_value->next) {
        config_value = config_value->next;
      }
      config_value->next = malloc(sizeof(C_SIMPLE_HTTP_ConfigValue));
      config_value->next->value = (char*)value;
      config_value->next->next = NULL;
    } else {
      config_value = malloc(sizeof(C_SIMPLE_HTTP_ConfigValue));
      config_value->value = (char*)value;
      config_value->next = NULL;
      if (simple_archiver_hash_map_insert(
          hash_map_wrapper->paths,
          config_value,
          key,
          *key_idx,
          c_simple_http_cleanup_config_value_void_ptr,
          NULL) != 0) {
        fprintf(stderr,
          "ERROR: Internal error failed to insert into hash map with path "
          "\"%s\"!", (*current_separating_key_value));
        c_simple_http_clean_up_parsed_config(config);
        config->hash_map = NULL;
        free(key);
        free(value);
        return 1;
      }
    }
  }
  (*key_idx) = 0;
  (*value_idx) = 0;
  return 0;
}

void c_simple_http_hash_map_cleanup_helper(SDArchiverHashMap *hash_map) {
  simple_archiver_hash_map_free(&hash_map);
}

int c_simple_http_check_required_iter_fn(void *path_void_str, void *ud) {
  C_SIMPLE_HTTP_INTERNAL_RequiredCheck *req = ud;
  uint32_t path_void_str_len = (uint32_t)strlen(path_void_str) + 1;
  C_SIMPLE_HTTP_HashMapWrapper *wrapper =
    simple_archiver_hash_map_get(
      req->map_of_paths_and_their_vars,
      path_void_str,
      path_void_str_len);
  if (!wrapper) {
    fprintf(stderr,
            "WARNING: Map of paths does not have path \"%s\"!\n",
            (char*)path_void_str);
    return 1;
  }
  C_SIMPLE_HTTP_INTERNAL_RequiredIter req_iter_struct;
  req_iter_struct.hash_map = wrapper->hash_map;
  req_iter_struct.path = path_void_str;
  return simple_archiver_list_get(
    req->required,
    c_simple_http_required_iter_fn,
    &req_iter_struct) != NULL ? 1 : 0;
}

C_SIMPLE_HTTP_ParsedConfig c_simple_http_parse_config(
  const char *config_filename,
  const char *separating_key,
  const SDArchiverLinkedList *required_names
) {
  C_SIMPLE_HTTP_ParsedConfig config;
  config.hash_map = NULL;

  if (!config_filename) {
    fprintf(stderr, "ERROR: config_filename argument is NULL!\n");
    return config;
  } else if (!separating_key) {
    fprintf(stderr, "ERROR: separating_key argument is NULL!\n");
    return config;
  }
  const uint32_t separating_key_size =
    (uint32_t)strlen(separating_key) + 1;

  config.hash_map = simple_archiver_hash_map_init();

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *paths = simple_archiver_list_init();

  __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
  FILE *f = fopen(config_filename, "r");
  if (!f) {
    fprintf(stderr, "ERROR: Failed to open file \"%s\"!\n", config_filename);
    c_simple_http_clean_up_parsed_config(&config);
    return config;
  }
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *key_buf = malloc(C_SIMPLE_HTTP_CONFIG_BUF_SIZE);
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *value_buf = malloc(C_SIMPLE_HTTP_CONFIG_BUF_SIZE);
  uint32_t key_capacity = C_SIMPLE_HTTP_CONFIG_BUF_SIZE;
  uint32_t value_capacity = C_SIMPLE_HTTP_CONFIG_BUF_SIZE;
  uint32_t key_idx = 0;
  uint32_t value_idx = 0;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *current_separating_key_value = NULL;
  uint32_t current_separating_key_value_size = 0;

  // xxx0 - reading key
  // xxx1 - reading value
  // 00xx - reading value is not quoted
  // 01xx - reading value is single quoted
  // 10xx - reading value is double quoted
  uint32_t state = 0;
  uint_fast8_t single_quote_count = 0;
  uint_fast8_t double_quote_count = 0;
  int32_t c;

  while (feof(f) == 0) {
    c = fgetc(f);
    if (c == EOF) {
      break;
    } else if ((state & 0xC) == 0 && (c == ' ' || c == '\t')) {
      // Ignore whitespace when not quoted.
      internal_single_quote_decrement(&single_quote_count,
                                      &state,
                                      &key_buf,
                                      &key_capacity,
                                      &key_idx,
                                      &value_buf,
                                      &value_capacity,
                                      &value_idx);
      internal_double_quote_decrement(&double_quote_count,
                                      &state,
                                      &key_buf,
                                      &key_capacity,
                                      &key_idx,
                                      &value_buf,
                                      &value_capacity,
                                      &value_idx);
      continue;
    } else if ((state & 1) == 0
        && (state & 0xC) == 0
        && (c == '\r' || c == '\n')) {
      // Ignore newlines when parsing for key and when not quoted.
      internal_single_quote_decrement(&single_quote_count,
                                      &state,
                                      &key_buf,
                                      &key_capacity,
                                      &key_idx,
                                      &value_buf,
                                      &value_capacity,
                                      &value_idx);
      internal_double_quote_decrement(&double_quote_count,
                                      &state,
                                      &key_buf,
                                      &key_capacity,
                                      &key_idx,
                                      &value_buf,
                                      &value_capacity,
                                      &value_idx);
      continue;
    } else if ((state & 1) == 1) {
      if (c == '\'') {
        ++single_quote_count;
        internal_double_quote_decrement(&double_quote_count,
                                        &state,
                                        &key_buf,
                                        &key_capacity,
                                        &key_idx,
                                        &value_buf,
                                        &value_capacity,
                                        &value_idx);

        if (((state & 0xC) == 0x4 || (state & 0xC) == 0)
            && single_quote_count >= C_SIMPLE_HTTP_QUOTE_COUNT_MAX) {
          single_quote_count = 0;

          // (state & 0xC) is known to be either 0x4 or 0.
          if ((state & 0xC) == 0) {
            state |= 0x4;
          } else {
            state &= 0xFFFFFFF3;
          }
        }
        continue;
      } else if (c == '"') {
        ++double_quote_count;
        internal_single_quote_decrement(&single_quote_count,
                                        &state,
                                        &key_buf,
                                        &key_capacity,
                                        &key_idx,
                                        &value_buf,
                                        &value_capacity,
                                        &value_idx);

        if (((state & 0xC) == 0x8 || (state & 0xC) == 0)
            && double_quote_count >= C_SIMPLE_HTTP_QUOTE_COUNT_MAX) {
          double_quote_count = 0;

          // (state & 0xC) is known to be either 0x8 or 0.
          if ((state & 0xC) == 0) {
            state |= 0x8;
          } else {
            state &= 0xFFFFFFF3;
          }
        }
        continue;
      } else {
        internal_single_quote_decrement(&single_quote_count,
                                        &state,
                                        &key_buf,
                                        &key_capacity,
                                        &key_idx,
                                        &value_buf,
                                        &value_capacity,
                                        &value_idx);
        internal_double_quote_decrement(&double_quote_count,
                                        &state,
                                        &key_buf,
                                        &key_capacity,
                                        &key_idx,
                                        &value_buf,
                                        &value_capacity,
                                        &value_idx);
      }
    }
    if ((state & 1) == 0) {
      if (c != '=') {
        key_buf[key_idx++] = (char)c;
        if (key_idx >= key_capacity) {
          key_capacity *= 2;
          key_buf = realloc(key_buf, key_capacity);
        }
      } else {
        if (key_idx < key_capacity) {
          key_buf[key_idx++] = 0;
          if (key_idx >= key_capacity) {
            key_capacity *= 2;
            key_buf = realloc(key_buf, key_capacity);
          }
        } else {
          key_capacity *= 2;
          key_buf = realloc(key_buf, key_capacity);
          key_buf[key_idx++] = 0;
        }
        state |= 1;
      }
    } else if ((state & 1) == 1) {
      if ((c != '\n' && c != '\r') || (state & 0xC) != 0) {
        value_buf[value_idx++] = (char)c;
        if (value_idx >= value_capacity) {
          value_capacity *= 2;
          value_buf = realloc(value_buf, value_capacity);
        }
      } else {
        if (internal_check_add_value(&state,
                                     &key_buf,
                                     &key_idx,
                                     &value_buf,
                                     &value_capacity,
                                     &value_idx,
                                     &config,
                                     separating_key,
                                     required_names,
                                     separating_key_size,
                                     paths,
                                     &current_separating_key_value,
                                     &current_separating_key_value_size)) {
          return config;
        }
      }
    }
  }
  simple_archiver_helper_cleanup_FILE(&f);

  if ((state & 0x1) == 1 && (state & 0xC) == 0 && value_idx != 0) {
    // Leftover "value" not added yet.

    if (internal_check_add_value(&state,
                                 &key_buf,
                                 &key_idx,
                                 &value_buf,
                                 &value_capacity,
                                 &value_idx,
                                 &config,
                                 separating_key,
                                 required_names,
                                 separating_key_size,
                                 paths,
                                 &current_separating_key_value,
                                 &current_separating_key_value_size)) {
      return config;
    }
  }

  if (!current_separating_key_value) {
    fprintf(stderr, "ERROR: Never got \"PATH\" key in config!\n");
    c_simple_http_clean_up_parsed_config(&config);
    config.hash_map = NULL;
  } else if (required_names) {
    C_SIMPLE_HTTP_INTERNAL_RequiredCheck required_check_struct;
    required_check_struct.map_of_paths_and_their_vars = config.hash_map;
    required_check_struct.required = required_names;
    const char *path_with_missing_key = simple_archiver_list_get(
      paths,
      c_simple_http_check_required_iter_fn,
      &required_check_struct);
    if (path_with_missing_key) {
      fprintf(stderr,
              "WARNING (parse end): config file with path \"%s\" did not have "
              "required key(s)! Returning NULL map!\n",
              path_with_missing_key);
      c_simple_http_clean_up_parsed_config(&config);
      config.hash_map = NULL;
    }
  }

  return config;
}

void c_simple_http_clean_up_parsed_config(C_SIMPLE_HTTP_ParsedConfig *config) {
  simple_archiver_hash_map_free(&config->paths);
}

// vim: et ts=2 sts=2 sw=2
