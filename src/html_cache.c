// ISC License
// 
// Copyright (c) 2024-2025 Stephen Seo
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// POSIX includes.
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// libc includes.
#include <time.h>

// Third-party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "http.h"
#include "helpers.h"
#include "http_template.h"

int c_simple_http_internal_write_filenames_to_cache_file(
    const void *key,
    __attribute__((unused)) size_t key_size,
    __attribute__((unused)) const void *value,
    void *ud) {
  const char *filename = key;
  FILE *cache_fd = ud;

  const size_t filename_size = strlen(filename);
  if (fwrite(filename, 1, filename_size, cache_fd) != filename_size) {
    return 1;
  } else if (fwrite("\n", 1, 1, cache_fd) != 1) {
    return 1;
  }

  return 0;
}

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
    C_SIMPLE_HTTP_HTTPTemplates *templates,
    size_t cache_entry_lifespan,
    char **buf_out) {
  if (!path) {
    fprintf(stderr, "ERROR cache_path function: path is NULL!\n");
    return -9;
  } else if (!config_filename) {
    fprintf(stderr, "ERROR cache_path function: config_filename is NULL!\n");
    return -10;
  } else if (!cache_dir) {
    fprintf(stderr, "ERROR cache_path function: cache_dir is NULL!\n");
    return -11;
  } else if (!templates) {
    fprintf(stderr, "ERROR cache_path function: templates is NULL!\n");
    return -12;
  } else if (!buf_out) {
    fprintf(stderr, "ERROR cache_path function: buf_out is NULL!\n");
    return -13;
  }

  int ret = c_simple_http_helper_mkdir_tree(cache_dir);
  if (ret != 0 && ret != 1) {
    fprintf(
      stderr, "ERROR failed to ensure cache_dir \"%s\" exists!\n", cache_dir);
    return -15;
  }

  // Get the cache filename from the path.
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *cache_filename = c_simple_http_path_to_cache_filename(path);
  if (!cache_filename) {
    fprintf(stderr, "ERROR Failed to convert path to cache_filename!");
    return -1;
  }

  // Combine the cache_dir with cache filename.
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *parts = simple_archiver_list_init();

  c_simple_http_add_string_part(parts, cache_dir, 0);
  c_simple_http_add_string_part(parts, "/", 0);
  c_simple_http_add_string_part(parts, cache_filename, 0);

  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *cache_filename_full = c_simple_http_combine_string_parts(parts);

  simple_archiver_list_free(&parts);
  parts = simple_archiver_list_init();

  if (!cache_filename_full) {
    fprintf(stderr, "ERROR Failed to create full-path to cache filename!\n");
    return -2;
  }

  // Get "stat" info on the cache filename.
  uint_fast8_t force_cache_update = 0;
  struct stat cache_file_stat;
  memset(&cache_file_stat, 0, sizeof(struct stat));
  ret = stat(cache_filename_full, &cache_file_stat);
  if (ret == -1) {
    if (errno == ENOENT) {
      fprintf(stderr, "NOTICE cache file doesn't exist, will create...\n");
    } else {
      fprintf(
        stderr,
        "ERROR getting stat info on file \"%s\" (errno %d)! "
        "Assuming out of date!\n",
        cache_filename_full,
        errno);
    }
    force_cache_update = 1;
  }

  // Get "stat" info on config file.
  struct stat config_file_stat;
  memset(&config_file_stat, 0, sizeof(struct stat));
  ret = stat(config_filename, &config_file_stat);
  if (ret == -1) {
    if (errno == ENOENT) {
      fprintf(
        stderr, "ERROR config file \"%s\" doesn't exist!\n", config_filename);
    } else {
      fprintf(
        stderr,
        "ERROR getting stat info on config file \"%s\" (errno %d)!\n",
        config_filename,
        errno);
    }
    return -3;
  }

  if (!force_cache_update) {
    do {
      // Check filenames in cache file.
      __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
      FILE *cache_fd = fopen(cache_filename_full, "r");
      const size_t buf_size = 1024;
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *buf = malloc(buf_size);

      // Check header.
      if (fread(buf, 1, 20, cache_fd) != 20) {
        fprintf(stderr, "ERROR Failed to read header from cache file!\n");
        return -14;
      } else if (strncmp(buf, "--- CACHE ENTRY ---\n", 20) != 0) {
        fprintf(
          stderr,
          "WARNING Cache is invalid (bad header), assuming out of date!\n");
        force_cache_update = 1;
        break;
      }

      // Check filenames.
      size_t buf_idx = 0;
      while(1) {
        ret = fgetc(cache_fd);
        if (ret == EOF) {
          fprintf(
            stderr, "WARNING Cache is invalid (EOF), assuming out of date!\n");
          force_cache_update = 1;
          break;
        } else if (ret == '\n') {
          // Got filename in "buf" of size "buf_idx".
          if (strncmp(buf, "--- BEGIN HTML ---", 18) == 0) {
            // Got end header instead of filename.
            break;
          } else if (buf_idx < buf_size) {
            buf[buf_idx++] = 0;
          } else {
            fprintf(
              stderr,
              "WARNING Cache is invalid (too large filename), assuming out of "
              "date!\n");
            force_cache_update = 1;
            break;
          }

          struct stat file_stat;
          ret = stat(buf, &file_stat);
          if (ret == -1) {
            if (errno == ENOENT) {
              fprintf(
                stderr,
                "WARNING Invalid filename cache entry \"%s\" (doesn't exist)! "
                "Assuming out of date!\n",
                buf);
              force_cache_update = 1;
              break;
            } else {
              fprintf(
                stderr,
                "WARNING Invalid filename cache entry \"%s\" (stat errno %d)! "
                "Assuming out of date!\n",
                buf,
                errno);
              force_cache_update = 1;
              break;
            }
          }

          if (cache_file_stat.st_mtim.tv_sec < file_stat.st_mtim.tv_sec
              || (cache_file_stat.st_mtim.tv_sec == file_stat.st_mtim.tv_sec
                 && cache_file_stat.st_mtim.tv_nsec
                    < file_stat.st_mtim.tv_nsec)) {
            // File is newer than cache.
            force_cache_update = 1;
            break;
          }

          buf_idx = 0;
        } else if (buf_idx < buf_size) {
          buf[buf_idx++] = (char)ret;
        } else {
          fprintf(
            stderr,
            "WARNING Cache is invalid (too large filename), assuming out of "
            "date!\n");
          force_cache_update = 1;
          break;
        }
      }
    } while(0);
  }

  // Compare modification times.
  struct timespec current_time;
  if (clock_gettime(CLOCK_REALTIME, &current_time) != 0) {
    memset(&current_time, 0, sizeof(struct timespec));
  }
CACHE_FILE_WRITE_CHECK:
  if (force_cache_update
      || cache_file_stat.st_mtim.tv_sec < config_file_stat.st_mtim.tv_sec
      || (cache_file_stat.st_mtim.tv_sec == config_file_stat.st_mtim.tv_sec
         && cache_file_stat.st_mtim.tv_nsec < config_file_stat.st_mtim.tv_nsec)
      || (current_time.tv_sec - cache_file_stat.st_mtim.tv_sec
         > (ssize_t)cache_entry_lifespan))
  {
    // Cache file is out of date.

    if (cache_file_stat.st_mtim.tv_sec < config_file_stat.st_mtim.tv_sec
        || (cache_file_stat.st_mtim.tv_sec == config_file_stat.st_mtim.tv_sec
           && cache_file_stat.st_mtim.tv_nsec
              < config_file_stat.st_mtim.tv_nsec))
    {
      // Reload config file.
      C_SIMPLE_HTTP_HTTPTemplates new_parsed_config =
        c_simple_http_parse_config(
          config_filename,
          "PATH",
          NULL);
      if (new_parsed_config.hash_map) {
        simple_archiver_hash_map_free(&templates->hash_map);
        *templates = new_parsed_config;
      }
    }

    __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
    FILE *cache_fd = fopen(cache_filename_full, "w");
    if (fwrite("--- CACHE ENTRY ---\n", 1, 20, cache_fd) != 20) {
      fprintf(
        stderr,
        "ERROR Failed to write to cache file \"%s\"!\n",
        cache_filename_full);
      return -5;
    }

    __attribute__((cleanup(simple_archiver_hash_map_free)))
    SDArchiverHashMap *used_filenames = NULL;

    size_t generated_html_size = 0;

    __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
    char *generated_html = c_simple_http_path_to_generated(
        path, templates, &generated_html_size, &used_filenames);

    if (!generated_html) {
      fprintf(stderr, "ERROR Failed to generate html for path \"%s\"!\n", path);
      simple_archiver_helper_cleanup_FILE(&cache_fd);
      remove(cache_filename_full);
      return -4;
    }

    if (simple_archiver_hash_map_iter(
          used_filenames,
          c_simple_http_internal_write_filenames_to_cache_file,
          cache_fd) != 0) {
      fprintf(stderr, "ERROR Failed to write filenames to cache file!\n");
      return -6;
    } else if (fwrite("--- BEGIN HTML ---\n", 1, 19, cache_fd) != 19) {
      fprintf(stderr, "ERROR Failed to write end of cache file header!\n");
      return -7;
    } else if (
        fwrite(
          generated_html,
          1,
          generated_html_size,
          cache_fd)
        != generated_html_size) {
      fprintf(stderr, "ERROR Failed to write html to cache file!\n");
      return -8;
    }

    *buf_out = generated_html;
    generated_html = NULL;
    return 1;
  }

  // Cache file is newer.
  __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
  FILE *cache_fd = fopen(cache_filename_full, "rb");

  const size_t buf_size = 128;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *buf = malloc(buf_size);

  // Get first header.
  if (fread(buf, 1, 20, cache_fd) != 20) {
    fprintf(
      stderr,
      "WARNING Invalid cache file (read header), assuming out of date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  } else if (strncmp("--- CACHE ENTRY ---\n", buf, 20) != 0) {
    fprintf(
      stderr,
      "WARNING Invalid cache file (check header), assuming out of date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  // Get filenames end header.
  uint_fast8_t reached_end_header = 0;
  size_t buf_idx = 0;
  while (1) {
    ret = fgetc(cache_fd);
    if (ret == EOF) {
      fprintf(
        stderr, "WARNING Invalid cache file (EOF), assuming out of date!\n");
      force_cache_update = 1;
      goto CACHE_FILE_WRITE_CHECK;
    } else if (ret == '\n') {
      if (strncmp("--- BEGIN HTML ---", buf, 18) == 0) {
        reached_end_header = 1;
        break;
      }
      buf_idx = 0;
      continue;
    }

    if (buf_idx < buf_size) {
      buf[buf_idx++] = (char)ret;
    }
  }

  if (!reached_end_header) {
    fprintf(
      stderr,
      "WARNING Invalid cache file (no end header), assuming out of date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  // Remaining bytes in cache_fd is cached html. Fetch it and return it.
  const long html_start_idx = ftell(cache_fd);
  if (html_start_idx <= 0) {
    fprintf(
      stderr,
      "WARNING Failed to get position in cache file, assuming "
      "invalid/out-of-date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  ret = fseek(cache_fd, 0, SEEK_END);
  if (ret != 0) {
    fprintf(
      stderr,
      "WARNING Failed to seek in cache file, assuming invalid/out-of-date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }
  const long html_end_idx = ftell(cache_fd);
  if (html_end_idx <= 0) {
    fprintf(
      stderr,
      "WARNING Failed to get end position in cache file, assuming "
      "invalid/out-of-date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  ret = fseek(cache_fd, html_start_idx, SEEK_SET);
  if (ret != 0) {
    fprintf(
      stderr,
      "WARNING Failed to seek in cache file, assuming invalid/out-of-date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  const size_t html_size = (size_t)html_end_idx - (size_t)html_start_idx + 1;
  *buf_out = malloc(html_size);

  if (fread(*buf_out, 1, html_size - 1, cache_fd) != html_size - 1) {
    fprintf(
      stderr,
      "WARNING Failed to read html in cache file, assuming "
      "invalid/out-of-date!\n");
    force_cache_update = 1;
    goto CACHE_FILE_WRITE_CHECK;
  }

  (*buf_out)[html_size - 1] = 0;

  return 0;
}

// vim: et ts=2 sts=2 sw=2
