# Makefile — Tail programming language
#
# Builds the `tail` interpreter from scratch in C11 with no external
# dependencies beyond the C standard library.
#
# Targets:
#   make          build the release binary
#   make test     run the test suite
#   make dev      build with AddressSanitizer + UBSan (development)
#   make clean    remove build artifacts
#
# SPDX-License-Identifier: MIT

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=

SRCS := main.c lexer.c parser.c value.c interpreter.c
OBJS := $(SRCS:.c=.o)

BIN := tail

.PHONY: all test dev clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c tail.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Development build with sanitizers enabled.
dev:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	           LDFLAGS="-fsanitize=address,undefined" $(BIN)

test: $(BIN)
	./test/run_tests.sh

clean:
	rm -f $(OBJS) $(BIN)
