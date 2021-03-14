#pragma once

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include "port.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

struct circular_area
{
  size_t size;
  void *base;
};

static inline void circular_area_init(struct circular_area *area)
{
  area->base = MAP_FAILED;
}

static inline int circular_area_free(struct circular_area *area)
{
  if (unlikely(area->base == MAP_FAILED))
    return 0;
  int status = munmap(area->base, area->size * 2);
  if (unlikely(!status))
    area->base = MAP_FAILED;
  return status;
}

static inline int circular_area_allocate_shared(struct circular_area *area, size_t size, char *filename_template)
{
  void *a = MAP_FAILED;
  void *b = MAP_FAILED;
  int status = -1;
  int fd = -1;

  do
  {
    // we require power of two size
    if (unlikely(size & (size - 1)))
      break;

    fd = mkstemp(filename_template);

    if (unlikely(fd < 0))
      break;

    if (unlikely(ftruncate(fd, size)))
      break;

    a = mmap(NULL, 2 * size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (unlikely(a == MAP_FAILED))
      break;

    {
      void *tmp = mremap(a, 2 * size, size, 0);

      if (unlikely(tmp == MAP_FAILED))
        break;

      a = tmp;
    }

    b = mmap((char *)a + size, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (unlikely((char*)b != (char*)a + size))
      break;

    close(fd);

    area->base = a;
    area->size = size;
    return 0;
  }
  while (0);

  // failure
  if (a != MAP_FAILED)
    munmap(a, size);
  if (b != MAP_FAILED)
    munmap(b, size);
  if (fd >= 0)
  {
    close(fd);
    unlink(filename_template);
  }
  return status;
}

static inline int circular_area_allocate_shared_anonymous(struct circular_area *area, size_t size)
{
  char filename_template[] = "/dev/shm/ring-buffer-XXXXXX";

  int status = circular_area_allocate_shared(area, size, filename_template);

  if (unlikely(!status))
  {
    if (unlink(filename_template))
    {
      // what do we do with this failure?
    }
  }

  return status;
}

static inline void * circular_area_get_pointer(struct circular_area *area, size_t offset)
{
  return (char*)area->base + (offset & (area->size - 1));
}
