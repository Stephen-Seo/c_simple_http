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

#include "static.h"

// Standard library includes.
#include <stdio.h>
#include <stdlib.h>

// Standard C library includes.
#include <spawn.h>

// Posix includes.
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>

// Third party includes.
#include "SimpleArchiver/src/helpers.h"

char **environ;

void internal_fd_cleanup_helper(int *fd) {
  if (fd && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

void internal_cleanup_file_actions(posix_spawn_file_actions_t **actions) {
  if (actions && *actions) {
    posix_spawn_file_actions_destroy(*actions);
    free(*actions);
    *actions = NULL;
  }
}

void internal_cleanup_prev_cwd(char **path) {
  if (path && *path) {
    int ret = chdir(*path);
    if (ret != 0) {
      fprintf(stderr, "WARNING: chdir back to cwd failed! (errno %d)\n", errno);
    }
    free(*path);
    *path = NULL;
  }
}

int_fast8_t c_simple_http_is_xdg_mime_available(void) {
  __attribute__((cleanup(internal_fd_cleanup_helper)))
  int dev_null_fd = open("/dev/null", O_WRONLY);

  __attribute__((cleanup(internal_cleanup_file_actions)))
  posix_spawn_file_actions_t *actions =
    malloc(sizeof(posix_spawn_file_actions_t));
  int ret = posix_spawn_file_actions_init(actions);
  if (ret != 0) {
    free(actions);
    actions = NULL;
    return 0;
  }

  posix_spawn_file_actions_adddup2(actions, dev_null_fd, STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(actions, dev_null_fd, STDERR_FILENO);

  pid_t pid;
  ret = posix_spawnp(&pid,
                     "xdg-mime",
                     actions,
                     NULL,
                     (char *const[]){"xdg-mime", "--help", NULL},
                     environ);
  if (ret != 0) {
    return 0;
  }

  waitpid(pid, &ret, 0);

  return (ret == 0 ? 1 : 0);
}

void c_simple_http_cleanup_static_file_info(
    C_SIMPLE_HTTP_StaticFileInfo *file_info) {
  if (file_info->buf) {
    free(file_info->buf);
    file_info->buf = NULL;
  }
  file_info->buf_size = 0;
  if (file_info->mime_type) {
    free(file_info->mime_type);
    file_info->mime_type = NULL;
  }
}

C_SIMPLE_HTTP_StaticFileInfo c_simple_http_get_file(const char *static_dir,
                                                    const char *path) {
  C_SIMPLE_HTTP_StaticFileInfo file_info;
  memset(&file_info, 0, sizeof(C_SIMPLE_HTTP_StaticFileInfo));

  if (!static_dir || !path || !c_simple_http_is_xdg_mime_available()) {
    return file_info;
  }

  uint64_t buf_size = 128;
  char *buf = malloc(buf_size);
  char *ptr;
  while (1) {
    ptr = getcwd(buf, buf_size);
    if (ptr == NULL) {
      if (errno == ERANGE) {
        buf_size *= 2;
        buf = realloc(buf, buf_size);
        if (buf == NULL) {
          return file_info;
        }
      } else {
        free(buf);
        return file_info;
      }
    } else {
      break;
    }
  }

  __attribute__((cleanup(internal_cleanup_prev_cwd)))
  char *prev_cwd = buf;

  int ret = chdir(static_dir);
  if (ret != 0) {
    fprintf(stderr,
            "ERROR: Failed to chdir into \"%s\"! (errno %d)\n",
            static_dir,
            errno);
    return file_info;
  }

  __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
  FILE *fd = NULL;
  uint64_t idx = 0;
  if (path[0] == '/') {
    for(; path[idx] != 0; ++idx) {
      if (path[idx] != '/') {
        break;
      }
    }
    if (path[idx] == 0) {
      fprintf(stderr, "ERROR: Received invalid path \"%s\"!\n", path);
      return file_info;
    }
  }
  fd = fopen(path + idx, "rb");

  if (fd == NULL) {
    fprintf(stderr, "ERROR: Failed to open path \"%s\"!\n", path + idx);
    return file_info;
  }

  fseek(fd, 0, SEEK_END);
  long long_ret = ftell(fd);
  if (long_ret < 0) {
    fprintf(stderr, "ERROR: Failed to seek in path fd \"%s\"!\n", path);
    return file_info;
  }
  fseek(fd, 0, SEEK_SET);
  file_info.buf_size = (uint64_t)long_ret;
  file_info.buf = malloc(file_info.buf_size);
  size_t size_t_ret = fread(file_info.buf, 1, file_info.buf_size, fd);
  if (size_t_ret != file_info.buf_size) {
    fprintf(stderr, "ERROR: Failed to read path fd \"%s\"!\n", path);
    free(file_info.buf);
    file_info.buf_size = 0;
    return file_info;
  }

  simple_archiver_helper_cleanup_FILE(&fd);

  int from_xdg_mime_pipe[2];
  ret = pipe(from_xdg_mime_pipe);

  __attribute__((cleanup(internal_cleanup_file_actions)))
  posix_spawn_file_actions_t *actions =
    malloc(sizeof(posix_spawn_file_actions_t));
  ret = posix_spawn_file_actions_init(actions);
  if (ret != 0) {
    free(actions);
    actions = NULL;
    c_simple_http_cleanup_static_file_info(&file_info);
    close(from_xdg_mime_pipe[1]);
    close(from_xdg_mime_pipe[0]);
    return file_info;
  }

  posix_spawn_file_actions_adddup2(actions,
                                   from_xdg_mime_pipe[1],
                                   STDOUT_FILENO);

  // Close "read" side of pipe on "xdg-mime"'s side.
  posix_spawn_file_actions_addclose(actions, from_xdg_mime_pipe[0]);

  buf_size = 256;
  buf = malloc(buf_size);
  uint64_t buf_idx = 0;

  char *path_plus_idx = (char*)path + idx;
  pid_t pid;
  ret = posix_spawnp(&pid,
                     "xdg-mime",
                     actions,
                     NULL,
                     (char *const[]){"xdg-mime",
                                     "query",
                                     "filetype",
                                     path_plus_idx,
                                     NULL},
                     environ);
  if (ret != 0) {
    c_simple_http_cleanup_static_file_info(&file_info);
    close(from_xdg_mime_pipe[1]);
    close(from_xdg_mime_pipe[0]);
    return file_info;
  }

  close(from_xdg_mime_pipe[1]);

  ssize_t ssize_t_ret;
  while (1) {
    ssize_t_ret =
      read(from_xdg_mime_pipe[0], buf + buf_idx, buf_size - buf_idx);
    if (ssize_t_ret <= 0) {
      break;
    } else {
      buf_idx += (uint64_t)ssize_t_ret;
      if (buf_idx >= buf_size) {
        buf_size *= 2;
        buf = realloc(buf, buf_size);
        if (buf == NULL) {
          c_simple_http_cleanup_static_file_info(&file_info);
          close(from_xdg_mime_pipe[0]);
          return file_info;
        }
      }
    }
  }

  close(from_xdg_mime_pipe[0]);
  waitpid(pid, &ret, 0);

  if (ret != 0) {
    c_simple_http_cleanup_static_file_info(&file_info);
    return file_info;
  }

  buf[buf_idx] = 0;
  if (buf[buf_idx-1] == '\n') {
    buf[buf_idx-1] = 0;
  }

  file_info.mime_type = buf;

  return file_info;
}

// vim: et ts=2 sts=2 sw=2
