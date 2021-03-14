#pragma once

#ifdef __cplusplus
#include <atomic>

#define SQ_ATOMIC(T) std::atomic<T>

template <typename T>
static T sq_read_once(const std::atomic<T> &var)
{
  return var.load(std::memory_order_relaxed);
}

template <typename T>
static void sq_store_once(std::atomic<T> &var, T value)
{
  return var.store(value, std::memory_order_relaxed);
}

template <typename T>
static T sq_fetch_add_once(std::atomic<T> &var, T value)
{
  return var.fetch_add(value, std::memory_order_relaxed);
}

static void sq_thread_fence_release()
{
  std::atomic_thread_fence(std::memory_order_release);
}

static void sq_thread_fence_acquire()
{
  std::atomic_thread_fence(std::memory_order_acquire);
}
#else
#include <stdatomic.h>

#define SQ_ATOMIC(T) _Atomic T

#define sq_read_once(var)               atomic_load_explicit(&(var), memory_order_relaxed)
#define sq_store_once(var, value)       atomic_store_explicit(&(var), (value), memory_order_relaxed)
#define sq_fetch_add_once(var, value)   atomic_fetch_add_explicit(&(var), (value), memory_order_relaxed)

static void sq_thread_fence_release()
{
  atomic_thread_fence(memory_order_release);
}

static void sq_thread_fence_acquire()
{
  atomic_thread_fence(memory_order_acquire);
}
#endif
