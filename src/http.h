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

// Standard library includes.
#include <stddef.h>

// Third party includes.
#include <SimpleArchiver/src/data_structures/hash_map.h>

// Local includes.
#include "config.h"

typedef C_SIMPLE_HTTP_ParsedConfig C_SIMPLE_HTTP_HTTPTemplates;

enum C_SIMPLE_HTTP_ResponseCode {
  C_SIMPLE_HTTP_Response_200_OK,
  C_SIMPLE_HTTP_Response_400_Bad_Request,
  C_SIMPLE_HTTP_Response_404_Not_Found,
  C_SIMPLE_HTTP_Response_500_Internal_Server_Error,
};

// If the response code is an error, returns a full response string that doesn't
// need to be free'd. Otherwise, returns as if error 500.
const char *c_simple_http_response_code_error_to_response(
  enum C_SIMPLE_HTTP_ResponseCode response_code);

/// Returned buffer must be "free"d after use.
/// If the request is not valid, or 404, then the buffer will be NULL.
char *c_simple_http_request_response(
  const char *request,
  unsigned int size,
  const C_SIMPLE_HTTP_HTTPTemplates *templates,
  size_t *out_size,
  enum C_SIMPLE_HTTP_ResponseCode *out_response_code
);

/// Takes a PATH string and returns a "bare" path.
/// This will simply omit the first instance of "?" or "#" and the rest of the
/// string. This will also remove trailing "/" characters.
/// Must be free'd if returns non-NULL.
char *c_simple_http_strip_path(const char *path, size_t path_size);

/// Returns non-NULL if successful. Must be freed with
/// simple_archiver_hash_map_free if non-NULL.
/// The map is a mapping of lowercase header names to header lines.
/// E.g. "user-agent" -> "User-Agent: curl".
SDArchiverHashMap *c_simple_http_request_to_headers_map(
  const char *request, size_t request_size);

#endif

// vim: et ts=2 sts=2 sw=2
