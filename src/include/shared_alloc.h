#pragma once

#include "port.h"
#include <unistd.h>

#include <sys/mman.h>

static inline size_t shared_alloc_round_up(size_t bytes)
{
  size_t page_size = getpagesize();
  return page_size * ((bytes + (page_size - 1)) / page_size);
}

static inline void *shared_alloc_mmap(size_t size, int fd, size_t offset)
{
  size = shared_alloc_round_up(size);
  int flags = fd == -1 ? MAP_SHARED|MAP_ANONYMOUS : MAP_SHARED;
  return mmap(NULL, size, PROT_READ|PROT_WRITE, flags, fd, 0);
}

static inline void *shared_alloc_anonymous(size_t size)
{
  return shared_alloc_mmap(size, -1, 0);
}

static inline void *shared_alloc_free(void *ptr, size_t size)
{
  if (unlikely(ptr == MAP_FAILED))
    return ptr;

  size = shared_alloc_round_up(size);
  munmap(ptr, size);
  return MAP_FAILED;
}
