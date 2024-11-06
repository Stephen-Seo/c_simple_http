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
#include <stdint.h>

// Linux/Unix includes.
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <fcntl.h>

// Third party includes.
#include <SimpleArchiver/src/helpers.h>

// Local includes.
#include "arg_parse.h"
#include "big_endian.h"
#include "config.h"
#include "tcp_socket.h"
#include "signal_handling.h"
#include "globals.h"
#include "constants.h"
#include "http.h"
#include "helpers.h"
#include "static.h"

#define CHECK_ERROR_WRITE(connection_fd, write_expr) \
  if (write_expr < 0) { \
    close(connection_fd); \
    fprintf(stderr, "ERROR Failed to write to connected peer, closing...\n"); \
    return 1; \
  }

#define CHECK_ERROR_WRITE_NO_FD(write_expr) \
  if (write_expr < 0) { \
    fprintf(stderr, "ERROR Failed to write to connected peer, closing...\n"); \
    return 1; \
  }

void c_simple_http_print_ipv6_addr(FILE *out, const struct in6_addr *addr) {
  for (uint32_t idx = 0; idx < 16; ++idx) {
    if (idx % 2 == 0 && idx > 0) {
      fprintf(out, ":");
    }
    fprintf(out, "%02x", addr->s6_addr[idx]);
  }
}

typedef struct ConnectionItem {
  int fd;
  struct timespec time_point;
  struct in6_addr peer_addr;
} ConnectionItem;

typedef struct ConnectionContext {
  char *buf;
  const Args *args;
  C_SIMPLE_HTTP_ParsedConfig *parsed;
  struct timespec current_time;
} ConnectionContext;

