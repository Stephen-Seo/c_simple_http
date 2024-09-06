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

// Third party includes.
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "arg_parse.h"
#include "big_endian.h"
#include "tcp_socket.h"
#include "signal_handling.h"
#include "globals.h"
#include "constants.h"
#include "http.h"

#define CHECK_ERROR_WRITE(write_expr) \
  if (write_expr < 0) { \
    close(connection_fd); \
    fprintf(stderr, "ERROR Failed to write to connected peer, closing...\n"); \
    continue; \
  }

int main(int argc, char **argv) {
  Args args = parse_args(argc, argv);

  if (!args.config_file) {
    fprintf(stderr, "ERROR Config file not specified!\n");
    print_usage();
    return 2;
  }

  printf("Config file is: %s\n", args.config_file);

  __attribute__((cleanup(c_simple_http_clean_up_parsed_config)))
  C_SIMPLE_HTTP_ParsedConfig parsed_config = c_simple_http_parse_config(
    args.config_file,
    "PATH",
    NULL
  );

  __attribute__((cleanup(cleanup_tcp_socket))) int tcp_socket =
    create_tcp_socket(args.port);
  if (tcp_socket == -1) {
    return 1;
  }

  {
    struct sockaddr_in6 ipv6_addr;
    memset(&ipv6_addr, 0, sizeof(struct sockaddr_in6));
    socklen_t size = sizeof(ipv6_addr);
    int ret = getsockname(tcp_socket, (struct sockaddr*)&ipv6_addr, &size);
    if (ret == 0) {
      printf("Listening on port: %u\n", u16_be_swap(ipv6_addr.sin6_port));
    } else {
      fprintf(
        stderr,
        "WARNING Failed to get info on tcp socket to print port number!\n");
    }
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
      if (read_ret < 0) {
        close(connection_fd);
        fprintf(
          stderr,
          "WARNING Failed to read from new connection, closing...\n");
        continue;
      }
      // DEBUG print received buf.
#ifndef NDEBUG
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
#endif

      size_t response_size = 0;
      enum C_SIMPLE_HTTP_ResponseCode response_code;
      const char *response = c_simple_http_request_response(
        (const char*)recv_buf,
        (unsigned int)read_ret,
        &parsed_config,
        &response_size,
        &response_code);
      if (response && response_code == C_SIMPLE_HTTP_Response_200_OK) {
        CHECK_ERROR_WRITE(write(connection_fd, "HTTP/1.1 200 OK\n", 16));
        CHECK_ERROR_WRITE(write(connection_fd, "Allow: GET\n", 11));
        CHECK_ERROR_WRITE(write(
          connection_fd, "Content-Type: text/html\n", 24));
        char content_length_buf[128];
        size_t content_length_buf_size = 0;
        memcpy(content_length_buf, "Content-Length: ", 16);
        content_length_buf_size = 16;
        int written = 0;
        snprintf(
          content_length_buf + content_length_buf_size,
          127 - content_length_buf_size,
          "%lu\n%n",
          response_size,
          &written);
        if (written <= 0) {
          close(connection_fd);
          fprintf(
            stderr,
            "WARNING Failed to write in response, closing connection...\n");
          continue;
        }
        content_length_buf_size += (size_t)written;
        CHECK_ERROR_WRITE(write(
          connection_fd, content_length_buf, content_length_buf_size));
        CHECK_ERROR_WRITE(write(connection_fd, "\n", 1));
        CHECK_ERROR_WRITE(write(connection_fd, response, response_size));

        free((void*)response);
      } else {
        const char *response = c_simple_http_response_code_error_to_response(
          response_code);
        size_t response_size = strlen(response);
        if (response) {
          CHECK_ERROR_WRITE(write(connection_fd, response, response_size));
        } else {
          CHECK_ERROR_WRITE(write(
            connection_fd, "HTTP/1.1 404 Not Found\n", 23));
          CHECK_ERROR_WRITE(write(connection_fd, "Allow: GET\n", 11));
          CHECK_ERROR_WRITE(write(
            connection_fd, "Content-Type: text/html\n", 24));
          CHECK_ERROR_WRITE(write(connection_fd, "Content-Length: 35\n", 19));
          CHECK_ERROR_WRITE(write(
            connection_fd, "\n<h1>500 Internal Server Error</h1>\n", 36));
        }
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
