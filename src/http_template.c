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

// Third party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/helpers.h>

typedef struct C_SIMPLE_HTTP_INTERNAL_Template_Node {
  char *html;
  size_t html_size;
  union {
    size_t orig_end_idx;
    size_t html_capacity;
  };
  SDArchiverLinkedList *forced_next;
} C_SIMPLE_HTTP_INTERNAL_Template_Node;

void c_simple_http_internal_free_template_node(void *data) {
  C_SIMPLE_HTTP_INTERNAL_Template_Node *node = data;
  if (node) {
    if (node->html) {
      free(node->html);
    }
    if (node->forced_next) {
      simple_archiver_list_free(&node->forced_next);
    }
    free(node);
  }
}

void c_simple_http_internal_cleanup_template_node(
    C_SIMPLE_HTTP_INTERNAL_Template_Node **node) {
  if (node && *node) {
    c_simple_http_internal_free_template_node(*node);
    *node = NULL;
  }
}

int c_simple_http_internal_get_final_size_fn(void *data, void *ud) {
  size_t *final_size = ud;
  C_SIMPLE_HTTP_INTERNAL_Template_Node *node = data;
  *final_size += node->html_size;
  return 0;
}

int c_simple_http_internal_fill_buf_fn(void *data, void *ud) {
  C_SIMPLE_HTTP_INTERNAL_Template_Node *node = data;
  C_SIMPLE_HTTP_INTERNAL_Template_Node *to_fill = ud;
  if (to_fill->html_capacity < to_fill->html_size + node->html_size) {
    return 1;
  }
  memcpy(
    to_fill->html + to_fill->html_size,
    node->html,
    node->html_size);
  to_fill->html_size += node->html_size;
  return 0;
}

