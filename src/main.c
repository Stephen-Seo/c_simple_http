// Standard library includes.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Unix includes.
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define C_SIMPLE_HTTP_TCP_SOCKET_BACKLOG 64
#define C_SIMPLE_HTTP_SLEEP_NANOS 1000000

static int C_SIMPLE_HTTP_KEEP_RUNNING = 1;

void C_SIMPLE_HTTP_handle_sigint(int signal) {
  if (signal == SIGINT) {
#ifndef NDEBUG
    puts("Handling SIGINT");
#endif
    C_SIMPLE_HTTP_KEEP_RUNNING = 0;
  }
}

typedef struct Args {
  unsigned short flags;
  unsigned short port;
} Args;

int is_big_endian(void) {
  union {
    int i;
    char c[4];
  } bint = {0x01020304};

  return bint.c[0] == 1 ? 1 : 0;
}

unsigned short u16_be_swap(unsigned short value) {
  if (is_big_endian()) {
    return value;
  } else {
    return ((value >> 8) & 0xFF) | ((value << 8) & 0xFF00);
  }
}

int create_tcp_socket(unsigned short port) {
  struct sockaddr_in6 ipv6_addr;
  memset(&ipv6_addr, 0, sizeof(struct sockaddr_in6));
  ipv6_addr.sin6_family = AF_INET6;
  ipv6_addr.sin6_port = u16_be_swap(port);
  ipv6_addr.sin6_addr = in6addr_any;

  int tcp_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 6);

  if (tcp_socket == -1) {
    switch (errno) {
      case EACCES:
        puts("ERROR: Socket creation: EACCES");
        break;
      case EAFNOSUPPORT:
        puts("ERROR: Socket creation: EAFNOSUPPORT");
        break;
      case EINVAL:
        puts("ERROR: Socket creation: EINVAL");
        break;
      case EMFILE:
        puts("ERROR: Socket creation: EMFILE");
        break;
      case ENOBUFS:
        puts("ERROR: Socket creation: ENOBUFS");
        break;
      case ENOMEM:
        puts("ERROR: Socket creation: ENOMEM");
        break;
      case EPROTONOSUPPORT:
        puts("ERROR: Socket creation: EPROTONOSUPPORT");
        break;
      default:
        puts("ERROR: Socket creation: Unknown Error");
        break;
    }
    return -1;
  }

  int ret = bind(tcp_socket, (const struct sockaddr *)&ipv6_addr, sizeof(struct sockaddr_in6));
  if (ret != 0) {
    close(tcp_socket);
    puts("ERROR: Failed to bind socket!");
    return -1;
  }

  ret = listen(tcp_socket, C_SIMPLE_HTTP_TCP_SOCKET_BACKLOG);

  if (ret == 0) {
    return tcp_socket;
  } else {
    switch (errno) {
      case EADDRINUSE:
        puts("ERROR: Socket listen: EADDRINUSE");
        break;
      case EBADF:
        puts("ERROR: Socket listen: EBADF");
        break;
      case ENOTSOCK:
        puts("ERROR: Socket listen: ENOTSOCK");
        break;
      default:
        puts("ERROR: Socket listen: Unknown Error");
        break;
    }

    return -1;
  }
}

void cleanup_tcp_socket(int *tcp_socket) {
  if (tcp_socket && *tcp_socket != -1) {
    close(*tcp_socket);
    *tcp_socket = -1;
  }
}

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
