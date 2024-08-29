#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_ARG_PARSE_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_ARG_PARSE_H_

typedef struct Args {
  unsigned short flags;
  unsigned short port;
} Args;

void print_usage(void);

Args parse_args(int argc, char **argv);

#endif

// vim: ts=2 sts=2 sw=2
