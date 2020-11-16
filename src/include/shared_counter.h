#pragma once

#include <errno.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include "futex.h"


static inline void futex_wait(const atomic_uint *ptr, uint32_t val)
{
  int status = futex((int*)ptr, FUTEX_WAIT, (int)val, NULL, NULL, 0);

  (void)status;

  assert(-1 != status || errno == EAGAIN);
}

static inline int futex_wake(const atomic_uint *ptr, int waiters)
{
  int status = futex((int*)ptr, FUTEX_WAKE, waiters, NULL, NULL, 0);

  assert(-1 < status);

  return status;
}

struct shared_counter
{
  atomic_uint n __attribute__((aligned (64)));
};

static inline uint32_t shared_counter_read(const struct shared_counter *counter)
{
  return atomic_load_explicit(&counter->n, memory_order_relaxed);
}

static inline uint32_t shared_counter_inc(struct shared_counter *counter, uint32_t value)
{
  return atomic_fetch_add_explicit(&counter->n, value, memory_order_relaxed);
}

static inline void shared_counter_wait(const struct shared_counter *counter, uint32_t current_value)
{
  futex_wait(&counter->n, current_value);
}

static inline int shared_counter_wake(const struct shared_counter *counter)
{
  return futex_wake(&counter->n, 1);
}
