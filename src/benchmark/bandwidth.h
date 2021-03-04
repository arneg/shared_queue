#include <spsc_queue.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

const double GB = 1024 * 1024 * 1024;
const size_t SIZE = 4 * 1024 * 1024;
const size_t M = 1000000;
const size_t KB = 1024;

static unsigned int get_seed()
{
  pid_t tid = gettid();
  pid_t pid = getpid();

  size_t tmp = (size_t)tid ^ (size_t)pid;

  return (unsigned int)(tmp ^ (tmp >> 32));
}

static inline size_t minimum(size_t a, size_t b)
{
  return a < b ? a : b;
}

static void write_to_queue(struct spsc_queue *q,
                           size_t size,
                           size_t min_message_size,
                           size_t random_max)
{
  struct timespec start;
  struct timespec finish;
  size_t max_message_size = min_message_size + random_max;
  char *buf = malloc(max_message_size);
  unsigned int seed = get_seed();
  size_t message_size = max_message_size;
  size_t ops = 0;

  assert(buf);

  memset(buf, 0, max_message_size);

  // sleep for 100ms
  usleep(1000 * 100);

  clock_gettime(CLOCK_MONOTONIC, &start);

  for (size_t i = 0; i < size; i+=message_size, ops++)
  {
    if (random_max) {
      message_size = min_message_size + rand_r(&seed) % random_max;
      message_size = minimum(message_size, size - i);
    }
    spsc_queue_write_from(q, buf, message_size);
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = size / elapsed;

  printf("Writer took: %lf, %lf GB/s, wrote 0x%llX bytes, %llu ops\n",
         elapsed, bytes_per_second / GB, (unsigned long long)size, (unsigned long long)ops);
  free(buf);
}

static void read_from_queue(struct spsc_queue *q,
                            size_t size,
                            size_t min_message_size,
                            size_t random_max)
{
  struct timespec start;
  struct timespec finish;
  size_t max_message_size = min_message_size + random_max;
  char *buf = malloc(max_message_size);
  unsigned int seed = get_seed();
  size_t message_size = max_message_size;
  size_t ops = 0;

  assert(buf);

  for (size_t i = 0; i < size; i+=message_size, ops++)
  {
    if (random_max) {
      message_size = min_message_size + rand_r(&seed) % random_max;
      message_size = minimum(message_size, size - i);
    }
    spsc_queue_read_to(q, buf, message_size);

    if (!i)
      clock_gettime(CLOCK_MONOTONIC, &start);
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = size / elapsed;

  printf("Reader took: %lf, %lf GB/s, read 0x%llX bytes, %llu ops\n",
         elapsed, bytes_per_second / GB, (unsigned long long)size, (unsigned long long)ops);
  free(buf);
}


static void writer(struct spsc_queue *q)
{
  printf("Starting homogenous test. (read_size == write_size).\n");
  write_to_queue(q, (10 * M) * (10 * KB), 10 * KB, 0);

  usleep(1000 * 100);
  printf("Starting inhomogenous test (read_size > write_size).\n");
  write_to_queue(q, (10 * M) * (10 * KB), 10 * KB, 0);

  usleep(1000 * 100);
  printf("Starting inhomogenous test (read_size < write_size).\n");
  write_to_queue(q, (1 * M) * (100 * KB), 100 * KB, 0);

  usleep(1000 * 100);
  printf("Starting homogenous random test. (read_size ~ write_size).\n");
  write_to_queue(q, (10 * M) * (10 * KB), 9 * KB, 2 * KB);

  usleep(1000 * 100);
  printf("Starting inhomogenous random test (read_size ~> write_size).\n");
  write_to_queue(q, (10 * M) * (10 * KB), 9 * KB, 2 * KB);

  usleep(1000 * 100);
  printf("Starting inhomogenous random test (read_size ~< write_size).\n");
  write_to_queue(q, (1 * M) * (100 * KB), 90 * KB, 20 * KB);
}

static void reader(struct spsc_queue *q)
{
  read_from_queue(q, (10 * M) * (10 * KB), 10 * KB, 0);

  read_from_queue(q, (1 * M) * (100 * KB), 100 * KB, 0);

  read_from_queue(q, (10 * M) * (10 * KB), 10 * KB, 0);

  read_from_queue(q, (10 * M) * (10 * KB), 9 * KB, 2 * KB);

  read_from_queue(q, (1 * M) * (100 * KB), 90 * KB, 20 * KB);

  read_from_queue(q, (10 * M) * (10 * KB), 9 * KB, 2 * KB);
}
