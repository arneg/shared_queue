#include <spsc_queue.hpp>

#include <cassert>
#include <thread>

struct message {
  float foo[127];
};

void writer(spsc::queue &q)
{
  message m;

  assert(q.empty());

  for (int i = 0; i < 1000 * 1000; i++) {
    q.write(m); 
  }

  for (int i = 0; i < 1000 * 1000; i++) {
    q.write_with(sizeof(m), [=](void *dst) {
      memcpy(dst, &m, sizeof(m));
    }); 
  }

}

void reader(spsc::queue &q)
{
  for (int i = 0; i < 1000 * 1000; i++) {
    auto m = q.read<message>();
  }

  assert(q.empty());

  for (int i = 0; i < 1000 * 1000; i++) {
    q.read_with(sizeof(message), [=](const void *src) {
      message tmp;
      memcpy(&tmp, src, sizeof(message));
    });
  }

  assert(q.empty());
}

int main(int argc, const char **argv)
{
  spsc::queue q(4 * 1024 * 1024);   

  std::thread t1(writer, std::ref(q));
  std::thread t2(reader, std::ref(q));

  t1.join();
  t2.join();

  return 0;
}
