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

void C_SIMPLE_HTTP_handle_sigusr1(int signal) {
  if (signal == SIGUSR1) {
#ifndef NDEBUG
    puts("Handling SIGUSR1");
#endif
    C_SIMPLE_HTTP_SIGUSR1_SET = 1;
  }
}

void C_SIMPLE_HTTP_handle_sigpipe(int signal) {
  if (signal == SIGPIPE) {
#ifndef NDEBUG
    fprintf(stderr, "WARNING Recieved SIGPIPE\n");
#endif
  }
}

// vim: et ts=2 sts=2 sw=2
