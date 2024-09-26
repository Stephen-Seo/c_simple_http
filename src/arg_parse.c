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

#include "arg_parse.h"

// Standard library includes.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// libc includes.
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

// Posix includes.
#include <sys/stat.h>

// Local includes.
#include "constants.h"

void print_usage(void) {
  puts("Usage:");
  puts("  -p <port> | --port <port>");
  puts("  --config=<config_file>");
  puts("  --disable-peer-addr-print");
  puts("  --req-header-to-print=<header> (can be used multiple times)");
  puts("    For example: --req-header-to-print=User-Agent");
  puts("    Note that this option is case-insensitive");
  puts("  --enable-reload-config-on-change");
  puts("  --enable-cache-dir=<DIR>");
  puts("  --cache-entry-lifetime-seconds=<SECONDS>");
}

Args parse_args(int32_t argc, char **argv) {
  --argc;
  ++argv;

  __attribute__((cleanup(c_simple_http_free_args)))
  Args args;
  memset(&args, 0, sizeof(Args));
  args.list_of_headers_to_log = simple_archiver_list_init();
  args.cache_lifespan_seconds = C_SIMPLE_HTTP_DEFAULT_CACHE_LIFESPAN_SECONDS;

  while (argc > 0) {
    if ((strcmp(argv[0], "-p") == 0 || strcmp(argv[0], "--port") == 0)
        && argc > 1) {
      int32_t value = atoi(argv[1]);
      if (value >= 0 && value <= 0xFFFF) {
        args.port = (uint16_t) value;
      }
      --argc;
      ++argv;
    } else if (strncmp(argv[0], "--config=", 9) == 0 && strlen(argv[0]) > 9) {
      args.config_file = argv[0] + 9;
    } else if (strcmp(argv[0], "--disable-peer-addr-print") == 0) {
      args.flags |= 1;
    } else if (strncmp(argv[0], "--req-header-to-print=", 22) == 0
        && strlen(argv[0]) > 22) {
      size_t arg_size = strlen(argv[0]) + 1 - 22;
      char *header_buf = malloc(arg_size);
      memcpy(header_buf, argv[0] + 22, arg_size);
      if (simple_archiver_list_add(
            args.list_of_headers_to_log, header_buf, NULL)
          != 0) {
        fprintf(
          stderr, "ERROR Failed to parse \"--req-header-to-print=...\" !\n");
        free(header_buf);
        exit(1);
      }
    } else if (strcmp(argv[0], "--enable-reload-config-on-change") == 0) {
      args.flags |= 2;
    } else if (strncmp(argv[0], "--enable-cache-dir=", 19) == 0) {
      args.cache_dir = argv[0] + 19;
      // Check if it actually is an existing directory.
      DIR *d = opendir(args.cache_dir);
      if (d == NULL) {
        if (errno == ENOENT) {
          printf(
            "Directory \"%s\" doesn't exist, creating it...\n",
            args.cache_dir);
          int ret = mkdir(
              args.cache_dir,
              S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
          if (ret == -1) {
            fprintf(
              stderr,
              "ERROR Failed to create new directory (errno %d)\n",
              errno);
            exit(1);
          }
        } else {
          fprintf(
            stderr,
            "ERROR Failed to open directory \"%s\" (errno %d)!\n",
            args.cache_dir,
            errno);
          exit(1);
        }
      } else {
        printf("Directory \"%s\" exists.\n", args.cache_dir);
      }
      closedir(d);
    } else if (strncmp(argv[0], "--cache-entry-lifetime-seconds=", 31) == 0) {
      args.cache_lifespan_seconds = strtoul(argv[0] + 31, NULL, 10);
      if (args.cache_lifespan_seconds == 0) {
        fprintf(
          stderr,
          "ERROR: Invalid --cache-entry-lifetime-seconds=%s entry!\n",
          argv[0] + 31);
        print_usage();
        exit(1);
      } else {
        printf(
          "NOTICE set cache-entry-lifetime to %lu\n",
          args.cache_lifespan_seconds);
      }
    } else {
      fprintf(stderr, "ERROR: Invalid args!\n");
      print_usage();
      exit(1);
    }

    --argc;
    ++argv;
  }

  // Prevent freeing of Args due to successful parsing.
  Args to_return = args;
  args.list_of_headers_to_log = NULL;
  return to_return;
}

void c_simple_http_free_args(Args *args) {
  if (args) {
    if (args->list_of_headers_to_log) {
      simple_archiver_list_free(&args->list_of_headers_to_log);
    }
  }
}

// vim: et ts=2 sts=2 sw=2
