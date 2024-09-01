COMMON_FLAGS = -Wall -Wextra -Wpedantic \
	-Ithird_party
DEBUG_FLAGS = -Og -g
RELEASE_FLAGS = -O3 -DNDEBUG

ifdef RELEASE
	CFLAGS = ${COMMON_FLAGS} ${RELEASE_FLAGS}
else
	CFLAGS = ${COMMON_FLAGS} ${DEBUG_FLAGS}
endif

HEADERS = \
	src/arg_parse.h \
	src/big_endian.h \
	src/tcp_socket.h \
	src/globals.h \
	src/signal_handling.h \
	src/constants.h \
	src/http.h \
	src/config.h

SOURCES = \
		src/main.c \
		src/arg_parse.c \
		src/big_endian.c \
		src/tcp_socket.c \
		src/signal_handling.c \
		src/globals.c \
		src/http.c \
		src/config.c \
		third_party/SimpleArchiver/src/helpers.c \
		third_party/SimpleArchiver/src/data_structures/linked_list.c \
		third_party/SimpleArchiver/src/data_structures/hash_map.c \
		third_party/SimpleArchiver/src/data_structures/priority_heap.c \
		third_party/SimpleArchiver/src/algorithms/linear_congruential_gen.c

OBJECT_DIR = objs
OBJECTS = $(addprefix ${OBJECT_DIR}/,$(patsubst %.c,%.c.o,${SOURCES}))

all: c_simple_http unit_test

c_simple_http: ${OBJECTS}
	gcc -o c_simple_http ${CFLAGS} $^

unit_test: $(filter-out ${OBJECT_DIR}/src/main.c.o,${OBJECTS}) ${OBJECT_DIR}/src/test.c.o
	gcc -o unit_test ${CFLAGS} $^

.PHONY: clean

clean:
	rm -f c_simple_http
	rm -f unit_test
	rm -rf ${OBJECT_DIR}

${OBJECT_DIR}/%.c.o: %.c ${HEADERS}
	@mkdir -p $(dir $@)
	gcc -o $@ -c ${CFLAGS} $<
