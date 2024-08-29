COMMON_FLAGS = -Wall -Wextra -Wpedantic
DEBUG_FLAGS = -Og -g
RELEASE_FLAGS = -O3 -DNDEBUG

ifdef RELEASE
	CFLAGS = ${COMMON_FLAGS} ${RELEASE_FLAGS}
else
	CFLAGS = ${COMMON_FLAGS} ${DEBUG_FLAGS}
endif

SOURCES = \
		src/main.c \
		src/arg_parse.c \
		src/big_endian.c \
		src/tcp_socket.c \
		src/signal_handling.c \
		src/globals.c

OBJECT_DIR = objs
OBJECTS = $(addprefix ${OBJECT_DIR}/,$(patsubst %.c,%.c.o,${SOURCES}))

all: c_simple_http

c_simple_http: ${OBJECTS}
	gcc -o c_simple_http ${CFLAGS} $^

.PHONY: clean

clean:
	rm -f c_simple_http
	rm -rf ${OBJECT_DIR}

${OBJECT_DIR}/%.c.o: %.c
	@mkdir -p $(dir $@)
	gcc -o $@ -c ${CFLAGS} $<
