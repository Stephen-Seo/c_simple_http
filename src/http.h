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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_HTTP_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_HTTP_H_

// Third party includes.
#include <SimpleArchiver/src/data_structures/hash_map.h>

// Local includes.
#include "config.h"

typedef C_SIMPLE_HTTP_ParsedConfig C_SIMPLE_HTTP_HTTPTemplates;

/// Returned buffer must be "free"d after use.
/// If the request is not valid, or 404, then the buffer will be NULL.
char *c_simple_http_request_response(
  const char *request,
  unsigned int size,
  const C_SIMPLE_HTTP_HTTPTemplates *templates
);

#endif

// vim: ts=2 sts=2 sw=2
