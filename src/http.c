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

const char *c_simple_http_response_code_error_to_response(
    enum C_SIMPLE_HTTP_ResponseCode response_code) {
  switch (response_code) {
    case C_SIMPLE_HTTP_Response_400_Bad_Request:
      return "HTTP/1.1 400 Bad Request\nAllow: GET\nContent-Type: text/html\n"
             "Content-Length: 25\n\n"
             "<h1>400 Bad Request</h1>\n";
    case C_SIMPLE_HTTP_Response_404_Not_Found:
      return "HTTP/1.1 404 Not Found\nAllow: GET\nContent-Type: text/html\n"
             "Content-Length: 23\n\n"
             "<h1>404 Not Found</h1>\n";
    case C_SIMPLE_HTTP_Response_500_Internal_Server_Error:
    default:
      return "HTTP/1.1 500 Internal Server Error\nAllow: GET\n"
             "Content-Type: text/html\n"
             "Content-Length: 35\n\n"
             "<h1>500 Internal Server Error</h1>\n";
  }
}

char *c_simple_http_request_response(
    const char *request,
    unsigned int size,
    const C_SIMPLE_HTTP_HTTPTemplates *templates,
    size_t *out_size,
    enum C_SIMPLE_HTTP_ResponseCode *out_response_code) {
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
    if (out_response_code) {
      *out_response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
    }
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
    if (out_response_code) {
      *out_response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
    }
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
    if (out_response_code) {
      *out_response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
    }
    return NULL;
  }
#ifndef NDEBUG
  fprintf(stderr, "Parsing request: got http protocol \"%s\"\n", request_proto);
#endif

  if (strcmp(request_type, "GET") != 0) {
    fprintf(stderr, "ERROR Only GET requests are allowed!\n");
    if (out_response_code) {
      *out_response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
    }
    return NULL;
  } else if (strcmp(request_proto, "HTTP/1.1") != 0) {
    fprintf(stderr, "ERROR Only HTTP/1.1 protocol requests are allowed!\n");
    if (out_response_code) {
      *out_response_code = C_SIMPLE_HTTP_Response_400_Bad_Request;
    }
    return NULL;
  }

  size_t generated_size = 0;
  __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
  char *stripped_path = c_simple_http_strip_path(
    request_path, request_path_idx);
  char *generated_buf = c_simple_http_path_to_generated(
    stripped_path ? stripped_path : request_path,
    templates,
    &generated_size);

  if (!generated_buf || generated_size == 0) {
    fprintf(stderr, "ERROR Unable to generate response html for path \"%s\"!\n",
      request_path);
    if (out_response_code) {
      if (
          simple_archiver_hash_map_get(
            templates->hash_map,
            stripped_path ? stripped_path : request_path,
            stripped_path ? strlen(stripped_path) + 1 : request_path_idx + 1)
          == NULL) {
        *out_response_code = C_SIMPLE_HTTP_Response_404_Not_Found;
      } else {
        *out_response_code = C_SIMPLE_HTTP_Response_500_Internal_Server_Error;
      }
    }
    return NULL;
  }

  if (out_size) {
    *out_size = generated_size;
  }
  if (out_response_code) {
    *out_response_code = C_SIMPLE_HTTP_Response_200_OK;
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

char *c_simple_http_filter_request_header(
    const char *request, size_t request_size, const char *header) {
  if (!request) {
    fprintf(stderr, "ERROR filter_request_header: request is NULL!\n");
    return NULL;
  } else if (request_size == 0) {
    fprintf(stderr, "ERROR filter_request_header: request_size is zero!\n");
    return NULL;
  } else if (!header) {
    fprintf(stderr, "ERROR filter_request_header: header is NULL!\n");
    return NULL;
  }

  // xxxx xxx0 - Start of line.
  // xxxx xxx1 - In middle of line.
  // xxxx xx0x - Header NOT found.
  // xxxx xx1x - Header was found.
  unsigned int flags = 0;
  size_t idx = 0;
  size_t line_start_idx = 0;
  size_t header_idx = 0;
  for(; idx < request_size && request[idx] != 0; ++idx) {
    if ((flags & 1) == 0) {
      if (request[idx] == '\n') {
        continue;
      }
      line_start_idx = idx;
      flags |= 1;
      if (request[idx] == header[header_idx]) {
        ++header_idx;
        if (header[header_idx] == 0) {
          flags |= 2;
          break;
        }
      } else {
        header_idx = 0;
      }
    } else {
      if (header_idx != 0) {
        if (request[idx] == header[header_idx]) {
          ++header_idx;
          if (header[header_idx] == 0) {
            flags |= 2;
            break;
          }
        } else {
          header_idx = 0;
        }
      }

      if (request[idx] == '\n') {
        flags &= 0xFFFFFFFE;
      }
    }
  }

  if ((flags & 2) != 0) {
    // Get line end starting from line_start_idx.
    for (
      idx = line_start_idx;
      idx < request_size && request[idx] != 0 && request[idx] != '\n';
      ++idx);
    char *line_buf = malloc(idx - line_start_idx + 1);
    memcpy(line_buf, request + line_start_idx, idx - line_start_idx);
    line_buf[idx - line_start_idx] = 0;
    return line_buf;
  }

  return NULL;
}

// vim: ts=2 sts=2 sw=2
