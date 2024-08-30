// Standard library includes.
#include <stdio.h>
#include <string.h>

// Local includes.
#include "http.h"

// Third party includes.
#include <helpers.h>
#include <hash_map.h>

static int checks_checked = 0;
static int checks_passed = 0;

#define RETURN() \
  do { \
    fprintf(stderr, "checked %d\npassed  %d\n", checks_checked, checks_passed);\
    return checks_checked == checks_passed ? 0 : 1; \
  } while (0);

#define CHECK_TRUE(x)                                             \
  do {                                                            \
    ++checks_checked;                                             \
    if (!(x)) {                                                   \
      printf("CHECK_TRUE at line %u failed: %s\n", __LINE__, #x); \
    } else {                                                      \
      ++checks_passed;                                            \
    }                                                             \
  } while (0);
#define CHECK_FALSE(x)                                             \
  do {                                                             \
    ++checks_checked;                                              \
    if (x) {                                                       \
      printf("CHECK_FALSE at line %u failed: %s\n", __LINE__, #x); \
    } else {                                                       \
      ++checks_passed;                                             \
    }                                                              \
  } while (0);
#define CHECK_STREQ(a, b)                                                    \
  do {                                                                       \
    ++checks_checked;                                                        \
    if (strcmp((a), (b)) == 0) {                                             \
      ++checks_passed;                                                       \
    } else {                                                                 \
      printf("CHECK_STREQ at line %u failed: %s != %s\n", __LINE__, #a, #b); \
    }                                                                        \
  } while (0);

#define ASSERT_TRUE(x)                                             \
  do {                                                            \
    ++checks_checked;                                             \
    if (!(x)) {                                                   \
      printf("ASSERT_TRUE at line %u failed: %s\n", __LINE__, #x); \
      RETURN() \
    } else {                                                      \
      ++checks_passed;                                            \
    }                                                             \
  } while (0);
#define ASSERT_FALSE(x)                                             \
  do {                                                             \
    ++checks_checked;                                              \
    if (x) {                                                       \
      printf("ASSERT_FALSE at line %u failed: %s\n", __LINE__, #x); \
      RETURN() \
    } else {                                                       \
      ++checks_passed;                                             \
    }                                                              \
  } while (0);
#define ASSERT_STREQ(a, b)                                                    \
  do {                                                                       \
    ++checks_checked;                                                        \
    if (strcmp((a), (b)) == 0) {                                             \
      ++checks_passed;                                                       \
    } else {                                                                 \
      printf("ASSERT_STREQ at line %u failed: %s != %s\n", __LINE__, #a, #b); \
      RETURN() \
    }                                                                        \
  } while (0);

int main(void) {
  // Test set up templates.
  {
    const char *test_config_filename = "/tmp/c_simple_http_test.config";
    __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
    FILE *test_file = fopen(test_config_filename, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite("PATH=/\nHTML=''' one two three '''\n", 1, 34, test_file)
        == 34);
    ASSERT_TRUE(
        fwrite("TEST=''' \"one two \"three '''\n", 1, 29, test_file)
        == 29);
    ASSERT_TRUE(
        fwrite("TEST2=' \"one two \"three ''\n", 1, 27, test_file)
        == 27);
    ASSERT_TRUE(
        fwrite("TEST3=\"\"\" \"one two \"three ''\"\"\"\n", 1, 32, test_file)
        == 32);
    ASSERT_TRUE(
        fwrite("TEST4=''' \"\"\"one two \"\"\"three '''\n", 1, 34, test_file)
        == 34);
    ASSERT_TRUE(
        fwrite("TEST5=\"\"\" '''one two '''three \"\"\"\n", 1, 34, test_file)
        == 34);
    simple_archiver_helper_cleanup_FILE(&test_file);

    __attribute__((cleanup(c_simple_http_clean_up_templates)))
    HTTPTemplates templates =
      c_simple_http_set_up_templates(test_config_filename);
    ASSERT_TRUE(templates.paths);

    HashMapWrapper *first_path_map_wrapper =
      simple_archiver_hash_map_get(templates.paths, "/", 2);
    ASSERT_TRUE(first_path_map_wrapper);

    const char *value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "PATH", 5);
    ASSERT_TRUE(value);
    ASSERT_STREQ(value, "/");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "HTML", 5);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, " one two three ");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "TEST", 5);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, " \"one two \"three ");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "TEST2", 6);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, "'\"onetwo\"three''");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "TEST3", 6);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, " \"one two \"three ''");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "TEST4", 6);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, " \"\"\"one two \"\"\"three ");

    value =
      simple_archiver_hash_map_get(first_path_map_wrapper->paths, "TEST5", 6);
    ASSERT_TRUE(value);
    // printf("%s\n", value);
    ASSERT_STREQ(value, " '''one two '''three ");
  }

  RETURN()
}

// vim: ts=2 sts=2 sw=2
