cmake_minimum_required(VERSION 3.25)
project(c_simple_http C)

set(c_simple_http_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/arg_parse.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/big_endian.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_socket.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/signal_handling.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/globals.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/http.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/http_template.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/helpers.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/html_cache.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/static.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/generate.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src/helpers.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src/data_structures/linked_list.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src/data_structures/hash_map.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src/data_structures/priority_heap.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src/algorithms/linear_congruential_gen.c"
)

if(NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
    message("Defaulting to \"Debug\" build type.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
else()
    message("Using build type \"${CMAKE_BUILD_TYPE}\".")
endif()

add_executable(c_simple_http
  ${c_simple_http_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
)
target_include_directories(c_simple_http PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_executable(unit_tests
  ${c_simple_http_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/test.c"
)
target_include_directories(unit_tests PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

target_compile_options(c_simple_http PUBLIC
$<IF:$<CONFIG:Debug>,-Og,-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-fPIE -pie
-Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion
)

target_link_options(c_simple_http PUBLIC
$<IF:$<CONFIG:Debug>,-Og,-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-Wl,-z,noexecstack
-Wl,-z,relro -Wl,-z,now
-fPIE -pie
-Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion
)

target_compile_options(unit_tests PUBLIC
$<IF:$<CONFIG:Debug>,-Og,-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-fPIE -pie
-Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion
)

target_link_options(unit_tests PUBLIC
$<IF:$<CONFIG:Debug>,-Og,-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-Wl,-z,noexecstack
-Wl,-z,relro -Wl,-z,now
-fPIE -pie
-Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion
)
