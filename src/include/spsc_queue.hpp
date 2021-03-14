#include <new>

#include "spsc_queue.h"

namespace spsc
{
  class queue
  {
    struct spsc_queue q;
  public:
    queue(size_t size)
    {
      spsc_queue_init(&q);

      if (spsc_queue_alloc_anonymous(&q, size))
      {
        throw std::bad_alloc();
      }
    }

    size_t write_size() const
    {
      return spsc_queue_write_size(&q);
    }

    size_t read_size() const
    {
      return spsc_queue_read_size(&q);
    }

    size_t size() const
    {
      return read_size();
    }

    size_t capacity() const
    {
      return spsc_queue_capacity(&q);
    }

    bool empty() const
    {
      return size() == 0;
    }

    void write(const void *src, size_t bytes) {
      spsc_queue_write_from(&q, src, bytes);
    }

    bool try_write(const void *src, size_t bytes) {
      return spsc_queue_try_write_from(&q, src, bytes);
    }

    template <typename Func>
    void write_with(size_t bytes, Func &&f)
    {
      void *dst = spsc_queue_write(&q, bytes);

      f(dst);

      spsc_queue_write_commit(&q, bytes);
    }

    template <typename Func>
    bool try_write_with(size_t bytes, Func &&f)
    {
      void *dst = spsc_queue_try_write(&q, bytes);

      if (dst == nullptr)
        return false;

      f(dst);

      spsc_queue_write_commit(&q, bytes);

      return true;
    }

    template <typename T>
    typename std::enable_if<std::is_trivially_copyable<T>::value, void>::type
    write(const T &value) {
      write(reinterpret_cast<const void*>(&value), sizeof(T));
    }

    template <typename T>
    typename std::enable_if<std::is_trivially_copyable<T>::value, bool>::type
    try_write(const T &value) {
      return try_write(reinterpret_cast<const void*>(&value), sizeof(T));
    }

    void read(void *dst, size_t bytes) {
      spsc_queue_read_to(&q, dst, bytes);
    }

    bool try_read(void *dst, size_t bytes) {
      return spsc_queue_try_read_to(&q, dst, bytes);
    }

    template <typename T>
    typename std::enable_if<std::is_trivially_copyable<T>::value, T>::type
    read() {
      T tmp;
      read(reinterpret_cast<void*>(&tmp), sizeof(T));
      return std::move(tmp);
    }

    template <typename T>
    typename std::enable_if<std::is_trivially_copyable<T>::value, bool>::type
    try_read(T &dst)
    {
      return try_read(&dst, sizeof(T));
    }

    template <typename Func>
    void read_with(size_t bytes, Func &&f)
    {
      const void *dst = spsc_queue_read(&q, bytes);

      f(dst);

      spsc_queue_read_commit(&q, bytes);
    }

    template <typename Func>
    bool try_read_with(size_t bytes, Func &&f)
    {
      void *dst = spsc_queue_try_read(&q, bytes);

      if (dst == nullptr)
        return false;

      f(dst);

      spsc_queue_read_commit(&q, bytes);

      return true;
    }

    ~queue()
    {
      spsc_queue_free(&q);
    }
  };
}
