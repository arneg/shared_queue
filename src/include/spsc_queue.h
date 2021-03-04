#pragma once

#include "circular_area.h"
#include "shared_alloc.h"
#include "shared_counter.h"
#include "port.h"

#include <assert.h>
#include <string.h>

struct spsc_header
{
  // Offset at which the next write will start.
  struct shared_counter write_offset;
  // When the writer is waiting for space to become available, this
  // variable will contain the number of bytes that the writer would
  // like to write.
  size_t write_size;
  // Offset at which the next read will start.
  struct shared_counter read_offset;
  // When the reader is waiting for space to become available, this
  // variable will contain the number of bytes that the reader would
  // like to read.
  size_t read_size;
};

struct spsc_queue
{
  struct spsc_header *header;
  struct circular_area area;
};

static inline void spsc_queue_init(struct spsc_queue *q)
{
  q->header = MAP_FAILED;
  circular_area_init(&q->area);
}

static inline void spsc_queue_free(struct spsc_queue *q)
{
  shared_alloc_free(q->header, sizeof(*q->header));
}

static inline int spsc_queue_alloc_anonymous(struct spsc_queue *q, size_t size)
{
  struct spsc_header *header = shared_alloc_anonymous(sizeof(struct spsc_header));

  if (unlikely(header == MAP_FAILED))
    return -1;

  int status = circular_area_allocate_shared_anonymous(&q->area, size);

  if (unlikely(status))
  {
    shared_alloc_free(header, sizeof(*header));
  }
  else
  {
    q->header = header;
  }

  return status;
}

static inline const void * spsc_queue_read(struct spsc_queue *q, size_t size)
{
  uint32_t read_offset = shared_counter_read(&q->header->read_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t write_offset = shared_counter_read(&q->header->write_offset);

    if (unlikely(write_offset - read_offset < size))
    {
      atomic_store_explicit(&q->header->read_size, size, memory_order_relaxed);
      shared_counter_wait(&q->header->write_offset, write_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, read_offset);
  }
}

static inline void spsc_queue_read_commit(struct spsc_queue *q, size_t size)
{
  atomic_thread_fence(memory_order_release);
  uint32_t read_offset = shared_counter_inc(&q->header->read_offset, size);
  atomic_thread_fence(memory_order_acquire);
  uint32_t write_offset = shared_counter_read(&q->header->write_offset);
  size_t write_size = atomic_load_explicit(&q->header->write_size, memory_order_relaxed);

  if (unlikely(q->area.size < (write_offset - read_offset) + write_size))
  {
    shared_counter_wake(&q->header->read_offset);
  }
}

static inline void spsc_queue_read_to(struct spsc_queue *q, void *dst, size_t size)
{
  const void *src = spsc_queue_read(q, size);

  memcpy(dst, src, size);

  spsc_queue_read_commit(q, size);
}

static inline void * spsc_queue_write(struct spsc_queue *q, size_t size)
{
  uint32_t write_offset = shared_counter_read(&q->header->write_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t read_offset = shared_counter_read(&q->header->read_offset);

    if (unlikely(q->area.size < (write_offset - read_offset) + size))
    {
      atomic_store_explicit(&q->header->write_size, size, memory_order_relaxed);
      shared_counter_wait(&q->header->read_offset, read_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, write_offset);
  }
}

static inline void spsc_queue_write_commit(struct spsc_queue *q, size_t size)
{
  assert(size <= q->area.size);

  atomic_thread_fence(memory_order_release);
  uint32_t write_offset = shared_counter_inc(&q->header->write_offset, size);
  atomic_thread_fence(memory_order_acquire);
  uint32_t read_offset = shared_counter_read(&q->header->read_offset);
  size_t read_size = atomic_load_explicit(&q->header->read_size, memory_order_relaxed);

  if (unlikely(write_offset - read_offset < read_size))
  {
    // wake the reader
    shared_counter_wake(&q->header->write_offset);
  }
}

static inline void spsc_queue_write_from(struct spsc_queue *q, const void *src, size_t size)
{
  void *dst = spsc_queue_write(q, size);

  memcpy(dst, src, size);

  spsc_queue_write_commit(q, size);
}
