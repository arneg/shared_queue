
int main(int argc, const char **argv)
{
  struct spsc_queue q;

  spsc_queue_init(&q);

  if (spsc_queue_alloc_anonymous(&q, SIZE))
  {
    printf("Creating spsc queue failed: %s\n", strerror(errno));
    return 1;
  }

  if (fork())
  {
    reader(&q);
  }
  else
  {
    writer(&q);
  }

  spsc_queue_free(&q);

  return 0;
}
