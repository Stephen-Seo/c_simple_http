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

#include "helpers.h"

// Standard library includes.
#include <stdlib.h>

void c_simple_http_helper_to_lowercase_in_place(char *buf, size_t size) {
  for (size_t idx = 0; idx < size; ++idx) {
    if (buf[idx] >= 'A' && buf[idx] <= 'Z') {
      buf[idx] += 32;
    }
  }
}

char *c_simple_http_helper_to_lowercase(const char *buf, size_t size) {
  char *ret_buf = malloc(size + 1);
  for (size_t idx = 0; idx < size; ++idx) {
    if (buf[idx] >= 'A' && buf[idx] <= 'Z') {
      ret_buf[idx] = buf[idx] + 32;
    } else {
      ret_buf[idx] = buf[idx];
    }
  }

  ret_buf[size] = 0;
  return ret_buf;
}

// vim: et ts=2 sts=2 sw=2
