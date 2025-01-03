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
#include <SimpleArchiver/src/data_structures/hash_map.h>
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "config.h"
#include "helpers.h"

typedef struct C_SIMPLE_HTTP_ArrayVar {
  // Should be the value of the variable, or filename if var ends with _FILE.
  char *value;
  struct C_SIMPLE_HTTP_ArrayVar *next;
} C_SIMPLE_HTTP_ArrayVar;

typedef struct C_SIMPLE_HTTP_ForState {
  uint64_t idx;
  uint64_t buf_start_idx;
  uint64_t inner_foreach_count;
  SDArchiverLinkedList *array_vars_keys;
} C_SIMPLE_HTTP_ForState;

typedef struct C_SIMPLE_HTTP_RemoveForState {
  C_SIMPLE_HTTP_ForState *for_state;
  SDArchiverHashMap *array_vars;
} C_SIMPLE_HTTP_RemoveForState;

// Function forward declaration.
int c_simple_http_internal_handle_inside_delimeters(
    uint32_t *state,
    const char *html_buf,
    const size_t html_buf_idx,
    const char *var,
    const size_t var_size,
    SDArchiverLinkedList *if_state_stack,
    SDArchiverLinkedList *for_state_stack,
    uint64_t for_state_idx,
    SDArchiverHashMap *array_vars,
    const C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map,
    SDArchiverLinkedList *string_part_list,
    SDArchiverHashMap **files_set_out);

void c_simple_http_internal_set_out_insert(SDArchiverHashMap **set,
                                           const char *str) {
  if (set
      && *set
      && simple_archiver_hash_map_get(*set, str, strlen(str) + 1) == NULL) {
    simple_archiver_hash_map_insert(
      *set,
      (void*)1,
      strdup(str),
      strlen(str) + 1,
      simple_archiver_helper_datastructure_cleanup_nop,
      NULL);
  }
}

void c_simple_http_internal_cleanup_ArrayVar(
    C_SIMPLE_HTTP_ArrayVar **array_var) {
  if (*array_var) {
    if ((*array_var)->next) {
      c_simple_http_internal_cleanup_ArrayVar(&(*array_var)->next);
    }
    if ((*array_var)->value) {
      free((*array_var)->value);
    }
    free(*array_var);
    *array_var = NULL;
  }
}

void c_simple_http_internal_cleanup_ArrayVar_void(void *data) {
  c_simple_http_internal_cleanup_ArrayVar((C_SIMPLE_HTTP_ArrayVar**)&data);
}

void c_simple_http_internal_cleanup_ForState(C_SIMPLE_HTTP_ForState **state) {
  if (*state) {
    if ((*state)->array_vars_keys) {
      simple_archiver_list_free(&(*state)->array_vars_keys);
    }
    free(*state);
    *state = NULL;
  }
}

void c_simple_http_internal_cleanup_ForState_void(void *data) {
  c_simple_http_internal_cleanup_ForState((C_SIMPLE_HTTP_ForState**)&data);
}

int c_simple_http_internal_cleanup_array_vars_keys(void *data, void *ud) {
  SDArchiverHashMap *array_vars = ud;
  char *key = data;
  simple_archiver_hash_map_remove(array_vars, key, strlen(key) + 1);
  return 0;
}

int c_simple_http_internal_free_exact_ForState_in_list(void *data, void *ud) {
  C_SIMPLE_HTTP_ForState *for_state = data;
  C_SIMPLE_HTTP_RemoveForState *state_was_top = ud;
  int ret = state_was_top->for_state == for_state;
  if (ret != 0) {
    simple_archiver_list_get(state_was_top->for_state->array_vars_keys,
                             c_simple_http_internal_cleanup_array_vars_keys,
                             state_was_top->array_vars);
  }
  return ret;
}

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

int c_simple_http_internal_always_return_one(
    __attribute__((unused)) void *unused_0,
    __attribute__((unused)) void *unused_1) {
  return 1;
}

/// If non-NULL, must not be free'd.
char *c_simple_http_internal_get_for_var(const char *var,
                                         uint64_t idx,
                                         SDArchiverHashMap *array_vars) {
  C_SIMPLE_HTTP_ArrayVar *array_var =
    simple_archiver_hash_map_get(array_vars,
                                 var,
                                 strlen(var) + 1);
  if (!array_var) {
    return NULL;
  }
  for(; idx-- > 0;) {
    array_var = array_var->next;
    if (!array_var) {
      return NULL;
    }
  }
  return array_var->value;
}