void c_simple_http_cleanup_connection_item(void *data) {
  ConnectionItem *citem = data;
  if (citem) {
#ifndef NDEBUG
    fprintf(stderr, "Closed connection to peer ");
    c_simple_http_print_ipv6_addr(stderr, &citem->peer_addr);
#endif
    if (citem->fd >= 0) {
      close(citem->fd);
#ifndef NDEBUG
      fprintf(stderr, ", fd %d\n", citem->fd);
#endif
      citem->fd = -1;
    } else {
#ifndef NDEBUG
      fprintf(stderr, "\n");
#endif
    }
    free(citem);
  }
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

int c_simple_http_on_error(
    enum C_SIMPLE_HTTP_ResponseCode response_code,
    int connection_fd
) {
  const char *response = c_simple_http_response_code_error_to_response(
    response_code);
  size_t response_size;
  if (response) {
    response_size = strlen(response);
    CHECK_ERROR_WRITE(connection_fd,
                      write(connection_fd, response, response_size));
  } else {
    CHECK_ERROR_WRITE(connection_fd, write(
      connection_fd, "HTTP/1.1 500 Internal Server Error\n", 35));
    CHECK_ERROR_WRITE(connection_fd, write(connection_fd, "Allow: GET\n", 11));
    CHECK_ERROR_WRITE(connection_fd,
                      write(connection_fd, "Connection: close\n", 18));
    CHECK_ERROR_WRITE(connection_fd,
                      write(connection_fd, "Content-Type: text/html\n", 24));
    CHECK_ERROR_WRITE(connection_fd,
                      write(connection_fd, "Content-Length: 35\n", 19));
    CHECK_ERROR_WRITE(connection_fd,
                      write(connection_fd,
                            "\n<h1>500 Internal Server Error</h1>\n",
                            36));
  }

  return 0;
}

int c_simple_http_manage_connections(void *data, void *ud) {
  ConnectionItem *citem = data;
  ConnectionContext *ctx = ud;
  char *recv_buf = ctx->buf;
  const Args *args = ctx->args;
  C_SIMPLE_HTTP_ParsedConfig *parsed = ctx->parsed;

  if (ctx->current_time.tv_sec - citem->time_point.tv_sec
      >= C_SIMPLE_HTTP_CONNECTION_TIMEOUT_SECONDS) {
    fprintf(stderr, "Peer ");
    c_simple_http_print_ipv6_addr(stderr, &citem->peer_addr);
    fprintf(stderr, " timed out.\n");
    return 1;
  }

  ssize_t read_ret = read(citem->fd, recv_buf, C_SIMPLE_HTTP_RECV_BUF_SIZE);
  if (read_ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    } else {
      fprintf(stderr, "Peer ");
      c_simple_http_print_ipv6_addr(stderr, &citem->peer_addr);
      fprintf(stderr, " error.\n");
      return 1;
    }
  }

#ifndef NDEBUG
  // DEBUG print received buf.
  for (uint32_t idx = 0;
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
      args->list_of_headers_to_log,
      c_simple_http_headers_check_print,
      headers_map);
    simple_archiver_hash_map_free(&headers_map);
  }

  size_t response_size = 0;
  enum C_SIMPLE_HTTP_ResponseCode response_code;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *request_path = NULL;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *response = c_simple_http_request_response(
    (const char*)recv_buf,
    (uint32_t)read_ret,
    parsed,
    &response_size,
    &response_code,
    args,
    &request_path);
  if (response && response_code == C_SIMPLE_HTTP_Response_200_OK) {
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "HTTP/1.1 200 OK\n", 16));
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "Allow: GET\n", 11));
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "Connection: close\n", 18));
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "Content-Type: text/html\n", 24));
    char content_length_buf[128];
    size_t content_length_buf_size = 0;
    memcpy(content_length_buf, "Content-Length: ", 16);
    content_length_buf_size = 16;
    int32_t written = 0;
    snprintf(
      content_length_buf + content_length_buf_size,
      127 - content_length_buf_size,
      "%lu\n%n",
      response_size,
      &written);
    if (written <= 0) {
      close(citem->fd);
      fprintf(
        stderr,
        "WARNING Failed to write in response, closing connection...\n");
      return 1;
    }
    content_length_buf_size += (size_t)written;
    CHECK_ERROR_WRITE_NO_FD(write(
      citem->fd, content_length_buf, content_length_buf_size));
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "\n", 1));
    CHECK_ERROR_WRITE_NO_FD(write(citem->fd, response, response_size));
  } else if (
      response_code == C_SIMPLE_HTTP_Response_404_Not_Found
      && args->static_dir) {
    __attribute__((cleanup(c_simple_http_cleanup_static_file_info)))
    C_SIMPLE_HTTP_StaticFileInfo file_info =
      c_simple_http_get_file(args->static_dir, request_path, 0);
    if (file_info.result == STATIC_FILE_RESULT_NoXDGMimeAvailable) {
      file_info = c_simple_http_get_file(args->static_dir, request_path, 1);
    }

    if (file_info.result != STATIC_FILE_RESULT_OK
        || !file_info.buf
        || file_info.buf_size == 0
        || !file_info.mime_type) {
      if (file_info.result == STATIC_FILE_RESULT_FileError
          || file_info.result == STATIC_FILE_RESULT_InternalError) {
        response_code = C_SIMPLE_HTTP_Response_500_Internal_Server_Error;
      } else if (file_info.result == STATIC_FILE_RESULT_InvalidParameter) {
        response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
      } else if (file_info.result == STATIC_FILE_RESULT_404NotFound) {
        response_code = C_SIMPLE_HTTP_Response_404_Not_Found;
      } else if (file_info.result == STATIC_FILE_RESULT_InvalidPath) {
        response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
      } else {
        response_code = C_SIMPLE_HTTP_Response_500_Internal_Server_Error;
      }

      c_simple_http_on_error(response_code, citem->fd);
      return 1;
    } else {
      CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "HTTP/1.1 200 OK\n", 16));
      CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "Allow: GET\n", 11));
      CHECK_ERROR_WRITE_NO_FD(write( citem->fd, "Connection: close\n", 18));
      uint64_t mime_length = strlen(file_info.mime_type);
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *mime_type_buf = malloc(mime_length + 1 + 14 + 1);
      snprintf(
        mime_type_buf,
        mime_length + 1 + 14 + 1,
        "Content-Type: %s\n",
        file_info.mime_type);
      CHECK_ERROR_WRITE_NO_FD(write(
        citem->fd, mime_type_buf, mime_length + 1 + 14));
      uint64_t content_str_len = 0;
      for(uint64_t buf_size_temp = file_info.buf_size;
          buf_size_temp > 0;
          buf_size_temp /= 10) {
        ++content_str_len;
      }
      if (content_str_len == 0) {
        content_str_len = 1;
      }
      __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
      char *content_length_buf = malloc(content_str_len + 1 + 16 + 1);
      snprintf(
        content_length_buf,
        content_str_len + 1 + 16 + 1,
        "Content-Length: %lu\n",
        file_info.buf_size);
      CHECK_ERROR_WRITE_NO_FD(write(
        citem->fd, content_length_buf, content_str_len + 1 + 16));
      CHECK_ERROR_WRITE_NO_FD(write(citem->fd, "\n", 1));
      CHECK_ERROR_WRITE_NO_FD(write(
        citem->fd, file_info.buf, file_info.buf_size));
      fprintf(stderr,
              "NOTICE Found static file for path \"%s\"\n",
              request_path);
    }
  } else {
    c_simple_http_on_error(response_code, citem->fd);
    return 1;
  }

  return 1;
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

  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *connections = simple_archiver_list_init();

  char recv_buf[C_SIMPLE_HTTP_RECV_BUF_SIZE];

  ConnectionContext connection_context;
  connection_context.buf = recv_buf;
  connection_context.args = &args;
  connection_context.parsed = &parsed_config;

  signal(SIGINT, C_SIMPLE_HTTP_handle_sigint);
  signal(SIGUSR1, C_SIMPLE_HTTP_handle_sigusr1);
  signal(SIGPIPE, C_SIMPLE_HTTP_handle_sigpipe);

  int ret;
  ssize_t read_ret;
  socklen_t socket_len;

  // xxxx xxx1 - config needs to be reloaded.
  uint32_t flags = 0;
  size_t config_try_reload_ticks_count = 0;
  uint32_t config_try_reload_attempts = 0;

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
    while (1) {
      ret = accept(tcp_socket, (struct sockaddr *)&peer_info, &socket_len);
      if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // No connecting peers, do nothing.
        break;
      } else if (ret == -1) {
        printf("WARNING: accept: errno %d\n", errno);
        break;
      } else if (ret >= 0) {
        // Received connection, handle it.
        if ((args.flags & 1) == 0) {
          printf("Peer connected: addr is ");
          c_simple_http_print_ipv6_addr(stdout, &peer_info.sin6_addr);
          printf(", fd is %d\n", ret);
        } else {
          printf("Peer connected.\n");
        }
        int connection_fd = ret;

        // Set non-blocking.
        ret = fcntl(connection_fd, F_SETFL, O_NONBLOCK);
        if (ret < 0) {
          fprintf(
            stderr, "ERROR Failed to set non-blocking on connection fd!\n");
          close(connection_fd);
          continue;
        }

        ConnectionItem *citem = malloc(sizeof(ConnectionItem));
        citem->fd = connection_fd;
        ret = clock_gettime(CLOCK_MONOTONIC, &citem->time_point);
        citem->peer_addr = peer_info.sin6_addr;
        simple_archiver_list_add(connections,
                                 citem,
                                 c_simple_http_cleanup_connection_item);
      } else {
        printf("WARNING: accept: Unknown invalid state!\n");
        break;
      }
    }

    clock_gettime(CLOCK_MONOTONIC, &connection_context.current_time);
    simple_archiver_list_remove(connections,
                                c_simple_http_manage_connections,
                                &connection_context);
#ifndef NDEBUG
    //printf(".");
    //fflush(stdout);
#endif
  }

  printf("End of program.\n");
  return 0;
}

// vim: et ts=2 sts=2 sw=2
