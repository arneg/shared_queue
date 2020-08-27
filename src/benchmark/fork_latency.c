#include <spsc_queue.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

static const size_t SIZE = 4 * 1024;
static const size_t OPS = 10000;

void parent(struct spsc_queue *q)
{
  uint32_t sum = 0;

  usleep(10000);

  for (size_t i = 0; i < OPS; i++)
  {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    
    spsc_queue_write_from(q, &now, sizeof(now));

    usleep(100);
  }

  printf("Writer done.\n");
}

int cmp(const void *a, const void *b)
{
  size_t t1 = *(size_t*)a;
  size_t t2 = *(size_t*)b;

  if (t1 == t2) return 0;

  return (t1 < t2) ? -1 : 1;
}

void child(struct spsc_queue *q)
{
  size_t data[OPS];

  for (size_t i = 0; i < OPS; i++)
  {
    struct timespec now, ptime;

    spsc_queue_read_to(q, &ptime, sizeof(ptime));

    clock_gettime(CLOCK_MONOTONIC, &now);

    data[i] = 1000 * 1000 * 1000 * (now.tv_sec - ptime.tv_sec) + (now.tv_nsec - ptime.tv_nsec);
  }

  qsort(data, OPS, sizeof(size_t), cmp);

  printf("Reader done.\n");
  printf("min: %lu ns, median: %lu ns, max: %lu ns\n",
         data[0], data[OPS/2], data[OPS-1]);
}

int main(int argc, const char **argv)
{
  struct spsc_queue q;

  spsc_queue_init(&q);

  if (spsc_queue_alloc_anonymous(&q, SIZE))
  {
    printf("Creating spsc queue failed: %s\n", strerror(errno));
    return 1;
  }

  if (fork())
  {
    child(&q);
  }
  else
  {
    parent(&q);
  }

  spsc_queue_free(&q);

  return 0;
}
