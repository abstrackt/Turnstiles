#ifndef SRC_TURNSTILE_H_
#define SRC_TURNSTILE_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>

class Mutex;

namespace util {
class Turnstile {
 public:
  std::mutex m;
  std::condition_variable cv;
  size_t on_hold;
  bool ready;
  Turnstile() {
    on_hold = 0;
    ready = false;
  }

  ~Turnstile() = default;
};

}

class Mutex {
 private:
  util::Turnstile *turnstile;

 public:
  Mutex();
  Mutex(const Mutex&) = delete;

  void lock();    // NOLINT
  void unlock();  // NOLINT
};

#endif // SRC_TURNSTILE_H_
