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

#ifndef SEODISPARATE_COM_C_SIMPLE_HTTP_CONFIG_H_
#define SEODISPARATE_COM_C_SIMPLE_HTTP_CONFIG_H_

// Third party includes.
#include <SimpleArchiver/src/data_structures/linked_list.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>

typedef struct C_SIMPLE_HTTP_ParsedConfig {
  /// Each entry in this data structure is a hash map where its value for the
  /// key "PATH" is the path it represents. The "key" value should match the
  /// mentioned value for "PATH".
  union {
    SDArchiverHashMap *paths;
    SDArchiverHashMap *hash_map;
  };
} C_SIMPLE_HTTP_ParsedConfig;

typedef C_SIMPLE_HTTP_ParsedConfig C_SIMPLE_HTTP_HashMapWrapper;

/// Each line in the config should be a key-value pair separated by an equals
/// sign "=". All whitespace is ignored unless if the value is "quoted". A part
/// of a string can be "quoted" if it is surrounded by three single-quotes or
/// three double-quotes. The "separating_key" separates lines of config into
/// sections such that each key with the "separating_key" is the start of a new
/// section. All key-value pairs after a "separating_key" belongs to that
/// section. "required_names" is an optional list of c-strings that defines
/// required keys for each section. If it is not desired to use
/// "required_names", then pass NULL instead of a pointer.
C_SIMPLE_HTTP_ParsedConfig c_simple_http_parse_config(
  const char *config_filename,
  const char *separating_key,
  SDArchiverLinkedList *required_names
);

void c_simple_http_clean_up_parsed_config(C_SIMPLE_HTTP_ParsedConfig *config);

#endif

// vim: ts=2 sts=2 sw=2
