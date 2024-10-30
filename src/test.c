// Standard library includes.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// POSIX includes.
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

// Local includes.
#include "config.h"
#include "helpers.h"
#include "http_template.h"
#include "http.h"
#include "html_cache.h"
#include "constants.h"
#include "static.h"

// Third party includes.
#include <SimpleArchiver/src/helpers.h>
#include <SimpleArchiver/src/data_structures/hash_map.h>
#include <SimpleArchiver/src/data_structures/linked_list.h>

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

int main(int argc, char **argv) {
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
    //printf("%s\n", buf);
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
    //printf("%s\n", buf);
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
    //printf("%s\n", buf);
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
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl/////", 13);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl?key=value", 18);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl#client_data", 20);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl////?key=value", 22);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl///#client_data", 23);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl");
    free(stripped_path_buf);

    stripped_path_buf = c_simple_http_strip_path("/someurl/////inner", 18);
    //printf("stripped path: %s\n", stripped_path_buf);
    CHECK_STREQ(stripped_path_buf, "/someurl/inner");
    free(stripped_path_buf);
  }

  // Test helpers.
  {
    __attribute__((cleanup(simple_archiver_list_free)))
    SDArchiverLinkedList *list = simple_archiver_list_init();

    c_simple_http_add_string_part(list, "one\n", 0);
    c_simple_http_add_string_part(list, "two\n", 0);
    c_simple_http_add_string_part(list, "three\n", 0);

    __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
    char *buf = c_simple_http_combine_string_parts(list);
    ASSERT_TRUE(buf);
    ASSERT_TRUE(strcmp(buf, "one\ntwo\nthree\n") == 0);
    free(buf);
    buf = NULL;

    char hex_result = c_simple_http_helper_hex_to_value('2', 'f');
    CHECK_TRUE(hex_result == '/');
    hex_result = c_simple_http_helper_hex_to_value('2', 'F');
    CHECK_TRUE(hex_result == '/');

    hex_result = c_simple_http_helper_hex_to_value('7', 'a');
    CHECK_TRUE(hex_result == 'z');
    hex_result = c_simple_http_helper_hex_to_value('7', 'A');
    CHECK_TRUE(hex_result == 'z');

    hex_result = c_simple_http_helper_hex_to_value('4', '1');
    CHECK_TRUE(hex_result == 'A');

    buf = c_simple_http_helper_unescape_uri("%2fderp%2Fdoop%21");
    CHECK_TRUE(strcmp(buf, "/derp/doop!") == 0);
    free(buf);
    buf = NULL;

    buf = c_simple_http_helper_unescape_uri("%41%42%43%25%5A%5a");
    CHECK_TRUE(strcmp(buf, "ABC%ZZ") == 0);
    free(buf);
    buf = NULL;

    DIR *dirp = opendir("/tmp/create_dirs_dir");
    uint_fast8_t dir_exists = dirp ? 1 : 0;
    closedir(dirp);
    ASSERT_FALSE(dir_exists);

    int ret = c_simple_http_helper_mkdir_tree("/tmp/create_dirs_dir/dir/");
    int ret2 = rmdir("/tmp/create_dirs_dir/dir");
    int ret3 = rmdir("/tmp/create_dirs_dir");

    CHECK_TRUE(ret == 0);
    CHECK_TRUE(ret2 == 0);
    CHECK_TRUE(ret3 == 0);
  }

  // Test html_cache.
  {
    char *ret = c_simple_http_path_to_cache_filename("/");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "ROOT") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("////");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "ROOT") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Finner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/inner////");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Finner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer/inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Fouter0x2Finner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer/inner////");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Fouter0x2Finner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer///inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Fouter0x2Finner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer/with_hex_0x2F_inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "%2Fouter%2Fwith_hex_0x2F_inner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer/0x2F_hex_inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "%2Fouter%2F0x2F_hex_inner") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename("/outer0x2F/inner_hex_0x2F");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "%2Fouter0x2F%2Finner_hex_0x2F") == 0);
    free(ret);

    ret = c_simple_http_path_to_cache_filename(
        "/0x2Fouter0x2F/0x2Finner_0x2F_hex_0x2F");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "%2F0x2Fouter0x2F%2F0x2Finner_0x2F_hex_0x2F") == 0);
    free(ret);

    ret = c_simple_http_cache_filename_to_path("0x2Fouter0x2Finner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "/outer/inner") == 0);
    free(ret);

    ret = c_simple_http_cache_filename_to_path(
      "0x2Fouter0x2Finner0x2F%2F0x2Fmore_inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "/outer/inner/%2F/more_inner") == 0);
    free(ret);

    ret = c_simple_http_cache_filename_to_path("%2Fouter%2Finner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "/outer/inner") == 0);
    free(ret);

    ret = c_simple_http_cache_filename_to_path(
      "%2Fouter%2Finner%2F0x2F%2Fmore_inner");
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "/outer/inner/0x2F/more_inner") == 0);
    free(ret);

    const char *uri0 = "/a/simple/url/with/inner/paths";
    ret =
      c_simple_http_path_to_cache_filename(uri0);
    ASSERT_TRUE(ret);
    CHECK_TRUE(
      strcmp(ret, "0x2Fa0x2Fsimple0x2Furl0x2Fwith0x2Finner0x2Fpaths")
      == 0);
    char *ret2 = c_simple_http_cache_filename_to_path(ret);
    free(ret);
    ASSERT_TRUE(ret2);
    CHECK_TRUE(strcmp(ret2, uri0) == 0);
    free(ret2);

    const char *uri1 = "/a/url/with/0x2F/in/it";
    ret =
      c_simple_http_path_to_cache_filename(uri1);
    ASSERT_TRUE(ret);
    CHECK_TRUE(
      strcmp(ret, "%2Fa%2Furl%2Fwith%2F0x2F%2Fin%2Fit")
      == 0);
    ret2 = c_simple_http_cache_filename_to_path(ret);
    free(ret);
    ASSERT_TRUE(ret2);
    CHECK_TRUE(strcmp(ret2, uri1) == 0);
    free(ret2);

    const char *uri2 = "/";
    ret =
      c_simple_http_path_to_cache_filename(uri2);
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "ROOT") == 0);
    ret2 = c_simple_http_cache_filename_to_path(ret);
    free(ret);
    ASSERT_TRUE(ret2);
    CHECK_TRUE(strcmp(ret2, uri2) == 0);
    free(ret2);

    const char *uri3 = "/a";
    ret =
      c_simple_http_path_to_cache_filename(uri3);
    ASSERT_TRUE(ret);
    CHECK_TRUE(strcmp(ret, "0x2Fa") == 0);
    ret2 = c_simple_http_cache_filename_to_path(ret);
    free(ret);
    ASSERT_TRUE(ret2);
    CHECK_TRUE(strcmp(ret2, uri3) == 0);
    free(ret2);

    // Set up test config to get template map to test cache.
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_filename5 =
      "/tmp/c_simple_http_template_test5.config";
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_html_filename3 =
      "/tmp/c_simple_http_template_test3.html";
    __attribute__((cleanup(test_internal_cleanup_delete_temporary_file)))
    const char *test_http_template_html_var_filename2 =
      "/tmp/c_simple_http_template_test_var2.html";

    FILE *test_file = fopen(test_http_template_filename5, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
      fwrite(
        "PATH=/\nHTML_FILE=/tmp/c_simple_http_template_test3.html\n",
        1,
        56,
        test_file)
      == 56);
    ASSERT_TRUE(
      fwrite(
        "VAR_FILE=/tmp/c_simple_http_template_test_var2.html\n",
        1,
        52,
        test_file)
      == 52);
    fclose(test_file);

    test_file = fopen(test_http_template_html_filename3, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
      fwrite(
        "<body>{{{VAR_FILE}}}</body>\n",
        1,
        28,
        test_file)
      == 28);
    fclose(test_file);

    test_file = fopen(test_http_template_html_var_filename2, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
      fwrite(
        "Some test text.<br>Yep.",
        1,
        23,
        test_file)
      == 23);
    fclose(test_file);

    __attribute__((cleanup(c_simple_http_clean_up_parsed_config)))
    C_SIMPLE_HTTP_ParsedConfig templates =
      c_simple_http_parse_config(test_http_template_filename5, "PATH", NULL);
    ASSERT_TRUE(templates.paths);

    // Run cache function. Should return >0 due to new/first cache entry.
    __attribute__((cleanup(simple_archiver_helper_cleanup_c_string)))
    char *buf = NULL;
    int int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      0xFFFFFFFF,
      &buf);

    CHECK_TRUE(int_ret > 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<body>Some test text.<br>Yep.</body>\n") == 0);
    free(buf);
    buf = NULL;

    // Check/get size of cache file.
    FILE *cache_file = fopen("/tmp/c_simple_http_cache_dir/ROOT", "r");
    uint_fast8_t cache_file_exists = cache_file ? 1 : 0;
    fseek(cache_file, 0, SEEK_END);
    const long cache_file_size_0 = ftell(cache_file);
    fclose(cache_file);
    ASSERT_TRUE(cache_file_exists);

    // Re-run cache function, checking that it is not invalidated.
    int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      0xFFFFFFFF,
      &buf);
    CHECK_TRUE(int_ret == 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<body>Some test text.<br>Yep.</body>\n") == 0);
    free(buf);
    buf = NULL;

    // Check/get size of cache file.
    cache_file = fopen("/tmp/c_simple_http_cache_dir/ROOT", "r");
    cache_file_exists = cache_file ? 1 : 0;
    fseek(cache_file, 0, SEEK_END);
    const long cache_file_size_1 = ftell(cache_file);
    fclose(cache_file);
    ASSERT_TRUE(cache_file_exists);
    CHECK_TRUE(cache_file_size_0 == cache_file_size_1);

    // Change a file used by the template for PATH=/ .
    // Sleep first since granularity is by the second.
    puts("Sleeping for two seconds to ensure edited file's timestamp has "
      "changed...");
    sleep(2);
    puts("Done sleeping.");
    test_file = fopen(test_http_template_html_var_filename2, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
      fwrite(
        "Alternate test text.<br>Yep.",
        1,
        28,
        test_file)
      == 28);
    fclose(test_file);

    // Re-run cache function, checking that it is invalidated.
    int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      0xFFFFFFFF,
      &buf);
    CHECK_TRUE(int_ret > 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<body>Alternate test text.<br>Yep.</body>\n") == 0);
    free(buf);
    buf = NULL;

    // Get/check size of cache file.
    cache_file = fopen("/tmp/c_simple_http_cache_dir/ROOT", "r");
    cache_file_exists = cache_file ? 1 : 0;
    fseek(cache_file, 0, SEEK_END);
    const long cache_file_size_2 = ftell(cache_file);
    fclose(cache_file);
    ASSERT_TRUE(cache_file_exists);
    CHECK_TRUE(cache_file_size_0 != cache_file_size_2);

    // Re-run cache function, checking that it is not invalidated.
    int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      0xFFFFFFFF,
      &buf);
    CHECK_TRUE(int_ret == 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<body>Alternate test text.<br>Yep.</body>\n") == 0);
    free(buf);
    buf = NULL;

    // Get/check size of cache file.
    cache_file = fopen("/tmp/c_simple_http_cache_dir/ROOT", "r");
    cache_file_exists = cache_file ? 1 : 0;
    fseek(cache_file, 0, SEEK_END);
    const long cache_file_size_3 = ftell(cache_file);
    fclose(cache_file);
    ASSERT_TRUE(cache_file_exists);
    CHECK_TRUE(cache_file_size_2 == cache_file_size_3);

    // Edit config file.
    puts("Sleeping for two seconds to ensure edited file's timestamp has "
      "changed...");
    sleep(2);
    puts("Done sleeping.");
    test_file = fopen(test_http_template_filename5, "w");
    ASSERT_TRUE(test_file);

    ASSERT_TRUE(
      fwrite(
        "PATH=/\nHTML='''<h1>{{{VAR_FILE}}}</h1>'''\n",
        1,
        42,
        test_file)
      == 42);
    ASSERT_TRUE(
      fwrite(
        "VAR_FILE=/tmp/c_simple_http_template_test_var2.html\n",
        1,
        52,
        test_file)
      == 52);

    fclose(test_file);

    // Re-run cache function, checking that it is invalidated.
    int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      0xFFFFFFFF,
      &buf);
    CHECK_TRUE(int_ret > 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<h1>Alternate test text.<br>Yep.</h1>") == 0);
    free(buf);
    buf = NULL;

    puts("Sleeping for two seconds to ensure cache file has aged...");
    sleep(2);
    puts("Done sleeping.");
    // Re-run cache function, checking that it is invalidated.
    int_ret = c_simple_http_cache_path(
      "/",
      test_http_template_filename5,
      "/tmp/c_simple_http_cache_dir",
      &templates,
      1,
      &buf);
    CHECK_TRUE(int_ret > 0);
    ASSERT_TRUE(buf);
    CHECK_TRUE(strcmp(buf, "<h1>Alternate test text.<br>Yep.</h1>") == 0);
    free(buf);
    buf = NULL;

    // Cleanup.
    remove("/tmp/c_simple_http_cache_dir/ROOT");
    rmdir("/tmp/c_simple_http_cache_dir");
  }

  // Test static.
  {
    FILE *fd = fopen("/usr/bin/xdg-mime", "rb");
    int_fast8_t is_xdg_mime_exists = fd ? 1 : 0;
    fclose(fd);

    if (is_xdg_mime_exists) {
      CHECK_TRUE(c_simple_http_is_xdg_mime_available());

      C_SIMPLE_HTTP_StaticFileInfo info = c_simple_http_get_file(".", argv[0], 0);
      CHECK_TRUE(info.buf);
      CHECK_TRUE(info.buf_size > 0);
      CHECK_TRUE(info.mime_type);
      CHECK_TRUE(info.result == STATIC_FILE_RESULT_OK);
      printf("unit test mime type is: %s\n", info.mime_type);
      c_simple_http_cleanup_static_file_info(&info);
    } else {
      CHECK_FALSE(c_simple_http_is_xdg_mime_available());
    }

    C_SIMPLE_HTTP_StaticFileInfo info = c_simple_http_get_file(".", argv[0], 1);
    CHECK_TRUE(info.buf);
    CHECK_TRUE(info.buf_size > 0);
    CHECK_TRUE(info.mime_type);
    CHECK_TRUE(info.result == STATIC_FILE_RESULT_OK);
    CHECK_STREQ(info.mime_type, "application/octet-stream");
    c_simple_http_cleanup_static_file_info(&info);
  }

  RETURN()
}

// vim: et ts=2 sts=2 sw=2
