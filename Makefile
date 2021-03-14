CFLAGS += -O3 -g -ggdb -Isrc/include -DNDEBUG
CXXFLAGS += -O3 -g -ggdb -Isrc/include -DNDEBUG
LIBS += -lrt -lpthread

LIBRARY_FILES=$(wildcard src/include/*)

all: build/benchmark/fork_bandwidth build/benchmark/fork_latency build/benchmark/thread_bandwidth build/benchmark/thread_bandwidth_cpp

build/%: src/%.c $(LIBRARY_FILES) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

build/benchmark/%: src/benchmark/%.c $(LIBRARY_FILES) Makefile $(wildcard src/benchmark/*.h)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

build/benchmark/%: src/benchmark/%.cpp $(LIBRARY_FILES) Makefile $(wildcard src/benchmark/*.h)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
