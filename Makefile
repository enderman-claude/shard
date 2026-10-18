# shard: freestanding library + host tests. One unit per src/<subsystem>/<unit>/.
CC      ?= gcc
AR      ?= ar
CFLAGS  := -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
           -fpic -mno-red-zone -O2 -Iinclude
SRCS    := $(wildcard src/*/*/*.c)
OBJS    := $(patsubst src/%.c,build/%.o,$(SRCS))

all: build/libshard.a

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/libshard.a: $(OBJS)
	$(AR) rcs $@ $^

build/elf_fixture.so: tests/fixture/fixture.c
	@mkdir -p build
	$(CC) -std=c11 -O1 -fpic -shared -nostdlib -fno-stack-protector -o $@ $<

build/needs_lib.so: tests/fixture/needs_lib.c
	@mkdir -p build
	$(CC) -std=c11 -O1 -fpic -shared -o $@ $< -lm

build/test_all: tests/test_all.c build/libshard.a
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -o $@ $< build/libshard.a

test: build/test_all build/elf_fixture.so build/needs_lib.so
	./build/test_all build/elf_fixture.so

clean:
	rm -rf build

.PHONY: all test clean
