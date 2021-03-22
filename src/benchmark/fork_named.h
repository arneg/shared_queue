#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

static int create_named_shm(const char *name, size_t size)
{
  int fd;

  fd = shm_open(name, O_RDWR|O_CREAT|O_EXCL, 0644);

  if (fd == -1)
  {
    printf("Creating shm object failed: %s\n", strerror(errno));
    return 1;
  }

  if (ftruncate(fd, spsc_queue_shm_size(size)))
  {
    printf("Setting size of shm object failed: %s\n", strerror(errno));
    return 1;
  }

  printf("Created shared memory object of size %llu (queue size %llu).\n",
         (unsigned long long)spsc_queue_shm_size(size), (unsigned long long)size);

  close(fd);

  return 0;
}

int main(int argc, const char **argv)
{
  const char *name = "test-shared-queue";
  struct spsc_queue q;
  int fd, is_reader, status = -1;

  if (create_named_shm(name, SIZE))
    return 1;

  do
  {
    is_reader = fork();

    fd = shm_open(name, O_RDWR, 0644);

    if (fd == -1)
    {
      printf("Failed to open shm object %s\n", name);
      break;
    }

    spsc_queue_init(&q);

    if (status = spsc_queue_fdopen(&q, fd))
    {
      printf("Creating spsc queue failed: %s\n", strerror(errno));
      break;
    }

    close(fd);

    if (is_reader)
    {
      reader(&q);
    }
    else
    {
      writer(&q);
    }
  } while (0);

  spsc_queue_free(&q);

  if (is_reader)
  {
    shm_unlink(name);
  }

  return status;
}
