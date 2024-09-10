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

// Linux/Unix includes.
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/inotify.h>
#include <linux/limits.h>

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
#include "helpers.h"

#define CHECK_ERROR_WRITE(write_expr) \
  if (write_expr < 0) { \
    close(connection_fd); \
    fprintf(stderr, "ERROR Failed to write to connected peer, closing...\n"); \
    continue; \
  }

int c_simple_http_headers_check_print(void *data, void *ud) {
  SDArchiverHashMap *headers_map = ud;
  const char *header_c_str = data;

  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *header_c_str_lowercase = c_simple_http_helper_to_lowercase(
    header_c_str, strlen(header_c_str));
  char *matching_line = simple_archiver_hash_map_get(
    headers_map,
    header_c_str_lowercase,
    strlen(header_c_str_lowercase) + 1);
  if (matching_line) {
    printf("Printing header line: %s\n", matching_line);
  }
  return 0;
}

void c_simple_http_inotify_fd_cleanup(int *fd) {
  if (fd && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

int main(int argc, char **argv) {
  __attribute__((cleanup(c_simple_http_free_args)))
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
  if (!parsed_config.hash_map) {
    return 5;
  }

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

  __attribute__((cleanup(c_simple_http_inotify_fd_cleanup)))
  int inotify_config_fd = -1;
  __attribute__((cleanup(simple_archiver_helper_cleanup_malloced)))
  void *inotify_event_buf = NULL;
  struct inotify_event *inotify_event = NULL;
  const size_t inotify_event_buf_size =
    sizeof(struct inotify_event) + NAME_MAX + 1;
  if ((args.flags & 0x2) != 0) {
    inotify_config_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_config_fd < 0) {
      fprintf(stderr, "ERROR Failed to init listen on config file for hot "
        "reloading! (error code \"%d\")\n", errno);
      return 3;
    }

    if (inotify_add_watch(
        inotify_config_fd,
        args.config_file,
        IN_MODIFY | IN_CLOSE_WRITE) == -1) {
      fprintf(stderr, "ERROR Failed to set up listen on config file for hot "
        "reloading! (error code \"%d\")\n", errno);
      return 4;
    }

    inotify_event_buf = malloc(inotify_event_buf_size);
    inotify_event = inotify_event_buf;
  }

  struct timespec sleep_time;
  sleep_time.tv_sec = 0;
  sleep_time.tv_nsec = C_SIMPLE_HTTP_SLEEP_NANOS;

  struct sockaddr_in6 peer_info;
  memset(&peer_info, 0, sizeof(struct sockaddr_in6));
  peer_info.sin6_family = AF_INET6;

  signal(SIGINT, C_SIMPLE_HTTP_handle_sigint);
  signal(SIGUSR1, C_SIMPLE_HTTP_handle_sigusr1);

  unsigned char recv_buf[C_SIMPLE_HTTP_RECV_BUF_SIZE];

  int ret;
  ssize_t read_ret;
  socklen_t socket_len;

  // xxxx xxx1 - config needs to be reloaded.
  unsigned int flags = 0;
  size_t config_try_reload_ticks_count = 0;
  unsigned int config_try_reload_attempts = 0;

  while (C_SIMPLE_HTTP_KEEP_RUNNING) {
    nanosleep(&sleep_time, NULL);
    if ((flags & 0x1) != 0) {
      ++config_try_reload_ticks_count;
      if (config_try_reload_ticks_count
          >= C_SIMPLE_HTTP_TRY_CONFIG_RELOAD_TICKS) {
        config_try_reload_ticks_count = 0;
        ++config_try_reload_attempts;
        fprintf(
          stderr,
          "Attempting to reload config now (try %u of %u)...\n",
          config_try_reload_attempts,
          C_SIMPLE_HTTP_TRY_CONFIG_RELOAD_MAX_ATTEMPTS);
        C_SIMPLE_HTTP_ParsedConfig new_parsed_config =
          c_simple_http_parse_config(
            args.config_file,
            "PATH",
            NULL);
        if (new_parsed_config.hash_map) {
          c_simple_http_clean_up_parsed_config(&parsed_config);
          parsed_config = new_parsed_config;
          flags &= 0xFFFFFFFE;
          fprintf(stderr, "Reloaded config.\n");
          if (inotify_add_watch(
            inotify_config_fd,
            args.config_file,
            IN_MODIFY | IN_CLOSE_WRITE) == -1) {
            fprintf(
              stderr,
              "WARNING Failed to set listen on config, autoreloading "
              "later...\n");
            flags |= 1;
          } else {
            config_try_reload_attempts = 0;
          }
        } else {
          c_simple_http_clean_up_parsed_config(&new_parsed_config);
          if (config_try_reload_attempts
              >= C_SIMPLE_HTTP_TRY_CONFIG_RELOAD_MAX_ATTEMPTS) {
            fprintf(stderr, "ERROR Attempted to reload config too many times,"
              " stopping!\n");
            return 6;
          }
        }
      }
    }

    if (C_SIMPLE_HTTP_SIGUSR1_SET) {
      // Handle hot-reloading of config file due to SIGUSR1.
      C_SIMPLE_HTTP_SIGUSR1_SET = 0;
      fprintf(stderr, "NOTICE SIGUSR1, reloading config file...\n");
      C_SIMPLE_HTTP_ParsedConfig new_parsed_config = c_simple_http_parse_config(
        args.config_file,
        "PATH",
        NULL);
      if (new_parsed_config.hash_map) {
        c_simple_http_clean_up_parsed_config(&parsed_config);
        parsed_config = new_parsed_config;
      } else {
        fprintf(
          stderr, "WARNING New config is invalid, keeping old config...\n");
        c_simple_http_clean_up_parsed_config(&new_parsed_config);
      }
    }
    if ((args.flags & 0x2) != 0) {
      // Handle hot-reloading of config file.
      read_ret =
        read(inotify_config_fd, inotify_event_buf, inotify_event_buf_size);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // No events, do nothing.
        } else {
          fprintf(
            stderr,
            "WARNING Error code \"%d\" on config file listen for hot "
            "reloading!\n",
            errno);
        }
      } else if (read_ret > 0) {
#ifndef NDEBUG
        printf("DEBUG inotify_event->mask: %x\n", inotify_event->mask);
#endif
        if ((inotify_event->mask & IN_MODIFY) != 0
            || (inotify_event->mask & IN_CLOSE_WRITE) != 0) {
          fprintf(stderr, "NOTICE Config file modified, reloading...\n");
          C_SIMPLE_HTTP_ParsedConfig new_parsed_config =
            c_simple_http_parse_config(
              args.config_file,
              "PATH",
              NULL);
          if (new_parsed_config.hash_map) {
            c_simple_http_clean_up_parsed_config(&parsed_config);
            parsed_config = new_parsed_config;
          } else {
            fprintf(
              stderr, "WARNING New config is invalid, keeping old config...\n");
            c_simple_http_clean_up_parsed_config(&new_parsed_config);
          }
        } else if ((inotify_event->mask & IN_IGNORED) != 0) {
          fprintf(
            stderr,
            "NOTICE Config file modified (IN_IGNORED), reloading...\n");
          C_SIMPLE_HTTP_ParsedConfig new_parsed_config =
            c_simple_http_parse_config(
              args.config_file,
              "PATH",
              NULL);
          if (new_parsed_config.hash_map) {
            c_simple_http_clean_up_parsed_config(&parsed_config);
            parsed_config = new_parsed_config;
          } else {
            fprintf(
              stderr, "WARNING New config is invalid, keeping old config...\n");
            c_simple_http_clean_up_parsed_config(&new_parsed_config);
          }
          // Re-initialize inotify.
          //c_simple_http_inotify_fd_cleanup(&inotify_config_fd);
          //inotify_config_fd = inotify_init1(IN_NONBLOCK);
          if (inotify_add_watch(
            inotify_config_fd,
            args.config_file,
            IN_MODIFY | IN_CLOSE_WRITE) == -1) {
            fprintf(
              stderr,
              "WARNING Failed to set listen on config, autoreloading "
              "later...\n");
            flags |= 1;
          }
        }
      }
    }

    socket_len = sizeof(struct sockaddr_in6);
    ret = accept(tcp_socket, (struct sockaddr *)&peer_info, &socket_len);
    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // No connecting peers, do nothing.
    } else if (ret == -1) {
      printf("WARNING: accept: errno %d\n", errno);
    } else if (ret >= 0) {
      // Received connection, handle it.
      if ((args.flags & 1) == 0) {
        printf("Peer connected: addr is ");
        for (unsigned int idx = 0; idx < 16; ++idx) {
          if (idx % 2 == 0 && idx > 0) {
            printf(":");
          }
          printf("%02x", peer_info.sin6_addr.s6_addr[idx]);
        }
        puts("");
      } else {
        printf("Peer connected.\n");
      }
      int connection_fd = ret;
      read_ret = read(connection_fd, recv_buf, C_SIMPLE_HTTP_RECV_BUF_SIZE);
      if (read_ret < 0) {
        close(connection_fd);
        fprintf(
          stderr,
          "WARNING Failed to read from new connection, closing...\n");
        continue;
      }
#ifndef NDEBUG
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
#endif
      {
        SDArchiverHashMap *headers_map = c_simple_http_request_to_headers_map(
          (const char*)recv_buf,
          (size_t)read_ret);
        simple_archiver_list_get(
          args.list_of_headers_to_log,
          c_simple_http_headers_check_print,
          headers_map);
        simple_archiver_hash_map_free(&headers_map);
      }

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
        if (response) {
          response_size = strlen(response);
          CHECK_ERROR_WRITE(write(connection_fd, response, response_size));
        } else {
          CHECK_ERROR_WRITE(write(
            connection_fd, "HTTP/1.1 500 Internal Server Error\n", 35));
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

// vim: et ts=2 sts=2 sw=2
