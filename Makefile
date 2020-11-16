CFLAGS += -O3 -g -ggdb -Isrc/include
LIBS += -lrt -lpthread

LIBRARY_FILES=$(wildcard src/include/*)

all: build/benchmark/fork_bandwidth build/benchmark/fork_latency build/benchmark/thread_bandwidth

build/%: src/%.c $(LIBRARY_FILES) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

build/benchmark/%: $(wildcard src/benchmark/*.h)
