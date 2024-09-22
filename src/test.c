// Standard library includes.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Local includes.
#include "SimpleArchiver/src/data_structures/linked_list.h"
#include "config.h"
#include "http_template.h"
#include "http.h"

// Third party includes.
#include <SimpleArchiver/src/helpers.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>

static int32_t checks_checked = 0;
static int32_t checks_passed = 0;

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

void test_internal_cleanup_delete_temporary_file(const char **filename) {
  if (filename && *filename) {
    if (remove(*filename) != 0) {
      fprintf(stderr, "ERROR Failed to remove file \"%s\"!\n", *filename);
    }
  }
}

int test_internal_check_matching_string_in_list(void *value, void *ud) {
  if (value && ud) {
    if (strcmp(value, ud) == 0) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  // Test config.
  {
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
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
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
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

    size_t output_buf_size;

    __attribute__((cleanup(simple_archiver_list_free)))
    SDArchiverLinkedList *filenames_list = NULL;

    __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
    char *buf = c_simple_http_path_to_generated(
        "/", &config, &output_buf_size, &filenames_list);
    ASSERT_TRUE(buf != NULL);
    ASSERT_TRUE(strcmp(buf, "<h1>Test</h1>") == 0);
    CHECK_TRUE(output_buf_size == 13);
    CHECK_TRUE(filenames_list->count == 0);
    simple_archiver_helper_cleanup_c_string(&buf);
    simple_archiver_list_free(&filenames_list);

    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
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

    buf = c_simple_http_path_to_generated(
        "/", &config, &output_buf_size, &filenames_list);
    ASSERT_TRUE(buf != NULL);
    printf("%s\n", buf);
    ASSERT_TRUE(
      strcmp(
        buf,
        "<h1> Some text. </h1><br><h2> More text. </h2>")
      == 0);
    CHECK_TRUE(output_buf_size == 46);
    CHECK_TRUE(filenames_list->count == 0);
    simple_archiver_helper_cleanup_c_string(&buf);
    simple_archiver_list_free(&filenames_list);

    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_filename3 =
      "/tmp/c_simple_http_template_test3.config";
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
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

    buf = c_simple_http_path_to_generated(
        "/", &config, &output_buf_size, &filenames_list);
    ASSERT_TRUE(buf != NULL);
    printf("%s\n", buf);
    ASSERT_TRUE(
      strcmp(
        buf,
        "<h1> testVar text. </h1><br><h2> testVar2 text. </h2>")
      == 0);
    CHECK_TRUE(output_buf_size == 53);
    CHECK_TRUE(filenames_list->count == 1);
    CHECK_TRUE(simple_archiver_list_get(
        filenames_list,
        test_internal_check_matching_string_in_list,
        (void*)test_http_template_html_filename)
      != NULL);
    simple_archiver_helper_cleanup_c_string(&buf);
    simple_archiver_list_free(&filenames_list);

    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_filename4 =
      "/tmp/c_simple_http_template_test4.config";
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_html_filename2 =
      "/tmp/c_simple_http_template_test2.html";
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_html_var_filename =
      "/tmp/c_simple_http_template_test_var.html";
    test_file = fopen(test_http_template_filename4, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          "PATH=/\nHTML_FILE=/tmp/c_simple_http_template_test2.html\n",
          1,
          56,
          test_file)
        == 56);
    ASSERT_TRUE(
        fwrite(
          "testVar_FILE=/tmp/c_simple_http_template_test_var.html\n",
          1,
          55,
          test_file)
        == 55);
    simple_archiver_helper_cleanup_FILE(&test_file);

    test_file = fopen(test_http_template_html_filename2, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          "<h1>{{{testVar_FILE}}}</h1>",
          1,
          27,
          test_file)
        == 27);
    simple_archiver_helper_cleanup_FILE(&test_file);

    test_file = fopen(test_http_template_html_var_filename, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
        fwrite(
          " some test text in test var file. ",
          1,
          34,
          test_file)
        == 34);
    simple_archiver_helper_cleanup_FILE(&test_file);

    simple_archiver_list_free(&required_names);
    required_names = simple_archiver_list_init();
    simple_archiver_list_add(
      required_names,
      "HTML_FILE",
      simple_archiver_helper_datastructure_cleanup_nop);
    simple_archiver_list_add(
      required_names,
      "testVar_FILE",
      simple_archiver_helper_datastructure_cleanup_nop);

    c_simple_http_clean_up_parsed_config(&config);

    config = c_simple_http_parse_config(
      test_http_template_filename4,
      "PATH",
      required_names
    );
    ASSERT_TRUE(config.paths != NULL);

    buf = c_simple_http_path_to_generated(
        "/", &config, &output_buf_size, &filenames_list);
    ASSERT_TRUE(buf != NULL);
    printf("%s\n", buf);
    ASSERT_TRUE(
      strcmp(
        buf,
        "<h1> some test text in test var file. </h1>")
      == 0);
    CHECK_TRUE(output_buf_size == 43);
    CHECK_TRUE(filenames_list->count == 2);
    CHECK_TRUE(simple_archiver_list_get(
        filenames_list,
        test_internal_check_matching_string_in_list,
        (void*)test_http_template_html_filename2)
      != NULL);
    CHECK_TRUE(simple_archiver_list_get(
        filenames_list,
        test_internal_check_matching_string_in_list,
        (void*)test_http_template_html_var_filename)
      != NULL);
    simple_archiver_helper_cleanup_c_string(&buf);
  }

  // Test http.
  {
    __attribute((cleanup(simple_archiver_hash_map_free)))
    SDArchiverHashMap *headers_map = c_simple_http_request_to_headers_map(
      "GET / HTTP/1.1\nUser-Agent: Blah\nHost: some host", 47);
    ASSERT_TRUE(headers_map);

    const char *ret =
      simple_archiver_hash_map_get(headers_map, "user-agent", 11);
    ASSERT_TRUE(ret);
    CHECK_STREQ(ret, "User-Agent: Blah");

    ret = simple_archiver_hash_map_get(headers_map, "host", 5);
    ASSERT_TRUE(ret);
    CHECK_STREQ(ret, "Host: some host");

    char *stripped_path_buf = c_simple_http_strip_path("/", 1);
    CHECK_STREQ(stripped_path_buf, "/");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("//", 2);
    CHECK_STREQ(stripped_path_buf, "/");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("///", 3);
    CHECK_STREQ(stripped_path_buf, "/");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl", 8);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl/", 9);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl/////", 13);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl?key=value", 18);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl#client_data", 20);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl////?key=value", 22);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl///#client_data", 23);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);
  }

  RETURN()
}

// vim: et ts=2 sts=2 sw=2
