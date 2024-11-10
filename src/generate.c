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

#include "generate.h"

// Standard library includes.
#include <string.h>

// Linux/Unix includes.
#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

// Local includes.
#include "helpers.h"
#include "http_template.h"

// Third party includes.
#include <SimpleArchiver/src/helpers.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>
#include <SimpleArchiver/src/data_structures/linked_list.h>

int c_simple_http_generate_paths_fn(const void *key,
                                    size_t key_size,
                                    __attribute__((unused)) const void *value,
                                    void *ud) {
  const char *path = key;
  const ConnectionContext *ctx = ud;
  const char *generate_dir = ctx->args->generate_dir;

  const unsigned long path_len = key_size - 1;
  const unsigned long generate_dir_len = strlen(generate_dir);

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *string_parts = simple_archiver_list_init();

  // Add generate_dir as first path of paths to join.
  c_simple_http_add_string_part(string_parts, generate_dir, 0);

  // Ensure next character after generate_dir contains a '/' if generate_dir
  // didn't contain one at the end.
  if (generate_dir_len > 0 && generate_dir[generate_dir_len - 1] != '/') {
    c_simple_http_add_string_part(string_parts, "/", 0);
  }

  // Append the path.
  if (strcmp(path, "/") != 0) {
    // Is not root.
    uint32_t idx = 0;
    while (idx <= path_len && path[idx] == '/') {
      ++idx;
    }
    c_simple_http_add_string_part(string_parts, path + idx, 0);
  }

  // Add the final '/'.
  if (path_len > 0 && path[path_len - 1] != '/') {
    c_simple_http_add_string_part(string_parts, "/", 0);
  }

  // Add the ending "index.html".
  c_simple_http_add_string_part(string_parts, "index.html", 0);

  // Get the combined string.
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *generated_path = c_simple_http_combine_string_parts(string_parts);
  if (!generated_path) {
    fprintf(stderr, "ERROR Failed to get generated path (path: %s)!\n", path);
    return 1;
  }

  if ((ctx->args->flags & 4) == 0) {
    // Overwrite not enabled, check if file already exists.
    FILE *fd = fopen(generated_path, "rb");
    if (fd) {
      fclose(fd);
      fprintf(
        stderr,
        "WARNING Path \"%s\" exists and \"--generate-enable-overwrite\" not "
          "specified, skipping!\n",
        generated_path);
      return 0;
    }
  }

  // Ensure the required dirs exist.
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *generated_path_dup = strdup(generated_path);

  uint_fast8_t did_make_generated_path_dir = 0;
  char *generated_path_dir = dirname(generated_path_dup);
  if (generated_path_dir) {
    DIR *fd = opendir(generated_path_dir);
    if (!fd) {
      if (errno == ENOENT) {
        c_simple_http_helper_mkdir_tree(generated_path_dir);
        did_make_generated_path_dir = 1;
      } else {
        fprintf(stderr,
                "ERROR opendir on path dirname failed unexpectedly (path: %s)!"
                  "\n",
                path);
        return 1;
      }
    } else {
      // Directory already exists.
      closedir(fd);
    }
  } else {
    fprintf(stderr,
            "ERROR Failed to get dirname of generated path dir (path: %s)"
              "!\n",
            path);
    return 1;
  }

  // Generate the html.
  size_t html_buf_size = 0;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *html_buf = c_simple_http_path_to_generated(path,
                                                   ctx->parsed,
                                                   &html_buf_size,
                                                   NULL);
  if (!html_buf || html_buf_size == 0) {
    fprintf(stderr,
            "WARNING Failed to generate html for generate (path: %s), "
              "skipping!\n",
            path);
    if (did_make_generated_path_dir) {
      if (rmdir(generated_path_dir) == -1) {
        fprintf(stderr,
                "WARNING rmdir on generated_path_dir failed, errno: %d\n",
                errno);
      }
    }
    return 0;
  }

  // Save the html.
  FILE *fd = fopen(generated_path, "wb");
  if (!fd) {
    fprintf(stderr,
            "WARNING Failed to open \"%s\" for writing, skipping!\n",
            generated_path);
    return 0;
  }
  unsigned long fwrite_ret = fwrite(html_buf, 1, html_buf_size, fd);
  if (fwrite_ret < html_buf_size) {
    fclose(fd);
    unlink(generated_path);
    fprintf(stderr,
            "ERROR Unable to write entirely to \"%s\"!\n",
            generated_path);
    return 1;
  } else {
    fclose(fd);
  }

  return 0;
}
