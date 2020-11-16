#include "bandwidth.h"

#include <pthread.h>

int main(int argc, const char **argv)
{
  struct spsc_queue q;

  spsc_queue_init(&q);

  if (spsc_queue_alloc_anonymous(&q, SIZE))
  {
    printf("Creating spsc queue failed: %s\n", strerror(errno));
    return 1;
  }

  pthread_t t1;
  pthread_t t2;

  pthread_create(&t1, NULL, (void*)receiver, &q);
  pthread_create(&t2, NULL, (void*)writer, &q);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  spsc_queue_free(&q);

  return 0;
}
