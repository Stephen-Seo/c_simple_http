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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_HELPERS_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_HELPERS_H_

// Standard library includes.
#include <stddef.h>
#include <stdint.h>

// libc includes.
#include <time.h>
#include <dirent.h>

// Local includes.
#include "config.h"
#include "arg_parse.h"

// Third-party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>

typedef struct ConnectionContext {
  char *buf;
  const Args *args;
  C_SIMPLE_HTTP_ParsedConfig *parsed;
  struct timespec current_time;
} ConnectionContext;

typedef struct C_SIMPLE_HTTP_String_Part {
  char *buf;
  size_t size;
  uintptr_t extra;
} C_SIMPLE_HTTP_String_Part;

void c_simple_http_cleanup_attr_string_part(C_SIMPLE_HTTP_String_Part **);

/// Assumes "data" is a C_SIMPLE_HTTP_String_Part, "data" was malloced, and
/// "data->buf" was malloced.
void c_simple_http_cleanup_string_part(void *data);

/// Puts a malloced instance of String_Part into the list.
/// The given c_string will be copied into a newly malloced buffer.
void c_simple_http_add_string_part(
  SDArchiverLinkedList *list, const char *c_string, uintptr_t extra);

/// Puts a malloced instance of String_Part into the list.
/// The given c_string will be copied into a newly malloced buffer.
/// "size" must include NULL if "buffer" is a c_string.
/// If there is no NULL at the end, "size" must be +1 actual size.
void c_simple_http_add_string_part_sized(
  SDArchiverLinkedList *list,
  const char *buffer,
  size_t size,
  uintptr_t extra);

/// Combines all String_Parts in the list and returns it as a single buffer.
char *c_simple_http_combine_string_parts(const SDArchiverLinkedList *list);

/// Modifies "buf" in-place to change all uppercase to lowercase alpha chars.
void c_simple_http_helper_to_lowercase_in_place(char *buf, size_t size);

/// Returns a c-string that should be free'd after use that has converted all
/// uppercase to lowercase alpha chars.
char *c_simple_http_helper_to_lowercase(const char *buf, size_t size);

/// Converts two hexadecimal digits into its corresponding value.
char c_simple_http_helper_hex_to_value(const char upper, const char lower);

/// Unescapes percent-encoded parts in the given uri string. If this returns
/// non-NULL, it must be free'd.
char *c_simple_http_helper_unescape_uri(const char *uri);

/// Returns zero if successful. "dirpath" will point to a directory on success.
/// Returns 1 if the directory already exists.
/// Other return values are errors.
int c_simple_http_helper_mkdir_tree(const char *dirpath);

void c_simple_http_cleanup_DIR(DIR **fd);

/// Must be free'd if non-NULL.
char *c_simple_http_FILE_to_c_str(const char *filename, uint64_t *size_out);

/// Trims by placing NULL bytes in place of whitespace at the end of c_str.
/// Returns number of whitespace trimmed.
size_t c_simple_http_trim_end_whitespace(char *c_str);

#endif

// vim: et ts=2 sts=2 sw=2
