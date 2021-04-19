#pragma once

#include "barriers.h"
#include "circular_area.h"
#include "futex.h"
#include "port.h"
#include "shared_alloc.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct spsc_header
{
  // Offset at which the next write will start.
  SQ_ATOMIC(uint32_t) write_offset;
  // When the writer is waiting for space to become available, this
  // variable will contain the number of bytes that the writer would
  // like to write.
  SQ_ATOMIC(size_t) write_size;
  // Offset at which the next read will start.
  SQ_ATOMIC(uint32_t) read_offset;
  // When the reader is waiting for space to become available, this
  // variable will contain the number of bytes that the reader would
  // like to read.
  SQ_ATOMIC(size_t) read_size;
};

struct spsc_queue
{
  struct spsc_header *header;
  struct circular_area area;
};

static inline void spsc_queue_init(struct spsc_queue *q)
{
  q->header = (struct spsc_header*)MAP_FAILED;
  circular_area_init(&q->area);
}

static inline void spsc_queue_free(struct spsc_queue *q)
{
  shared_alloc_free(q->header, sizeof(*q->header));
}

static inline size_t spsc_queue_capacity(const struct spsc_queue *q)
{
  return q->area.size;
}

static inline int spsc_queue_alloc_anonymous(struct spsc_queue *q, size_t size)
{
  struct spsc_header *header = (struct spsc_header*)shared_alloc_anonymous(sizeof(struct spsc_header));

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

static inline int spsc_queue_fdopen(struct spsc_queue *q, int fd)
{
  struct stat statbuf;
  int status = -1;
  off_t fsize;
  size_t size;
  size_t page_size = (size_t)getpagesize();
  struct spsc_header *header = (struct spsc_header*)MAP_FAILED;

  do
  {
    status = fstat(fd, &statbuf);

    if (status)
      break;

    fsize = statbuf.st_size;

    if ((size_t)fsize < page_size)
      return -1;

    size = fsize - page_size;

    header = (struct spsc_header*)shared_alloc_mmap(sizeof(struct spsc_header), fd, 0);

    if (unlikely(header == MAP_FAILED))
      break;

    status = circular_area_mmap(&q->area, size, fd, page_size);

    if (status)
      break;

    q->header = header;
    return 0;
  }
  while (0);

  shared_alloc_free(header, sizeof(struct spsc_header));

  return status;
}

static inline off_t spsc_queue_shm_size(size_t size) {
  size_t page_size = (size_t)getpagesize();

  return (off_t)(page_size + shared_alloc_round_up(size));
}

static inline void spsc_queue_wake_reader(struct spsc_queue *q)
{
  futex_wake(&q->header->write_offset, 1);
}

static inline void spsc_queue_wake_writer(struct spsc_queue *q)
{
  futex_wake(&q->header->read_offset, 1);
}

static inline size_t spsc_queue_read_size(const struct spsc_queue *q)
{
  uint32_t write_offset = sq_read_once(q->header->write_offset);
  uint32_t read_offset = sq_read_once(q->header->read_offset);

  return write_offset - read_offset;
}

static inline const void * spsc_queue_try_read(struct spsc_queue *q, size_t size)
{
  uint32_t read_offset = sq_read_once(q->header->read_offset);
  uint32_t write_offset = sq_read_once(q->header->write_offset);

  assert(size <= q->area.size);

  if (unlikely(write_offset - read_offset < size))
  {
    return NULL;
  }

  return circular_area_get_pointer(&q->area, read_offset);
}

static inline const void * spsc_queue_read(struct spsc_queue *q, size_t size)
{
  uint32_t read_offset = sq_read_once(q->header->read_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t write_offset = sq_read_once(q->header->write_offset);

    if (unlikely(write_offset - read_offset < size))
    {
      sq_store_once(q->header->read_size, size);
      futex_wait(&q->header->write_offset, write_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, read_offset);
  }
}

static inline void spsc_queue_wait_read(struct spsc_queue *q, size_t size)
{
  spsc_queue_read(q, size);
}

static inline void spsc_queue_read_commit(struct spsc_queue *q, size_t size)
{
  sq_thread_fence_release();
  uint32_t read_offset = sq_fetch_add_once(q->header->read_offset, (uint32_t)size);
  sq_thread_fence_acquire();
  uint32_t write_offset = sq_read_once(q->header->write_offset);
  size_t write_size = sq_read_once(q->header->write_size);

  if (unlikely(q->area.size < (write_offset - read_offset) + write_size))
  {
    spsc_queue_wake_writer(q);
  }
}

static inline void spsc_queue_read_to(struct spsc_queue *q, void *dst, size_t size)
{
  const void *src = spsc_queue_read(q, size);

  memcpy(dst, src, size);

  spsc_queue_read_commit(q, size);
}

static inline int spsc_queue_try_read_to(struct spsc_queue *q, void *dst, size_t size)
{
  const void *src = spsc_queue_try_read(q, size);

  if (src == NULL)
    return 0;

  memcpy(dst, src, size);

  spsc_queue_read_commit(q, size);

  return 1;
}

static inline size_t spsc_queue_write_size(const struct spsc_queue *q)
{
  uint32_t write_offset = sq_read_once(q->header->write_offset);
  uint32_t read_offset = sq_read_once(q->header->read_offset);

  return q->area.size - (write_offset - read_offset);
}

static inline void * spsc_queue_write(struct spsc_queue *q, size_t size)
{
  uint32_t write_offset = sq_read_once(q->header->write_offset);

  assert(size <= q->area.size);

  while (1)
  {
    uint32_t read_offset = sq_read_once(q->header->read_offset);

    if (unlikely(q->area.size < (write_offset - read_offset) + size))
    {
      sq_store_once(q->header->write_size, size);
      futex_wait(&q->header->read_offset, read_offset);
      continue;
    }

    return circular_area_get_pointer(&q->area, write_offset);
  }
}

static inline void spsc_queue_wait_write(struct spsc_queue *q, size_t size)
{
  spsc_queue_write(q, size);
}

static inline void * spsc_queue_try_write(struct spsc_queue *q, size_t size)
{
  uint32_t write_offset = sq_read_once(q->header->write_offset);

  assert(size <= q->area.size);

  uint32_t read_offset = sq_read_once(q->header->read_offset);

  if (unlikely(q->area.size < (write_offset - read_offset) + size))
  {
    return NULL;
  }

  return circular_area_get_pointer(&q->area, write_offset);
}

static inline void spsc_queue_write_commit(struct spsc_queue *q, size_t size)
{
  assert(size <= q->area.size);

  sq_thread_fence_release();
  uint32_t write_offset = sq_fetch_add_once(q->header->write_offset, (uint32_t)size);
  sq_thread_fence_acquire();
  uint32_t read_offset = sq_read_once(q->header->read_offset);
  size_t read_size = sq_read_once(q->header->read_size);

  if (unlikely(write_offset - read_offset < read_size))
  {
    // wake the reader
    spsc_queue_wake_reader(q);
  }
}

static inline void spsc_queue_write_from(struct spsc_queue *q, const void *src, size_t size)
{
  void *dst = spsc_queue_write(q, size);

  memcpy(dst, src, size);

  spsc_queue_write_commit(q, size);
}

static inline int spsc_queue_try_write_from(struct spsc_queue *q, const void *src, size_t size)
{
  void *dst = spsc_queue_write(q, size);

  if (dst == NULL)
    return 0;

  memcpy(dst, src, size);

  spsc_queue_write_commit(q, size);
  return 1;
}
