#include <spsc_queue.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const size_t SIZE = 4 * 1024 * 1024;
const size_t OPS = 1000000;
const size_t MSG_SIZE = 10 * 1024;
const double GB = 1024 * 1024 * 1024;

static void writer(struct spsc_queue *q)
{
  uint32_t sum = 0;

  struct timespec start;
  struct timespec finish;

  // sleep for 100ms
  usleep(1000 * 100);

  clock_gettime(CLOCK_MONOTONIC, &start);

  for (size_t i = 0; i < OPS; i++)
  {
    uint8_t *from = spsc_queue_write(q, MSG_SIZE);
    uint8_t *to = from + MSG_SIZE;

    uint8_t n = 0;

    for (uint8_t *it = from; it < to; it++)
    {
      *it = n;
      sum += n;
      n++;
    }

    spsc_queue_write_commit(q, MSG_SIZE);
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = (OPS * MSG_SIZE) / elapsed;

  printf("Writer: sum %u, took: %lf, %lf GB/s\n", sum, elapsed, bytes_per_second / GB);
}

static void receiver(struct spsc_queue *q)
{
  uint32_t sum = 0;

  struct timespec start;
  struct timespec finish;

  for (size_t i = 0; i < OPS; i++)
  {
    const uint8_t *from = spsc_queue_read(q, MSG_SIZE);
    const uint8_t *to = from + MSG_SIZE;

    if (!i)
      clock_gettime(CLOCK_MONOTONIC, &start);

    for (const uint8_t *it = from; it < to; it++)
    {
      sum += *it;
    }

    spsc_queue_read_commit(q, MSG_SIZE);
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = (OPS * MSG_SIZE) / elapsed;

  printf("Reader: sum %u, took: %lf, %lf GB/s\n", sum, elapsed, bytes_per_second / GB);
}