int c_simple_http_internal_parse_if_expression(
    const C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map,
    const char *var,
    const size_t var_offset,
    const size_t var_size,
    const uint64_t *for_state_idx,
    SDArchiverHashMap *array_vars,
    char **left_side_out,
    char **right_side_out,
    uint_fast8_t *is_equality_out,
    SDArchiverHashMap **files_set_out) {
  if (!left_side_out || !right_side_out || !is_equality_out) {
    fprintf(stderr, "ERROR Internal error! %s\n", var);
    return 1;
  }
  *left_side_out = NULL;
  *right_side_out = NULL;

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *var_parts = simple_archiver_list_init();
  char buf[64];
  size_t buf_idx = 0;
  size_t idx = var_offset;
  ssize_t var_index = -1;
  for(; idx < var_size; ++idx) {
    if (idx + 1 < var_size && var[idx] == '!' && var[idx + 1] == '=') {
      break;
    } else if (idx + 1 < var_size && var[idx] == '=' && var[idx + 1] == '=') {
      break;
    } else if (var[idx] == '[') {
      if (var_index >= 0) {
        fprintf(stderr, "ERROR \"[\" Encountered twice! %s\n", var);
        return 1;
      }
      idx += 1;
      var_index = 0;
      for(; idx < var_size; ++idx) {
        if (var[idx] == ']') {
          break;
        } else if (var[idx] >= '0' && var[idx] <= '9') {
          var_index = var_index * 10 + ((ssize_t)(var[idx] - '0'));
        } else {
          fprintf(stderr, "ERROR Syntax error in \"[...]\"! %s\n", var);
          return 1;
        }
      }
      if (idx >= var_size) {
        fprintf(
          stderr, "ERROR End of expression during parsing! %s\n", var);
        return 1;
      } else if (var[idx] != ']') {
        fprintf(stderr, "ERROR No closing \"]\"! %s\n", var);
        return 1;
      } else if (
          idx + 2 < var_size && var[idx + 1] == '!' && var[idx + 2] == '=') {
        ++idx;
        break;
      } else if (
          idx + 2 < var_size && var[idx + 1] == '=' && var[idx + 2] == '=') {
        ++idx;
        break;
      } else {
        fprintf(stderr, "ERROR Invalid expression after \"]\"! %s\n", var);
        return 1;
      }
    }
    buf[buf_idx++] = var[idx];
    if (buf_idx >= 63) {
      buf[63] = 0;
      c_simple_http_add_string_part(var_parts, buf, 0);
      buf_idx = 0;
    }
  }

  if (buf_idx == 0) {
    fprintf(stderr, "ERROR Empty VAR in expression! %s\n", var);
    return 1;
  } else if (idx >= var_size) {
    fprintf(stderr, "ERROR Invalid \"IF\" expression! %s\n", var);
    return 1;
  }

  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *var_buf = NULL;
  if (var_parts->count != 0) {
    if (buf_idx != 0) {
      buf[buf_idx] = 0;
      c_simple_http_add_string_part(var_parts, buf, 0);
    }
    var_buf = c_simple_http_combine_string_parts(var_parts);
  } else {
    buf[buf_idx] = 0;
    var_buf = strdup(buf);
  }
  buf_idx = 0;
  simple_archiver_list_free(&var_parts);
  var_parts = simple_archiver_list_init();

  // At this point, var_buf contains the left-hand-side string that hasn't been
  // expanded yet.

  // Check if ForEach variable.
  char *value = NULL;
  C_SIMPLE_HTTP_ConfigValue *config_value = NULL;
  if (for_state_idx != NULL) {
    if (var_index != -1) {
      fprintf(stderr,
              "ERROR Invalid indexing on expanded FOREACH variable! %s\n",
              var);
      return 1;
    }
    value = c_simple_http_internal_get_for_var(var_buf,
                                               *for_state_idx,
                                               array_vars);
  }
  if (!value) {
    // No ForEach variable, get as regular variable.
    config_value =
      simple_archiver_hash_map_get(wrapped_hash_map->hash_map,
                                   var_buf,
                                   strlen(var_buf) + 1);
  }
  if (!value && (!config_value || !config_value->value)) {
    fprintf(stderr, "ERROR Invalid VAR after \"IF/ELSEIF\"! %s\n", var);
    return 1;
  } else if (var_index >= 0) {
    for (size_t var_index_idx = 0;
         var_index_idx < var_index;
         ++var_index_idx) {
      config_value = config_value->next;
      if (!config_value) {
        fprintf(stderr, "ERROR Array index out of bounds! %s\n", var);
        return 1;
      }
    }
  }

  uint64_t left_side_size = 0;
  if (c_simple_http_internal_ends_with_FILE(var_buf) == 0) {
    *left_side_out =
      c_simple_http_FILE_to_c_str(value ? value : config_value->value,
                                  &left_side_size);
    if (!*left_side_out || left_side_size == 0) {
      fprintf(stderr, "ERROR _FILE variable could not be read! %s\n", var);
      if (*left_side_out) {
        free(*left_side_out);
        *left_side_out = NULL;
      }
      return 1;
    }
    c_simple_http_trim_end_whitespace(*left_side_out);
    left_side_size = strlen(*left_side_out);
    c_simple_http_internal_set_out_insert(files_set_out, config_value->value);
  } else {
    *left_side_out = strdup(value ? value : config_value->value);
    c_simple_http_trim_end_whitespace(*left_side_out);
    left_side_size = strlen(*left_side_out);
  }

  if (idx + 1 < var_size && var[idx] == '=' && var[idx + 1] == '=') {
    *is_equality_out = 1;
  } else if (idx + 1 < var_size && var[idx] == '!' && var[idx + 1] == '=') {
    *is_equality_out = 0;
  } else {
    fprintf(stderr, "ERROR Operation not n/eq! %s\n", var);
    return 1;
  }

  idx += 2;

  for (; idx < var_size; ++idx) {
    buf[buf_idx++] = var[idx];
    if (buf_idx >= 63) {
      buf[63] = 0;
      c_simple_http_add_string_part(var_parts, buf, 0);
      buf_idx = 0;
    }
  }

  if (var_parts->count != 0) {
    if (buf_idx != 0) {
      buf[buf_idx] = 0;
      c_simple_http_add_string_part(var_parts, buf, 0);
    }
    *right_side_out = c_simple_http_combine_string_parts(var_parts);
  } else {
    buf[buf_idx] = 0;
    *right_side_out = strdup(buf);
  }
  c_simple_http_trim_end_whitespace(*right_side_out);
  uint64_t right_side_size = strlen(*right_side_out);
  if (right_side_size == 0) {
    fprintf(stderr, "ERROR Right side is empty! %s\n", var);
    return 1;
  }
  simple_archiver_list_free(&var_parts);
  var_parts = simple_archiver_list_init();
  buf_idx = 0;
  return 0;
}

