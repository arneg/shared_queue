#pragma once

#include <assert.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdint.h>

static inline int futex(int *uaddr, int futex_op, int val,
                        const struct timespec *timeout, int *uaddr2, int val3)
{
   return syscall(SYS_futex, uaddr, futex_op, val,
		  timeout, uaddr2, val3);
}

#ifdef __cplusplus
static inline void futex_wait(const std::atomic<unsigned int> *ptr, uint32_t val)
{
  int *futex_word = const_cast<int*>(reinterpret_cast<const int*>(ptr));

  int status = futex(futex_word, FUTEX_WAIT, (int)val, NULL, NULL, 0);

  (void)status;

  assert(-1 != status || errno == EAGAIN);
}

static inline int futex_wake(const std::atomic<unsigned int> *ptr, int waiters)
{
  int *futex_word = const_cast<int*>(reinterpret_cast<const int*>(ptr));
  int status = futex(futex_word, FUTEX_WAKE, waiters, NULL, NULL, 0);

  assert(-1 < status);

  return status;
}
#else
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
#endif
