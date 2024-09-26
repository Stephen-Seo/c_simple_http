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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_HTML_CACHE_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_HTML_CACHE_H_

// Local includes.
#include "http.h"

/// Must be free'd if non-NULL.
char *c_simple_http_path_to_cache_filename(const char *path);

/// Must be free'd if non-NULL.
char *c_simple_http_cache_filename_to_path(const char *cache_filename);

/// Given a "path", returns positive-non-zero if the cache is invalidated.
/// "config_filename" is required to check its timestamp. "cache_dir" is
/// required to actually get the cache file to check against. "buf_out" will be
/// populated if non-NULL, and will either be fetched from the cache or from the
/// config (using http_template). Note that "buf_out" will point to a c-string.
/// Returns a negative value on error.
int c_simple_http_cache_path(
  const char *path,
  const char *config_filename,
  const char *cache_dir,
  C_SIMPLE_HTTP_HTTPTemplates *templates,
  size_t cache_entry_lifespan,
  char **buf_out);

#endif

// vim: et ts=2 sts=2 sw=2
