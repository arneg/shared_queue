#pragma once

#include "port.h"

#include <sys/mman.h>

static inline size_t shared_alloc_round_up(size_t bytes)
{
  return 4096 * (bytes + 4095) / 4096;
}

static inline void *shared_alloc_anonymous(size_t size)
{
  size = shared_alloc_round_up(size);
  return mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
}

static inline void *shared_alloc_free(void *ptr, size_t size)
{
  if (unlikely(ptr == MAP_FAILED))
    return ptr;

  size = shared_alloc_round_up(size);
  munmap(ptr, size);
  return MAP_FAILED;
}
