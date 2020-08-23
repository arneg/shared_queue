#pragma once

#include <stdatomic.h>
#include <stdint.h>

#include "futex.h"

static inline void futex_wait(const uint32_t *ptr, uint32_t val)
{
  futex((int*)ptr, FUTEX_WAIT, (int)val, NULL, NULL, 0);
}

static inline void futex_wake(const uint32_t *ptr)
{
  futex((int*)ptr, FUTEX_WAKE, 1, NULL, NULL, 0);
}

struct shared_counter
{
  uint32_t n __attribute__((aligned (64)));
};

static inline uint32_t shared_counter_read(const struct shared_counter *counter)
{
  return atomic_load_explicit(&counter->n, memory_order_acquire);
}

static inline void shared_counter_write(struct shared_counter *counter, uint32_t value)
{
  atomic_store_explicit(&counter->n, value, memory_order_release);
}

static inline void shared_counter_wait(const struct shared_counter *counter, uint32_t current_value)
{
  futex_wait(&counter->n, current_value);
}

static inline void shared_counter_wake(const struct shared_counter *counter)
{
  futex_wake(&counter->n);
}
