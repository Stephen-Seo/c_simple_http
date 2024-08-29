#include "tcp_socket.h"

// Standard library includes.
#include <stdio.h>
#include <string.h>

// Unix includes.
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <unistd.h>

// Local includes.
#include "big_endian.h"

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

  int ret = bind(tcp_socket,
                 (const struct sockaddr *)&ipv6_addr,
                 sizeof(struct sockaddr_in6));
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

// vim: ts=2 sts=2 sw=2
