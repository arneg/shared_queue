#pragma once

#include "circular_area.h"
#include "shared_alloc.h"
#include "shared_counter.h"

#include <assert.h>
#include <string.h>

struct spsc_header
{
  struct shared_counter write_offset;
  struct shared_counter read_offset;
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

  if (header == MAP_FAILED)
    return -1;

  int status = circular_area_allocate_shared_anonymous(&q->area, size);

  if (status)
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
  uint32_t read_offset = shared_counter_load(&q->header->read_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t write_offset = shared_counter_read(&q->header->write_offset);

    if (write_offset - read_offset < size)
    {
      shared_counter_wait(&q->header->write_offset, write_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, read_offset);
  }
}

static inline void spsc_queue_read_commit(struct spsc_queue *q, size_t size)
{
  uint32_t read_offset = shared_counter_load(&q->header->read_offset);

  shared_counter_write(&q->header->read_offset, read_offset + size);

  uint32_t write_offset = shared_counter_read(&q->header->write_offset);

  // FIXME: this check is only valid if both reader and writer agree on a size here.
  if (q->area.size < (write_offset - read_offset) + size)
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
  uint32_t write_offset = shared_counter_load(&q->header->write_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t read_offset = shared_counter_read(&q->header->read_offset);

    if (q->area.size < (write_offset - read_offset) + size)
    {
      shared_counter_wait(&q->header->read_offset, read_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, write_offset);
  }
}

static inline void spsc_queue_write_commit(struct spsc_queue *q, size_t size)
{
  assert(size <= q->area.size);

  uint32_t write_offset = shared_counter_load(&q->header->write_offset);

  shared_counter_write(&q->header->write_offset, write_offset + size);

  uint32_t read_offset = shared_counter_read(&q->header->read_offset);

  // FIXME: this check is only valid if both reader and writer agree on a size here.
  if (write_offset - read_offset < size)
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
