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

#include <hash_map.h>

typedef struct HTTPTemplates {
  /// Each entry in this data structure is a hash map where its value for the
  /// key "PATH" is the path it represents. The "key" value should match the
  /// mentioned value for "PATH".
  SDArchiverHashMap *paths;
} HTTPTemplates;

/// Each line in the config should be a key-value pair separated by an equals
/// sign. All whitespace is ignored unless if the value is quoted. If there
/// exists a line with the key "PATH", then the value must be a path like
/// "/cache" and all the following key-value pairs are associated with that PATH
/// until the next "PATH" key or EOF. Each "PATH" "block" should have a "HTML"
/// key-value pair where the value is a HTML template. Inside such HTML
/// templates should be strings like "{{{{example_key}}}}" which will be
/// replaced by the string value of the key name deliminated by the four curly
/// braces. "HTML_FILE" specifies a filename to read instead of using a literal
/// string in the config file. It will store the contents of the specified file
/// with the "HTML" key internally.
HTTPTemplates c_simple_http_set_up_templates(const char *config_filename);

void c_simple_http_clean_up_templates(HTTPTemplates *templates);

/// Returned buffer must be "free"d after use.
/// If the request is not valid, or 404, then the buffer will be NULL.
char *c_simple_http_request_response(const char *request, unsigned int size,
                                     const HTTPTemplates *templates);

#endif

// vim: ts=2 sts=2 sw=2
