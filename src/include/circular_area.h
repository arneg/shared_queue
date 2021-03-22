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

static inline int circular_area_mmap(struct circular_area *area, size_t size, int fd, size_t offset)
{
  void *a = MAP_FAILED;
  void *b = MAP_FAILED;
  int status = -1;

  int flags = fd == -1 ? MAP_SHARED|MAP_ANONYMOUS : MAP_SHARED;

  do
  {
    // we require power of two size
    if (unlikely(size & (size - 1)))
      break;

    a = mmap(NULL, 2 * size, PROT_READ|PROT_WRITE, flags, fd, offset);

    if (unlikely(a == MAP_FAILED))
      break;

    {
      void *tmp = mremap(a, 2 * size, size, 0);

      if (unlikely(tmp == MAP_FAILED))
        break;

      a = tmp;
    }

    b = mmap((char *)a + size, size, PROT_READ|PROT_WRITE, flags, fd, offset);

    if (unlikely((char*)b != (char*)a + size))
      break;

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
  return status;
}

static inline int circular_area_allocate_shared(struct circular_area *area, size_t size, char *filename_template)
{
  int status = -1;
  int fd = -1;

  do
  {
    fd = mkstemp(filename_template);

    if (unlikely(fd < 0))
      break;

    if (unlikely(ftruncate(fd, size)))
      break;

    status = circular_area_mmap(area, size, fd, 0);

    close(fd);

    return 0;
  }
  while (0);

  if (fd >= 0)
  {
    close(fd);
    unlink(filename_template);
  }
  return status;
}

static inline int circular_area_allocate_shared_anonymous(struct circular_area *area, size_t size)
{
  return circular_area_mmap(area, size, -1, 0);
}

static inline void * circular_area_get_pointer(struct circular_area *area, size_t offset)
{
  return (char*)area->base + (offset & (area->size - 1));
}