int c_simple_http_internal_populate_array_vars(
    char **for_each_var_name,
    const C_SIMPLE_HTTP_HashMapWrapper *wrapped_hash_map,
    const char *var,
    SDArchiverHashMap *array_vars,
    SDArchiverLinkedList *array_vars_keys,
    SDArchiverHashMap **files_set_out) {
  C_SIMPLE_HTTP_ConfigValue *config_value =
    simple_archiver_hash_map_get(wrapped_hash_map->hash_map,
                                 *for_each_var_name,
                                 strlen(*for_each_var_name) + 1);
  if (!config_value || !config_value->value) {
    fprintf(
      stderr,
      "ERROR Given var name \"%s\" not in config! %s\n",
      *for_each_var_name,
      var);
    return 1;
  }
  C_SIMPLE_HTTP_ArrayVar *array_var =
    malloc(sizeof(C_SIMPLE_HTTP_ArrayVar));
  array_var->next = NULL;
  C_SIMPLE_HTTP_ArrayVar *current_array_var = array_var;

  const uint_fast8_t is_ends_with_FILE =
    c_simple_http_internal_ends_with_FILE(*for_each_var_name) == 0
      ? 1 : 0;

  if (is_ends_with_FILE) {
    array_var->value = c_simple_http_FILE_to_c_str(config_value->value, NULL);
    if (!array_var->value) {
      fprintf(stderr,
              "ERROR Failed to read from file \"%s\"! %s\n",
              config_value->value,
              var);
      c_simple_http_internal_cleanup_ArrayVar(&array_var);
      return 1;
    }
    c_simple_http_internal_set_out_insert(files_set_out, config_value->value);
  } else {
    array_var->value = strdup(config_value->value);
  }

  while (config_value) {
    config_value = config_value->next;
    if (config_value) {
      current_array_var->next =
        malloc(sizeof(C_SIMPLE_HTTP_ArrayVar));
      current_array_var = current_array_var->next;
      current_array_var->next = NULL;
      if (is_ends_with_FILE) {
        current_array_var->value =
          c_simple_http_FILE_to_c_str(config_value->value, NULL);
        if (!current_array_var->value) {
          fprintf(stderr,
                  "ERROR Failed to read from file \"%s\"! %s\n",
                  config_value->value,
                  var);
          c_simple_http_internal_cleanup_ArrayVar(&array_var);
          return 1;
        }
        c_simple_http_internal_set_out_insert(files_set_out,
                                              config_value->value);
      } else {
        current_array_var->value = strdup(config_value->value);
      }
    }
  }

  if (simple_archiver_hash_map_insert(
      array_vars,
      array_var,
      *for_each_var_name,
      strlen(*for_each_var_name) + 1,
      c_simple_http_internal_cleanup_ArrayVar_void,
      NULL) != 0) {
    fprintf(stderr, "ERROR Internal failed to add array_var! %s\n", var);
    *for_each_var_name = NULL;
    return 1;
  } else if (simple_archiver_list_add(array_vars_keys,
                                      strdup(*for_each_var_name),
                                      NULL) != 0) {
    fprintf(stderr, "ERROR Internal failed to add array_var name! %s\n", var);
    return 1;
  }

  return 0;
}