char *c_simple_http_path_to_generated(
    const char *path,
    const C_SIMPLE_HTTP_HTTPTemplates *templates) {
  C_SIMPLE_HTTP_ParsedConfig *wrapped_hash_map =
    simple_archiver_hash_map_get(templates->hash_map, path, strlen(path) + 1);
  if (!wrapped_hash_map) {
    return NULL;
  }
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *html_buf = NULL;
  size_t html_buf_size = 0;
  const char *html_filename =
    simple_archiver_hash_map_get(wrapped_hash_map->hash_map, "HTML_FILE", 10);
  if (html_filename) {
    __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
    FILE *f = fopen(html_filename, "r");
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
    html_buf = malloc(html_file_size + 1);
    size_t ret = fread(html_buf, 1, html_file_size, f);
    if (ret != (size_t)html_file_size) {
      return NULL;
    }
    html_buf[html_file_size] = 0;
    html_buf_size = html_file_size;
  } else {
    char *stored_html =
      simple_archiver_hash_map_get(wrapped_hash_map->hash_map, "HTML", 5);
    if (!stored_html) {
      return NULL;
    }
    size_t stored_html_size = strlen(stored_html) + 1;
    html_buf = malloc(stored_html_size);
    memcpy(html_buf, stored_html, stored_html_size);
    html_buf_size = stored_html_size;
  }

  // At this point, html_buf contains the raw HTML as a C-string.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *template_html_list = simple_archiver_list_init();

  size_t idx = 0;
  size_t last_template_idx = 0;

  __attribute__((cleanup(c_simple_http_internal_cleanup_template_node)))
  C_SIMPLE_HTTP_INTERNAL_Template_Node *template_node = NULL;

  size_t delimeter_count = 0;

  // xxxx xxx0 - Initial state, no delimeter reached.
  // xxxx xxx1 - Three "{" delimeters reached.
  unsigned int state = 0;

  for (; idx < html_buf_size; ++idx) {
    if ((state & 1) == 0) {
      // Using 0x7B instead of left curly-brace due to bug in vim navigation.
      if (html_buf[idx] == 0x7B) {
        ++delimeter_count;
        if (delimeter_count >= 3) {
          delimeter_count = 0;
          state |= 1;
          if (template_html_list->count != 0) {
            C_SIMPLE_HTTP_INTERNAL_Template_Node *last_node =
              template_html_list->tail->prev->data;
            last_template_idx = last_node->orig_end_idx;
          }
          template_node = malloc(sizeof(C_SIMPLE_HTTP_INTERNAL_Template_Node));
          template_node->html_size = idx - last_template_idx - 2;
          template_node->html = malloc(template_node->html_size);
          memcpy(
            template_node->html,
            html_buf + last_template_idx,
            template_node->html_size);
          template_node->orig_end_idx = idx + 1;
          template_node->forced_next = NULL;
          simple_archiver_list_add(template_html_list, template_node,
            c_simple_http_internal_free_template_node);
          template_node = NULL;
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
          C_SIMPLE_HTTP_INTERNAL_Template_Node *last_node =
            template_html_list->tail->prev->data;
          size_t var_size = idx - 2 - last_node->orig_end_idx;
          __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
          char *var = malloc(var_size + 1);
          memcpy(
            var,
            html_buf + last_node->orig_end_idx,
            var_size);
          var[var_size] = 0;
          const char *value_c_str =
            simple_archiver_hash_map_get(
              wrapped_hash_map->hash_map,
              var,
              var_size + 1);
          // TODO Impl. loading from file instead of directly from value.
          // Perhaps do this if "var" ends with "_FILE".
          if (value_c_str) {
            template_node =
              malloc(sizeof(C_SIMPLE_HTTP_INTERNAL_Template_Node));
            size_t size = strlen(value_c_str);
            template_node->html = malloc(size);
            memcpy(template_node->html, value_c_str, size);
            template_node->html_size = size;
            template_node->orig_end_idx = idx + 1;
            template_node->forced_next = NULL;
          } else {
            template_node =
              malloc(sizeof(C_SIMPLE_HTTP_INTERNAL_Template_Node));
            template_node->html = NULL;
            template_node->html_size = 0;
            template_node->orig_end_idx = idx + 1;
            template_node->forced_next = NULL;
          }
          simple_archiver_list_add(template_html_list, template_node,
            c_simple_http_internal_free_template_node);
          template_node = NULL;
        }
      } else {
        delimeter_count = 0;
      }
    }
  }

  if (template_html_list->count != 0) {
    C_SIMPLE_HTTP_INTERNAL_Template_Node *last_node =
      template_html_list->tail->prev->data;
    if (idx > last_node->orig_end_idx) {
      template_node = malloc(sizeof(C_SIMPLE_HTTP_INTERNAL_Template_Node));
      size_t size = idx - last_node->orig_end_idx;
      template_node->html = malloc(size);
      memcpy(template_node->html, html_buf + last_node->orig_end_idx, size);
      template_node->html_size = size;
      template_node->orig_end_idx = idx + 1;
      template_node->forced_next = NULL;
      simple_archiver_list_add(template_html_list, template_node,
        c_simple_http_internal_free_template_node);
      template_node = NULL;

      last_node = template_html_list->tail->prev->data;
    }
    size_t final_size = 0;
    simple_archiver_list_get(
      template_html_list,
      c_simple_http_internal_get_final_size_fn,
      &final_size);
    if (final_size == 0) {
      fprintf(stderr, "ERROR final_size calculated as ZERO from templates!\n");
      return NULL;
    }
    C_SIMPLE_HTTP_INTERNAL_Template_Node to_fill;
    to_fill.html = malloc(final_size);
    to_fill.html_size = 0;
    to_fill.html_capacity = final_size;
    if (simple_archiver_list_get(
        template_html_list,
        c_simple_http_internal_fill_buf_fn,
        &to_fill) != NULL) {
      fprintf(stderr, "ERROR internal issue processing final html buffer!\n");
      free(to_fill.html);
      return NULL;
    }
    return to_fill.html;
  } else {
    // Prevent cleanup fn from "free"ing html_buf and return it verbatim.
    char *buf = html_buf;
    html_buf = NULL;
    return buf;
  }
}

// vim: ts=2 sts=2 sw=2