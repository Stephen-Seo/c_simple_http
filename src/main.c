// Standard library includes.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Unix includes.
#include <signal.h>
#include <time.h>

// Local includes.
#include "arg_parse.h"
#include "big_endian.h"
#include "tcp_socket.h"
#include "signal_handling.h"
#include "globals.h"

#define C_SIMPLE_HTTP_SLEEP_NANOS 1000000

int main(int argc, char **argv) {
  Args args = parse_args(argc, argv);

  printf("%u\n", args.port);

  __attribute__((cleanup(cleanup_tcp_socket))) int tcp_socket = create_tcp_socket(args.port);

  struct timespec sleep_time;
  sleep_time.tv_sec = 0;
  sleep_time.tv_nsec = C_SIMPLE_HTTP_SLEEP_NANOS;
  signal(SIGINT, C_SIMPLE_HTTP_handle_sigint);
  while (C_SIMPLE_HTTP_KEEP_RUNNING) {
    nanosleep(&sleep_time, NULL);
#ifndef NDEBUG
    printf(".");
    fflush(stdout);
#endif
  }

  printf("End of program.\n");
  return 0;
}

// vim: ts=2 sts=2 sw=2
