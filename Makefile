CFLAGS += -O3 -g -ggdb -Isrc/include
LIBS += -lrt

LIBRARY_FILES=$(wildcard src/include/*)

all: build/benchmark/fork_bandwidth build/benchmark/fork_latency

build/%: src/%.c $(LIBRARY_FILES) Makefile
	mkdir -p $(dir $@)
	gcc $(CFLAGS) $< -o $@ $(LIBS)
