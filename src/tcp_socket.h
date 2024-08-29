#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_TCP_SOCKET_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_TCP_SOCKET_H_

#define C_SIMPLE_HTTP_TCP_SOCKET_BACKLOG 64

int create_tcp_socket(unsigned short port);

void cleanup_tcp_socket(int *tcp_socket);

#endif

// vim: ts=2 sts=2 sw=2
