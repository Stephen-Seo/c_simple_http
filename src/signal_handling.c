#include "signal_handling.h"

// Standard library includes.
#include <stdio.h>

// Unix includes.
#include <signal.h>

// Local includes
#include "globals.h"

void C_SIMPLE_HTTP_handle_sigint(int signal) {
  if (signal == SIGINT) {
#ifndef NDEBUG
    puts("Handling SIGINT");
#endif
    C_SIMPLE_HTTP_KEEP_RUNNING = 0;
  }
}

// vim: ts=2 sts=2 sw=2