int c_simple_http_internal_parse_iterate(
    uint32_t *state,
    const char *html_buf,
    uint64_t idx,
    size_t *delimeter_count,
    size_t *last_template_idx,
    SDArchiverLinkedList *if_state_stack,
    SDArchiverLinkedList *for_state_stack,
    uint64_t for_state_idx,
    SDArchiverHashMap *array_vars,
    SDArchiverLinkedList *string_part_list,
    const C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map,
    SDArchiverHashMap **files_set_out) {
  C_SIMPLE_HTTP_String_Part string_part;
  if (((*state) & 1) == 0) {
    // Using 0x7B instead of left curly-brace due to bug in vim navigation.
    if (html_buf[idx] == 0x7B) {
      ++(*delimeter_count);
      if ((*delimeter_count) >= 3) {
        (*delimeter_count) = 0;
        (*state) |= 1;
        uint32_t *if_state_stack_head = if_state_stack->count == 0 ? NULL :
          if_state_stack->head->next->data;
        if ((!if_state_stack_head
            || ((*if_state_stack_head) & 7) == 1
              || ((*if_state_stack_head) & 7) == 3
              || ((*if_state_stack_head) & 7) == 5)
            && for_state_stack->count == 0) {
          if (string_part_list->count != 0 && ((*state) & 2) == 0) {
            C_SIMPLE_HTTP_String_Part *last_part =
              string_part_list->tail->prev->data;
            (*last_template_idx) = last_part->extra;
          } else if (((*state) & 2) != 0) {
            // Only skip first "last_part->extra" in FOREACH.
            *state &= 0xFFFFFFFD;
          }
          string_part.size = idx - (*last_template_idx) - 1;
          string_part.buf = malloc(string_part.size);
          memcpy(
            string_part.buf,
            html_buf + (*last_template_idx),
            string_part.size);
          string_part.buf[string_part.size - 1] = 0;
          string_part.extra = idx + 1;
          c_simple_http_add_string_part(string_part_list,
                                        string_part.buf,
                                        string_part.extra);
          free(string_part.buf);
        } else {
          c_simple_http_add_string_part(string_part_list, NULL, idx + 1);
        }
      }
    } else {
      (*delimeter_count) = 0;
    }
  } else {
    // (state & 1) is 1
    // Using 0x7D instead of right curly-brace due to bug in vim navigation.
    if (html_buf[idx] == 0x7D) {
      ++(*delimeter_count);
      if ((*delimeter_count) >= 3) {
        (*delimeter_count) = 0;
        (*state) &= 0xFFFFFFFE;
        C_SIMPLE_HTTP_String_Part *last_part =
          string_part_list->tail->prev->data;
        const size_t var_size = idx - 2 - last_part->extra;
        __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
        char *var = malloc(var_size + 1);
        memcpy(
          var,
          html_buf + last_part->extra,
          var_size);
        var[var_size] = 0;
        if (c_simple_http_internal_handle_inside_delimeters(
              state,
              html_buf,
              idx,
              var,
              var_size,
              if_state_stack,
              for_state_stack,
              for_state_idx,
              array_vars,
              wrapped_hash_map,
              string_part_list,
              files_set_out) != 0) {
          return 1;
        }
      }
    } else {
      (*delimeter_count) = 0;
    }
  }

  return 0;
}

int c_simple_http_internal_for_hash_iter(const void *key,
                                         size_t size,
                                         const void *value,
                                         void *ud) {
  const C_SIMPLE_HTTP_ArrayVar *array_var = value;
  uint64_t *idx = ud;

  for (size_t count = 0; count < *idx; ++count) {
    array_var = array_var->next;
    if (!array_var) {
      return 1;
    }
  }

  return 0;
}

/// Returns zero if all ArrayVars can reach the idx in ForState.
int c_simple_http_internal_for_has_idx(SDArchiverHashMap *array_vars,
                                       uint64_t idx) {
  return simple_archiver_hash_map_iter(array_vars,
                                       c_simple_http_internal_for_hash_iter,
                                       &idx);
}

