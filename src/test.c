// Standard library includes.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Local includes.
#include "config.h"
#include "http_template.h"

// Third party includes.
#include <SimpleArchiver/src/helpers.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>

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
  // Test config.
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
    ASSERT_TRUE(
        fwrite("PATH=/derp\nHTML=''' one two three '''\n", 1, 38, test_file)
        == 38);
    ASSERT_TRUE(
        fwrite("TEST=''' \"one two \"three '''\n", 1, 29, test_file)
        == 29);
    ASSERT_TRUE(
        fwrite("TEST2=' \"one two \"three ''\n", 1, 27, test_file)
        == 27);
    ASSERT_TRUE(
        fwrite("PATH=/doop\n", 1, 11, test_file)
        == 11);
    ASSERT_TRUE(
        fwrite("TEST3=something\n", 1, 16, test_file)
        == 16);
    // No endline at the end of file to check for case where the config file
    // has no endline.
    ASSERT_TRUE(
        fwrite("TEST2=' \"one two \"three ''", 1, 26, test_file)
        == 26);
    simple_archiver_helper_cleanup_FILE(&test_file);

    __attribute__((cleanup(simple_archiver_list_free)))
    SDArchiverLinkedList *required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "TEST2",
      simple_archiver_helper_datastructure_cleanup_nop);

    __attribute__((cleanup(c_simple_http_clean_up_parsed_config)))
    C_SIMPLE_HTTP_ParsedConfig templates =
      c_simple_http_parse_config(test_config_filename, "PATH", required_names);
    ASSERT_TRUE(templates.paths);

    C_SIMPLE_HTTP_ParsedConfig *first_path_map_wrapper =
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

    simple_archiver_list_free(&required_names);
    required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "TEST3",
      simple_archiver_helper_datastructure_cleanup_nop);

    c_simple_http_clean_up_parsed_config(&templates);
    templates =
      c_simple_http_parse_config(test_config_filename, "PATH", required_names);
    ASSERT_FALSE(templates.paths);

    simple_archiver_list_free(&required_names);
    required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "TEST",
      simple_archiver_helper_datastructure_cleanup_nop);

    c_simple_http_clean_up_parsed_config(&templates);
    templates =
      c_simple_http_parse_config(test_config_filename, "PATH", required_names);
    ASSERT_FALSE(templates.paths);
  }

  // Test http_template.
  {
    const char *test_http_template_filename =
      "/tmp/c_simple_http_template_test.config";
    __attribute__((cleanup(simple_archiver_helper_cleanup_FILE)))
    FILE *test_file = fopen(test_http_template_filename, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite("PATH=/\nHTML=<h1>Test</h1>\n", 1, 26, test_file)
        == 26);
    simple_archiver_helper_cleanup_FILE(&test_file);

    __attribute__((cleanup(simple_archiver_list_free)))
    SDArchiverLinkedList *required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "HTML",
      simple_archiver_helper_datastructure_cleanup_nop
    );

    __attribute__((cleanup(c_simple_http_clean_up_parsed_config)))
    C_SIMPLE_HTTP_ParsedConfig config = c_simple_http_parse_config(
      test_http_template_filename,
      "PATH",
      required_names
    );
    ASSERT_TRUE(config.paths != NULL);

    __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
    char *buf = c_simple_http_path_to_generated("/", &config);
    ASSERT_TRUE(buf != NULL);
    ASSERT_TRUE(strcmp(buf, "<h1>Test</h1>") == 0);
    simple_archiver_helper_cleanup_c_string(&buf);

    const char *test_http_template_filename2 =
      "/tmp/c_simple_http_template_test2.config";
    test_file = fopen(test_http_template_filename2, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          "PATH=/\nHTML=<h1>{{{testVar}}}</h1><br><h2>{{{testVar2}}}</h2>\n",
          1,
          62,
          test_file)
        == 62);
    ASSERT_TRUE(
        fwrite("testVar=''' Some text. '''\n", 1, 27, test_file)
        == 27);
    ASSERT_TRUE(
        fwrite("testVar2=''' More text. '''\n", 1, 28, test_file)
        == 28);
    simple_archiver_helper_cleanup_FILE(&test_file);

    c_simple_http_clean_up_parsed_config(&config);
    config = c_simple_http_parse_config(
      test_http_template_filename2,
      "PATH",
      required_names
    );
    ASSERT_TRUE(config.paths != NULL);

    buf = c_simple_http_path_to_generated("/", &config);
    ASSERT_TRUE(buf != NULL);
    printf("%s\n", buf);
    ASSERT_TRUE(
      strcmp(
        buf,
        "<h1> Some text. </h1><br><h2> More text. </h2>")
      == 0);
    simple_archiver_helper_cleanup_c_string(&buf);

    const char *test_http_template_filename3 =
      "/tmp/c_simple_http_template_test3.config";
    const char *test_http_template_html_filename =
      "/tmp/c_simple_http_template_test.html";
    test_file = fopen(test_http_template_filename3, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          "PATH=/\nHTML_FILE=/tmp/c_simple_http_template_test.html\n",
          1,
          55,
          test_file)
        == 55);
    ASSERT_TRUE(
        fwrite(
          "testVar=''' testVar text. '''\ntestVar2=''' testVar2 text. '''\n",
          1,
          62,
          test_file)
        == 62);
    simple_archiver_helper_cleanup_FILE(&test_file);

    test_file = fopen(test_http_template_html_filename, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          "<h1>{{{testVar}}}</h1><br><h2>{{{testVar2}}}</h2>",
          1,
          49,
          test_file)
        == 49);
    simple_archiver_helper_cleanup_FILE(&test_file);

    simple_archiver_list_free(&required_names);
    required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "HTML_FILE",
      simple_archiver_helper_datastructure_cleanup_nop);

    c_simple_http_clean_up_parsed_config(&config);
    config = c_simple_http_parse_config(
      test_http_template_filename3,
      "PATH",
      required_names
    );
    ASSERT_TRUE(config.paths != NULL);

    buf = c_simple_http_path_to_generated("/", &config);
    ASSERT_TRUE(buf != NULL);
    printf("%s\n", buf);
    ASSERT_TRUE(
      strcmp(
        buf,
        "<h1> testVar text. </h1><br><h2> testVar2 text. </h2>")
      == 0);
    simple_archiver_helper_cleanup_c_string(&buf);
  }

  RETURN()
}

// vim: ts=2 sts=2 sw=2
