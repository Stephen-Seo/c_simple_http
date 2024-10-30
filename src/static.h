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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_STATIC_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_STATIC_H_

// Standard library includes.
#include <stdint.h>

typedef enum C_SIMPLE_HTTP_StaticFileResult {
  STATIC_FILE_RESULT_OK,
  STATIC_FILE_RESULT_FileError,
  STATIC_FILE_RESULT_InvalidParameter,
  STATIC_FILE_RESULT_NoXDGMimeAvailable,
  STATIC_FILE_RESULT_InternalError,
  STATIC_FILE_RESULT_404NotFound
} C_SIMPLE_HTTP_StaticFileResult;

typedef struct C_SIMPLE_HTTP_StaticFileInfo {
  char *buf;
  uint64_t buf_size;
  char *mime_type;
  C_SIMPLE_HTTP_StaticFileResult result;
} C_SIMPLE_HTTP_StaticFileInfo;

/// Returns non-zero if "xdg_mime" is available.
int_fast8_t c_simple_http_is_xdg_mime_available(void);

void c_simple_http_cleanup_static_file_info(
  C_SIMPLE_HTTP_StaticFileInfo *file_info);

/// If ignore_mime_type is non-zero, then mime information will not be fetched.
/// The mime_type string will therefore default to "application/octet-stream".
C_SIMPLE_HTTP_StaticFileInfo c_simple_http_get_file(
  const char *static_dir, const char *path, int_fast8_t ignore_mime_type);

#endif

// vim: et ts=2 sts=2 sw=2