/// Returns zero on success.
int c_simple_http_internal_handle_inside_delimeters(
    uint32_t *state,
    const char *html_buf,
    const size_t html_buf_idx,
    const char *var,
    const size_t var_size,
    SDArchiverLinkedList *if_state_stack,
    SDArchiverLinkedList *for_state_stack,
    uint64_t for_state_idx,
    SDArchiverHashMap *array_vars,
    const C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map,
    SDArchiverLinkedList *string_part_list,
    SDArchiverHashMap **files_set_out) {
  C_SIMPLE_HTTP_String_Part string_part;
  if (var_size == 0) {
    fprintf(stderr, "ERROR No characters within delimeters!\n");
    return 1;
  } else if (var[0] == '!') {
    // Is an expression.
    if (strncmp(var + 1, "IF ", 3) == 0) {
      if (for_state_stack->count != 0 && ((*state) & 0x4) == 0) {
        // Skip if in FOREACH but not iterating within FOREACH yet.
        return 0;
      }
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *left_side = NULL;
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *right_side = NULL;

      uint_fast8_t is_equality = 0;

      if (c_simple_http_internal_parse_if_expression(
          wrapped_hash_map,
          var,
          1 + 3,
          var_size,
          (*state) & 0x4 ? &for_state_idx : NULL,
          array_vars,
          &left_side,
          &right_side,
          &is_equality,
          files_set_out)) {
        return 1;
      } else if (!left_side || !right_side) {
        fprintf(stderr, "ERROR Internal error! %s\n", var);
        return 1;
      }

      uint32_t *state = malloc(sizeof(uint32_t));
      *state = 0;

      uint32_t *prev_if_state = if_state_stack->count == 0 ? NULL :
        if_state_stack->head->next->data;

      if (!prev_if_state
          || (((*prev_if_state) & 7) == 1
            || ((*prev_if_state) & 7) == 3
            || ((*prev_if_state) & 7) == 5)) {
        if (is_equality) {
          if (strcmp(left_side, right_side) == 0) {
            // Is equality is TRUE.
            (*state) |= 9;
          } else {
            // Is equality is FALSE.
            (*state) |= 2;
          }
        } else {
          if (strcmp(left_side, right_side) == 0) {
            // Is inequality is FALSE.
            (*state) |= 2;
          } else {
            // Is inequality is TRUE.
            (*state) |= 9;
          }
        }
      } else {
        // prev_if_state is not visible, so this nested block shouldn't be.
        (*state) |= 0xA;
      }
      simple_archiver_list_add_front(if_state_stack, state, NULL);
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
    } else if (strncmp(var + 1, "ELSEIF ", 7) == 0) {
      if (for_state_stack->count != 0 && ((*state) & 0x4) == 0) {
        // Skip if in FOREACH but not iterating within FOREACH yet.
        return 0;
      }
      if (if_state_stack->count == 0) {
        fprintf(stderr, "ERROR No previous conditional! %s\n", var);
        return 1;
      }
      uint32_t *state = if_state_stack->head->next->data;
      if (((*state) & 7) != 1 && ((*state) & 7) != 2
          && ((*state) & 7) != 3 && ((*state) & 7) != 4) {
        fprintf(
          stderr,
          "ERROR Invalid \"ELSEIF\" (no previous IF/ELSEIF)! %s\n",
          var);
        return 1;
      }

      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *left_side = NULL;
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *right_side = NULL;

      uint_fast8_t is_equality = 0;

      if (c_simple_http_internal_parse_if_expression(
          wrapped_hash_map,
          var,
          1 + 7,
          var_size,
          (*state) & 0x4 ? &for_state_idx : NULL,
          array_vars,
          &left_side,
          &right_side,
          &is_equality,
          files_set_out)) {
        return 1;
      } else if (!left_side || !right_side) {
        fprintf(stderr, "ERROR Internal error! %s\n", var);
        return 1;
      }

      if (((*state) & 8) == 0) {
        if (is_equality) {
          if (strcmp(left_side, right_side) == 0) {
            // Is equality is TRUE.
            (*state) &= 0xFFFFFFF8;
            (*state) |= 0xB;
          } else {
            // Is equality is FALSE.
            (*state) &= 0xFFFFFFF8;
            (*state) |= 4;
          }
        } else {
          if (strcmp(left_side, right_side) == 0) {
            // Is inequality is FALSE.
            (*state) &= 0xFFFFFFF8;
            (*state) |= 4;
          } else {
            // Is inequality is TRUE.
            (*state) &= 0xFFFFFFF8;
            (*state) |= 0xB;
          }
        }
      } else {
        // Previous IF/ELSEIF resolved true.
        (*state) &= 0xFFFFFFF8;
        (*state) |= 0xC;
      }
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
    } else if (strncmp(var + 1, "ELSE", 4) == 0) {
      if (for_state_stack->count != 0 && ((*state) & 0x4) == 0) {
        // Skip if in FOREACH but not iterating within FOREACH yet.
        return 0;
      }
      if (if_state_stack->count == 0) {
        fprintf(stderr, "ERROR No previous IF! %s\n", var);
        return 1;
      }
      uint32_t *state = if_state_stack->head->next->data;
      if (((*state) & 8) == 0) {
        // No previous expression resolved to true, enabling ELSE block.
        (*state) &= 0xFFFFFFF8;
        (*state) |= 0xD;
      } else {
        // Previous expression resolved to true, disabling ELSE block.
        (*state) &= 0xFFFFFFF8;
        (*state) |= 0xE;
      }
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
    } else if (strncmp(var + 1, "ENDIF", 5) == 0) {
      if (for_state_stack->count != 0 && ((*state) & 0x4) == 0) {
        // Skip if in FOREACH but not iterating within FOREACH yet.
        return 0;
      }
      if (if_state_stack->count == 0) {
        fprintf(stderr, "ERROR No previous IF! %s\n", var);
        return 1;
      }
      simple_archiver_list_remove_once(
        if_state_stack, c_simple_http_internal_always_return_one, NULL);
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
    } else if (strncmp(var + 1, "INDEX ", 6) == 0) {
      // Indexing into variable array.

      if (for_state_stack->count != 0 && ((*state) & 0x4) == 0) {
        // Skip if in FOREACH but not iterating within FOREACH yet.
        return 0;
      }

      __attribute__((cleanup(simple_archiver_list_free)))
      SDArchiverLinkedList *var_parts = simple_archiver_list_init();
      char buf[64];
      size_t buf_idx = 0;

      size_t idx = 7;
      for (; idx < var_size; ++idx) {
        if (var[idx] != '[') {
          buf[buf_idx++] = var[idx];
          if (buf_idx >= 63) {
            buf[63] = 0;
            c_simple_http_add_string_part(var_parts, buf, 0);
            buf_idx = 0;
          }
        } else {
          break;
        }
      }
      if (var[idx] != '[' || idx >= var_size) {
        fprintf(stderr, "ERROR Syntax error! %s\n", var);
        return 1;
      }
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *var_name = NULL;
      if (var_parts->count != 0) {
        if (buf_idx != 0) {
          buf[buf_idx] = 0;
          c_simple_http_add_string_part(var_parts, buf, 0);
          buf_idx = 0;
        }
        var_name = c_simple_http_combine_string_parts(var_parts);
      } else if (buf_idx != 0) {
        buf[buf_idx] = 0;
        var_name = strdup(buf);
      } else {
        fprintf(stderr, "ERROR Empty variable name string! %s\n", var);
        return 1;
      }
      C_SIMPLE_HTTP_ConfigValue *config_value =
        simple_archiver_hash_map_get(wrapped_hash_map->hash_map,
                                     var_name,
                                     strlen(var_name) + 1);
      if (!config_value) {
        fprintf(stderr, "ERROR Variable not found in config! %s\n", var);
        return 1;
      }

      idx += 1;
      ssize_t array_index = 0;
      for (; idx < var_size; ++idx) {
        if (var[idx] >= '0' && var[idx] <= '9') {
          array_index = array_index * 10 + (ssize_t)(var[idx] - '0');
        } else if (var[idx] == ']') {
          break;
        } else {
          fprintf(stderr, "ERROR Syntax error getting index! %s\n", var);
          return 1;
        }
      }
      if (idx >= var_size) {
        fprintf(stderr, "ERROR Syntax error getting index (reached end without "
            "reaching \"]\")! %s\n", var);
        return 1;
      }
      for (; array_index-- > 0;) {
        config_value = config_value->next;
        if (!config_value) {
          fprintf(stderr, "ERROR Array index out of bounds! %s\n", var);
          return 1;
        }
      }
      if (!config_value || !config_value->value) {
        fprintf(
          stderr, "ERROR Internal error invalid indexed value! %s\n", var);
        return 1;
      }

      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *value_contents = NULL;
      uint64_t value_contents_size = 0;
      if (c_simple_http_internal_ends_with_FILE(var_name) == 0) {
        value_contents = c_simple_http_FILE_to_c_str(
          config_value->value, &value_contents_size);
        if (!value_contents || value_contents_size == 0) {
          fprintf(stderr, "ERROR Failed to get value from FILE! %s\n", var);
          return 1;
        }
        c_simple_http_internal_set_out_insert(files_set_out,
                                              config_value->value);
      } else {
        value_contents = strdup(config_value->value);
        value_contents_size = strlen(value_contents);
      }

      c_simple_http_add_string_part_sized(string_part_list,
                                          value_contents,
                                          value_contents_size + 1,
                                          html_buf_idx + 1);
    } else if (strncmp(var + 1, "FOREACH ", 8) == 0) {
      if (if_state_stack->count != 0) {
        uint32_t *if_state = if_state_stack->head->next->data;
        if (((*if_state) & 7) != 1
            && ((*if_state) & 7) != 3
            && ((*if_state) & 7) != 5) {
          // In IF block but not enabled.
          return 0;
        }
      }
      if (for_state_stack->count != 0 && ((*state) & 4) == 0) {
        // Don't parse if nested and not running in nested iteration.
        C_SIMPLE_HTTP_ForState *for_state = for_state_stack->head->next->data;
        ++for_state->inner_foreach_count;
        return 0;
      }

      __attribute__((cleanup(c_simple_http_internal_cleanup_ForState)))
      C_SIMPLE_HTTP_ForState *for_state =
        malloc(sizeof(C_SIMPLE_HTTP_ForState));
      for_state->idx = 0;
      for_state->buf_start_idx = html_buf_idx + 1;
      for_state->inner_foreach_count = 0;
      for_state->array_vars_keys = simple_archiver_list_init();

      char buf[64];
      size_t buf_idx = 0;
      __attribute__((cleanup(simple_archiver_list_free)))
      SDArchiverLinkedList *parts = simple_archiver_list_init();

      size_t idx = 1 + 8;
      for (; idx < var_size; ++idx) {
        if (var[idx] != '!') {
          buf[buf_idx++] = var[idx];
          if (buf_idx >= 63) {
            buf[63] = 0;
            c_simple_http_add_string_part(parts, buf, 0);
            buf_idx = 0;
          }
        } else {
          char *for_each_var_name = NULL;
          if (parts->count != 0) {
            if (buf_idx != 0) {
              buf[buf_idx] = 0;
              c_simple_http_add_string_part(parts, buf, 0);
              buf_idx = 0;
            }
            for_each_var_name = c_simple_http_combine_string_parts(parts);
            simple_archiver_list_free(&parts);
            parts = simple_archiver_list_init();
          } else {
            if (buf_idx == 0) {
              fprintf(
                stderr, "ERROR Expected var name before \"!\"! %s\n", var);
              return 1;
            }
            buf[buf_idx] = 0;
            for_each_var_name = strdup(buf);
            buf_idx = 0;
          }

          if(c_simple_http_internal_populate_array_vars(
              &for_each_var_name,
              wrapped_hash_map,
              var,
              array_vars,
              for_state->array_vars_keys,
              files_set_out) != 0) {
            if (for_each_var_name) {
              free(for_each_var_name);
            }
            return 1;
          }
        }
      }
      if (buf_idx != 0 || parts->count != 0) {
        // Var-name remaining at end of "var".
        char *for_each_var_name = NULL;
        if (parts->count != 0) {
          if (buf_idx != 0) {
            buf[buf_idx] = 0;
            c_simple_http_add_string_part(parts, buf, 0);
            buf_idx = 0;
          }
          for_each_var_name = c_simple_http_combine_string_parts(parts);
          simple_archiver_list_free(&parts);
          parts = simple_archiver_list_init();
        } else {
          if (buf_idx == 0) {
            fprintf(
              stderr, "ERROR Expected var name before \"!\"! %s\n", var);
            return 1;
          }
          buf[buf_idx] = 0;
          for_each_var_name = strdup(buf);
          buf_idx = 0;
        }

        if(c_simple_http_internal_populate_array_vars(
            &for_each_var_name,
            wrapped_hash_map,
            var,
            array_vars,
            for_state->array_vars_keys,
            files_set_out) != 0) {
          if (for_each_var_name) {
            free(for_each_var_name);
          }
          return 1;
        }
      }

      simple_archiver_list_add_front(
        for_state_stack,
        for_state,
        c_simple_http_internal_cleanup_ForState_void);
      for_state = NULL;
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
      // Force "is nested iter" flag to be disabled after FOREACH once.
      (*state) &= 0xFFFFFFFB;
    } else if (strncmp(var + 1, "ENDFOREACH", 10) == 0) {
      if (if_state_stack->count != 0) {
        uint32_t *if_state = if_state_stack->head->next->data;
        if (((*if_state) & 7) != 1
            && ((*if_state) & 7) != 3
            && ((*if_state) & 7) != 5) {
          // In IF block but not enabled.
          return 0;
        }
      }
      if (for_state_stack->count == 0) {
        fprintf(stderr, "ERROR ENDFOREACH but no FOREACH!\n");
        return 1;
      } else if (for_state_stack->count != 0) {
        C_SIMPLE_HTTP_ForState *for_state = for_state_stack->head->next->data;
        if (((*state) & 4) == 0) {
          if (for_state->inner_foreach_count != 0) {
            --for_state->inner_foreach_count;
            c_simple_http_add_string_part(string_part_list,
                                          NULL,
                                          html_buf_idx + 1);
            return 0;
          }
        }
      }

      C_SIMPLE_HTTP_ForState *for_state = for_state_stack->head->next->data;
      if (!for_state) {
        fprintf(stderr, "ERROR No for_state in for_state_stack!\n");
        return 1;
      }

      const uint64_t end_idx = html_buf_idx + 1 - 3 - 10 - 1 - 3;
      if (end_idx < for_state->buf_start_idx) {
        fprintf(stderr, "ERROR Internal invalid idx! %s\n", var);
        return 1;
      }

      while (1) {
        // xxxx xxx0 - Initial state.
        // xxxx xxx1 - Three left-curly-brace delimeter reached.
        // xxxx xx1x - Don't update last_template_idx.
        // xxxx x1xx - Nested in ForEach.
        uint32_t inner_state = 6;
        size_t delimeter_count = 0;
        size_t last_template_idx = for_state->buf_start_idx;
        __attribute__((cleanup(simple_archiver_list_free)))
        SDArchiverLinkedList *for_state_stack_inner =
          simple_archiver_list_init();
        for (uint64_t idx = for_state->buf_start_idx; idx < end_idx; ++idx) {
          if (c_simple_http_internal_parse_iterate(&inner_state,
                                                   html_buf,
                                                   idx,
                                                   &delimeter_count,
                                                   &last_template_idx,
                                                   if_state_stack,
                                                   for_state_stack_inner,
                                                   for_state->idx,
                                                   array_vars,
                                                   string_part_list,
                                                   wrapped_hash_map,
                                                   files_set_out) != 0) {
            return 1;
          }
        }

        C_SIMPLE_HTTP_String_Part *last_part =
          string_part_list->tail->prev->data;
        if (end_idx > last_part->extra) {
          size_t size = end_idx - last_part->extra - 1;
          char *buf = malloc(size + 1);
          memcpy(buf, html_buf + last_part->extra, size);
          buf[size] = 0;
          c_simple_http_add_string_part(string_part_list, buf, end_idx + 1);
          free(buf);
        }

        ++for_state->idx;

        if (c_simple_http_internal_for_has_idx(array_vars,
                                               for_state->idx) != 0) {
          break;
        }
      }

      //simple_archiver_list_remove_once(
      //  for_state_stack,
      //  c_simple_http_internal_always_return_one,
      //  NULL);

      // Top may no longer refer to "state_top" when nested FOREACH is added,
      // so pointer comparison is necessary to remove the correct "state_top".
      C_SIMPLE_HTTP_RemoveForState remove_for_state;
      remove_for_state.for_state = for_state;
      remove_for_state.array_vars = array_vars;
      if(simple_archiver_list_remove_once(
          for_state_stack,
          c_simple_http_internal_free_exact_ForState_in_list,
          &remove_for_state) != 1) {
        fprintf(stderr, "ERROR Failed to free a single \"ForState\" entry!\n");
        return 1;
      }
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
    } else {
      fprintf(stderr, "ERROR Invalid expression! %s\n", var);
      return 1;
    }
  } else {
    // Refers to a variable by name.
    if (if_state_stack->count != 0) {
      uint32_t *if_state = if_state_stack->head->next->data;
      if (((*if_state) & 7) != 1
          && ((*if_state) & 7) != 3
          && ((*if_state) & 7) != 5) {
        // In IF block but not enabled.
        return 0;
      }
    }
    // Check if ForEach variable.
    if (for_state_stack->count == 0) {
      char *value = c_simple_http_internal_get_for_var(var,
                                                       for_state_idx,
                                                       array_vars);
      if (value) {
        c_simple_http_add_string_part(string_part_list,
                                      value,
                                      html_buf_idx + 1);
        return 0;
      }
    } else {
      c_simple_http_add_string_part(string_part_list, NULL, html_buf_idx + 1);
      return 0;
    }

    C_SIMPLE_HTTP_ConfigValue *config_value =
      simple_archiver_hash_map_get(
        wrapped_hash_map->hash_map,
        var,
        (uint32_t)var_size + 1);
    if (config_value && config_value->value) {
      if (c_simple_http_internal_ends_with_FILE(var) == 0) {
        // Load from file specified by "config_value->value".
        uint64_t size = 0;
        string_part.buf =
          c_simple_http_FILE_to_c_str(config_value->value, &size);
        if (!string_part.buf || size == 0) {
          fprintf(stderr, "ERROR Failed to read from file \"%s\"!\n",
                  config_value->value);
          if (string_part.buf) {
            free(string_part.buf);
          }
          return 1;
        }
        string_part.size = size + 1;
        string_part.extra = html_buf_idx + 1;

        string_part.buf[string_part.size - 1] = 0;
        c_simple_http_internal_set_out_insert(files_set_out,
                                              config_value->value);
      } else {
        // Variable data is "config_value->value".
        string_part.size = strlen(config_value->value) + 1;
        string_part.buf = malloc(string_part.size);
        memcpy(string_part.buf, config_value->value, string_part.size);
        string_part.buf[string_part.size - 1] = 0;
        string_part.extra = html_buf_idx + 1;
      }
    } else {
      string_part.buf = NULL;
      string_part.size = 0;
      string_part.extra = html_buf_idx + 1;
    }
    c_simple_http_add_string_part(string_part_list,
                                  string_part.buf,
                                  string_part.extra);
    if (string_part.buf) {
      free(string_part.buf);
    }
  }

  return 0;
}

