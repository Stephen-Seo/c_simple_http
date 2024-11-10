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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_ARG_PARSE_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_ARG_PARSE_H_

// Standard library includes.
#include <stdint.h>

// Third party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>

typedef struct Args {
  // xxxx xxx0 - enable peer addr print.
  // xxxx xxx1 - disable peer addr print.
  // xxxx xx0x - disable listen on config file for reloading.
  // xxxx xx1x - enable listen on config file for reloading.
  // xxxx x1xx - enable overwrite on generate.
  uint16_t flags;
  uint16_t port;
  // Does not need to be free'd, this should point to a string in argv.
  const char *config_file;
  // Needs to be free'd.
  SDArchiverLinkedList *list_of_headers_to_log;
  // Non-NULL if cache-dir is specified and cache is to be used.
  // Does not need to be free'd since it points to a string in argv.
  const char *cache_dir;
  size_t cache_lifespan_seconds;
  // Non-NULL if static-dir is specified and files in the dir are to be served.
  // Does not need to be free'd since it points to a string in argv.
  const char *static_dir;
  // Non-NULL if generate-dir is specified.
  // Does not need to be free'd since it points to a string in argv.
  const char *generate_dir;
} Args;

void print_usage(void);

Args parse_args(int argc, char **argv);

void c_simple_http_free_args(Args *args);

#endif

// vim: et ts=2 sts=2 sw=2
