CFLAGS += -O3 -g -ggdb
LIBS += -lrt

LIBRARY_FILES=$(wildcard src/include/*)

all: build/fork_test

build:
	mkdir -p $@

build/%: src/%.c $(LIBRARY_FILES) Makefile | build
	gcc $(CFLAGS) $< -o $@ $(LIBS)
