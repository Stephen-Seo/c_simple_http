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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_HTTP_TEMPLATE_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_HTTP_TEMPLATE_H_


// Standard library includes.
#include <stddef.h>

// Third-party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>

// Local includes.
#include "http.h"

// Returns non-NULL on success, which must be free'd after use. Takes a path
// string and templates and returns the generated HTML. If "output_buf_size" is
// non-NULL, it will be set to the size of the returned buffer. If
// "files_list_out" is non-NULL, then the pointer will be set to a newly
// allocated linked-list containing filenames used in generating the HTML. This
// newly allocated linked-list must be freed after use.
char *c_simple_http_path_to_generated(
  const char *path,
  const C_SIMPLE_HTTP_HTTPTemplates *templates,
  size_t *output_buf_size,
  SDArchiverHashMap **files_set_out);

#endif

// vim: et ts=2 sts=2 sw=2
