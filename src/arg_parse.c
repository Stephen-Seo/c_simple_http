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

void print_usage(void) {
  puts("Usage:");
  puts("  -p <port> | --port <port>");
}

Args parse_args(int argc, char **argv) {
  --argc;
  ++argv;

  Args args;
  memset(&args, 0, sizeof(Args));

  while (argc > 0) {
    if ((strcmp(argv[0], "-p") == 0 || strcmp(argv[0], "--port") == 0)
        && argc > 1) {
      int value = atoi(argv[1]);
      if (value >= 0 && value <= 0xFFFF) {
        args.port = (unsigned short) value;
      }
      --argc;
      ++argv;
    } else {
      puts("ERROR: Invalid args!\n");
      print_usage();
      exit(1);
    }

    --argc;
    ++argv;
  }

  return args;
}

// vim: ts=2 sts=2 sw=2
