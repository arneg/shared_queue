#include "include/circular_area.h"
#include "include/shared_counter.h"
#include "include/shared_alloc.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

const size_t SIZE = 4 * 1024 * 1024;
const size_t OPS = 1000000;
const size_t MSG_SIZE = 10 * 1024;
const double GB = 1024 * 1024 * 1024;

struct queue
{
  struct shared_counter write_offset;
  struct shared_counter read_offset;
};

struct queue *allocate_queue()
{
  return shared_alloc_anonymous(sizeof(struct queue));
}

void free_queue(struct queue *q)
{
  shared_alloc_free(q, sizeof(struct queue)); 
}

void parent(struct queue *q, struct circular_area *area)
{
  uint32_t write_offset = 0;
  uint32_t size = area->size;
  size_t stalled = 0;

  uint32_t sum = 0;

  struct timespec start;
  struct timespec finish;

  clock_gettime(CLOCK_MONOTONIC, &start);

  for (size_t i = 0; i < OPS; i++)
  {
    uint32_t read_offset = shared_counter_read(&q->read_offset);

    if (size < (write_offset - read_offset) + MSG_SIZE)
    {
      stalled ++;
      i--;
      shared_counter_wait(&q->read_offset, read_offset);
      continue;
    }

    uint8_t *from = circular_area_get_pointer(area, write_offset);
    uint8_t *to = from + MSG_SIZE;

    uint8_t n = 0;

    for (uint8_t *it = from; it < to; it++)
    {
      *it = n;
      sum += n;
      n++;
    }

    shared_counter_write(&q->write_offset, write_offset + MSG_SIZE);

    read_offset = shared_counter_read(&q->read_offset);

    if (read_offset == write_offset)
    {
      // wake the reader
      shared_counter_wake(&q->write_offset);
    }

    write_offset += MSG_SIZE;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = (OPS * MSG_SIZE) / elapsed;

  printf("Writer: stalled %lu, sum %u, took: %lf, %lf GB/s\n", stalled, sum, elapsed, bytes_per_second / GB);
}

void child(struct queue *q, struct circular_area *area)
{
  uint32_t read_offset = 0;
  uint32_t size = area->size;

  uint32_t sum = 0;
  size_t stalled = 0;

  struct timespec start;
  struct timespec finish;

  clock_gettime(CLOCK_MONOTONIC, &start);

  for (size_t i = 0; i < OPS; i++)
  {
    uint32_t write_offset = shared_counter_read(&q->write_offset);

    if (write_offset == read_offset)
    {
      stalled++;
      i--;
      shared_counter_wait(&q->write_offset, write_offset);
      continue;
    }

    const uint8_t *from = circular_area_get_pointer(area, read_offset);
    const uint8_t *to = from + MSG_SIZE;

    for (const uint8_t *it = from; it < to; it++)
    {
      sum += *it;
    }

    shared_counter_write(&q->read_offset, read_offset + MSG_SIZE);

    write_offset = shared_counter_read(&q->write_offset);

    if (size < (write_offset - read_offset) + MSG_SIZE)
    {
      shared_counter_wake(&q->read_offset);
    }

    read_offset += MSG_SIZE;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);

  double elapsed = finish.tv_sec - start.tv_sec + (finish.tv_nsec - start.tv_nsec) * 1E-9;
  double bytes_per_second = (OPS * MSG_SIZE) / elapsed;

  printf("Reader: stalled %lu, sum %u, took: %lf, %lf GB/s\n", stalled, sum, elapsed, bytes_per_second / GB);
}

int main(int argc, const char **argv)
{
  struct circular_area area;

  if (circular_area_allocate_shared_anonymous(&area, SIZE))
  {
    printf("Creating the area failed: %s\n", strerror(errno));
    return 1;
  }

  struct queue *q = allocate_queue();

  if (q == MAP_FAILED)
  {
    printf("Creating the queue failed: %s\n", strerror(errno));
    return 1;
  }

  if (fork())
  {
    child(q, &area);
  }
  else
  {
    parent(q, &area);
  }

  free_queue(q);
  circular_area_free(&area);

  return 0;
}
