#include "big_endian.h"

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

// vim: ts=2 sts=2 sw=2
