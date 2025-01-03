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
  ///
  /// An example mapping for this structure (based on current example config):
  /// KEY: "/", VALUE: HashMapWrapper struct
  /// KEY: "/inner", VALUE: HashMapWrapper struct
  /// KEY: "/inner/further", VALUE: HashMapWrapper struct
  ///
  /// Each HashMapWrapper struct's hash-map has the following:
  /// KEY: VAR_NAME, VALUE: ConfigValue struct
  union {
    SDArchiverHashMap *paths;
    SDArchiverHashMap *hash_map;
  };
} C_SIMPLE_HTTP_ParsedConfig;

typedef C_SIMPLE_HTTP_ParsedConfig C_SIMPLE_HTTP_HashMapWrapper;

typedef struct C_SIMPLE_HTTP_ConfigValue {
  char *value;
  struct C_SIMPLE_HTTP_ConfigValue *next;
} C_SIMPLE_HTTP_ConfigValue;

void c_simple_http_cleanup_config_value(
  C_SIMPLE_HTTP_ConfigValue *config_value);
void c_simple_http_cleanup_config_value_void_ptr(void *config_value);

/// Each line in the config should be a key-value pair separated by an equals
/// sign "=". All whitespace is ignored unless if the value is "quoted". A part
/// of a string can be "quoted" if it is surrounded by three single-quotes or
/// three double-quotes. The "separating_key" separates lines of config into
/// sections such that each key with the "separating_key" is the start of a new
/// section. All key-value pairs after a "separating_key" belongs to that
/// section. "required_names" is an optional list of c-strings that defines
/// required keys for each section. If it is not desired to use
/// "required_names", then pass NULL instead of a pointer.
///
/// The returned wrapped hash-map is structured as follows: Each key is a
/// c-string which corresponds to an existing value for a separating_key, and
/// each value is a C_SIMPLE_HTTP_ParsedConfig pointer containing the key-value
/// pairs from the config. For example, if the config is as follows:
///
/// PATH=/
/// SOME_KEY=SOME_VALUE
/// PATH=/inner
/// SOME_OTHER_KEY=SOME_OTHER_VALUE
///
/// then the wrapped hash-map has "/" and "/inner" as keys (length including the
/// zero-terminator of the c-string). The wrapped hash-map pointer in the
/// C_SIMPLE_HTTP_ParsedConfig pointer corresponding to key "/" has key "PATH"
/// to value "/" and has key "SOME_KEY" to value "SOME_VALUE". The wrapped
/// hash-map pointer corresponding to key "/inner" has key "PATH" to value
/// "/inner" and has key "SOME_OTHER_KEY" to value "SOME_OTHER_VALUE".
C_SIMPLE_HTTP_ParsedConfig c_simple_http_parse_config(
  const char *config_filename,
  const char *separating_key,
  const SDArchiverLinkedList *required_names
);

void c_simple_http_clean_up_parsed_config(C_SIMPLE_HTTP_ParsedConfig *config);

#endif

// vim: et ts=2 sts=2 sw=2
