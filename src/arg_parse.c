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
