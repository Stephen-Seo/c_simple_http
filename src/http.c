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

#include "http.h"

// Standard library includes.
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Third party includes.
#include <SimpleArchiver/src/helpers.h>

// Local includes
#include "constants.h"
#include "http_template.h"

#define REQUEST_TYPE_BUFFER_SIZE 16
#define REQUEST_PATH_BUFFER_SIZE 256
#define REQUEST_PROTO_BUFFER_SIZE 16

char *c_simple_http_request_response(
    const char *request,
    unsigned int size,
    const C_SIMPLE_HTTP_HTTPTemplates *templates,
    size_t *out_size) {
  if (out_size) {
    *out_size = 0;
  }
  // parse first line.
  unsigned int idx = 0;
  char request_type[REQUEST_TYPE_BUFFER_SIZE] = {0};
  unsigned int request_type_idx = 0;
  for (; idx < size
      && request[idx] != ' '
      && request[idx] != '\n'
      && request[idx] != '\r'
      && request[idx] != '\t'
      && request_type_idx < REQUEST_TYPE_BUFFER_SIZE - 1;
        ++idx) {
    request_type[request_type_idx++] = request[idx];
  }
  if (request_type_idx == 0) {
    return NULL;
  }
#ifndef NDEBUG
  fprintf(stderr, "Parsing request: got type \"%s\"\n", request_type);
#endif
  // skip whitespace until next part.
  for (; idx < size
      && (request[idx] == ' ' ||
          request[idx] == '\n' ||
          request[idx] == '\r' ||
          request[idx] == '\t');
      ++idx) {}
  // parse request path.
  char request_path[REQUEST_PATH_BUFFER_SIZE] = {0};
  unsigned int request_path_idx = 0;
  for (; idx < size
      && request[idx] != ' '
      && request[idx] != '\n'
      && request[idx] != '\r'
      && request[idx] != '\t'
      && request_path_idx < REQUEST_PATH_BUFFER_SIZE - 1;
        ++idx) {
    request_path[request_path_idx++] = request[idx];
  }
  if (request_path_idx == 0) {
    return NULL;
  }
#ifndef NDEBUG
  fprintf(stderr, "Parsing request: got path \"%s\"\n", request_path);
#endif
  // skip whitespace until next part.
  for (; idx < size
      && (request[idx] == ' ' ||
          request[idx] == '\n' ||
          request[idx] == '\r' ||
          request[idx] == '\t');
      ++idx) {}
  // parse request http protocol.
  char request_proto[REQUEST_PROTO_BUFFER_SIZE] = {0};
  unsigned int request_proto_idx = 0;
  for (; idx < size
      && request[idx] != ' '
      && request[idx] != '\n'
      && request[idx] != '\r'
      && request[idx] != '\t'
      && request_proto_idx < REQUEST_PROTO_BUFFER_SIZE - 1;
        ++idx) {
    request_proto[request_proto_idx++] = request[idx];
  }
  if (request_proto_idx == 0) {
    return NULL;
  }
#ifndef NDEBUG
  fprintf(stderr, "Parsing request: got http protocol \"%s\"\n", request_proto);
#endif

  if (strcmp(request_type, "GET") != 0) {
    fprintf(stderr, "ERROR Only GET requests are allowed!\n");
    return NULL;
  }

  size_t generated_size = 0;
  char *stripped_path = c_simple_http_strip_path(
    request_path, request_path_idx);
  char *generated_buf = c_simple_http_path_to_generated(
    stripped_path ? stripped_path : request_path,
    templates,
    &generated_size);

  if (stripped_path) {
    free(stripped_path);
  }

  if (!generated_buf || generated_size == 0) {
    fprintf(stderr, "ERROR Unable to generate response html for path \"%s\"!\n",
      request_path);
    return NULL;
  }

  if (out_size) {
    *out_size = generated_size;
  }
  return generated_buf;
}

char *c_simple_http_strip_path(const char *path, size_t path_size) {
  size_t idx = 0;
  for (; idx < path_size && path[idx] != 0; ++idx) {
    if (path[idx] == '?' || path[idx] == '#') {
      break;
    }
  }

  if (idx >= path_size || path[idx] == 0) {
    return NULL;
  }

  char *stripped_path = malloc(idx + 1);
  memcpy(stripped_path, path, idx);
  stripped_path[idx] = 0;

  return stripped_path;
}

// vim: ts=2 sts=2 sw=2
