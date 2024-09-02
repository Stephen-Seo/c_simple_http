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

// Standard library includes.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Unix includes.
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

// Local includes.
#include "arg_parse.h"
#include "big_endian.h"
#include "tcp_socket.h"
#include "signal_handling.h"
#include "globals.h"
#include "constants.h"
#include "http.h"

int main(int argc, char **argv) {
  Args args = parse_args(argc, argv);

  printf("%u\n", args.port);

  __attribute__((cleanup(cleanup_tcp_socket))) int tcp_socket =
    create_tcp_socket(args.port);
  if (tcp_socket == -1) {
    return 1;
  }

  struct timespec sleep_time;
  sleep_time.tv_sec = 0;
  sleep_time.tv_nsec = C_SIMPLE_HTTP_SLEEP_NANOS;

  struct sockaddr_in6 peer_info;
  memset(&peer_info, 0, sizeof(struct sockaddr_in6));
  peer_info.sin6_family = AF_INET6;

  signal(SIGINT, C_SIMPLE_HTTP_handle_sigint);

  unsigned char recv_buf[C_SIMPLE_HTTP_RECV_BUF_SIZE];

  int ret;
  ssize_t read_ret;
  socklen_t socket_len;

  while (C_SIMPLE_HTTP_KEEP_RUNNING) {
    nanosleep(&sleep_time, NULL);
    socket_len = sizeof(struct sockaddr_in6);
    ret = accept(tcp_socket, (struct sockaddr *)&peer_info, &socket_len);
    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // No connecting peers, do nothing.
    } else if (ret == -1) {
      printf("WARNING: accept: errno %d\n", errno);
    } else if (ret >= 0) {
      // Received connection, handle it.
      printf("Peer connected: addr is ");
      for (unsigned int idx = 0; idx < 16; ++idx) {
        if (idx % 2 == 0 && idx > 0) {
          printf(":");
        }
        printf("%02x", peer_info.sin6_addr.s6_addr[idx]);
      }
      puts("");
      int connection_fd = ret;
      read_ret = read(connection_fd, recv_buf, C_SIMPLE_HTTP_RECV_BUF_SIZE);
      // DEBUG print received buf.
      for (unsigned int idx = 0;
          idx < C_SIMPLE_HTTP_RECV_BUF_SIZE && idx < read_ret;
          ++idx) {
        if ((recv_buf[idx] >= 0x20 && recv_buf[idx] <= 0x7E)
            || recv_buf[idx] == '\n' || recv_buf[idx] == '\r') {
          printf("%c", recv_buf[idx]);
        } else {
          break;
        }
      }
      puts("");
      // TODO Validate request and send response.
      // TODO WIP
      const char *response = c_simple_http_request_response(
        (const char*)recv_buf, read_ret, NULL);
      if (response) {
        free((void*)response);
      }
      close(connection_fd);
    } else {
      printf("WARNING: accept: Unknown invalid state!\n");
    }
#ifndef NDEBUG
    //printf(".");
    //fflush(stdout);
#endif
  }

  printf("End of program.\n");
  return 0;
}

// vim: ts=2 sts=2 sw=2