char *c_simple_http_path_to_generated(
    const char *path,
    const C_SIMPLE_HTTP_HTTPTemplates *templates,
    size_t *output_buf_size,
    SDArchiverHashMap **files_set_out) {
  if (output_buf_size) {
    *output_buf_size = 0;
  }
  if (files_set_out) {
    *files_set_out = simple_archiver_hash_map_init();
  }
  size_t path_len_size_t = strlen(path) + 1;
  if (path_len_size_t > 0xFFFFFFFF) {
    fprintf(stderr, "ERROR: Path string is too large!\n");
    return NULL;
  }
  uint32_t path_len = (uint32_t)path_len_size_t;
  const C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map =
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
    c_simple_http_internal_set_out_insert(files_set_out,
                                          html_file_value->value);
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
  // xxxx xx1x - Don't update last_template_idx.
  // xxxx x1xx - Nested in ForEach.
  uint32_t state = 0;

  // xxxx x001 - If expression ALLOW contents.
  // xxxx x010 - If expression DISALLOW contents.
  // xxxx x011 - ElseIf expression ALLOW contents.
  // xxxx x100 - ElseIf expression DISALLOW contents.
  // xxxx x101 - Else expression ALLOW contents.
  // xxxx x110 - Else expression DISALLOW contents.
  // xxxx x111 - ForEach expression contents.
  // xxxx 1xxx - Previous If/ElseIf had true/ALLOW.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *if_state_stack = simple_archiver_list_init();

  // List values are ForState structs.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *for_state_stack = simple_archiver_list_init();

  __attribute__((cleanup(simple_archiver_hash_map_free)))
  SDArchiverHashMap *array_vars = simple_archiver_hash_map_init();

  for (; idx < html_buf_size; ++idx) {
    if(c_simple_http_internal_parse_iterate(&state,
                                            html_buf,
                                            idx,
                                            &delimeter_count,
                                            &last_template_idx,
                                            if_state_stack,
                                            for_state_stack,
                                            0,
                                            array_vars,
                                            string_part_list,
                                            wrapped_hash_map,
                                            files_set_out) != 0) {
      return NULL;
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
