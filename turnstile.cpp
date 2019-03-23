#include <condition_variable>
#include <mutex>
#include <queue>
#include "turnstile.h"

// Using construct on first use idiom to avoid possible SIOF errors.

std::queue<util::Turnstile *> &t_queue() {
  static auto *q = new std::queue<util::Turnstile *>();
  return *q;
}

// Creating mutexes and a dummy Turnstile to represent a Mutex which
// holds a single process in the critical section with no other processes
// waiting on the Mutex - this state does not require taking a real turnstile
// from the pool.

size_t in_use = 0;

util::Turnstile *dummy = reinterpret_cast<util::Turnstile *>('f');

// Internal guards restrict access to a turnstile's pointer.

std::mutex guard;
std::mutex internal_guards[257];

util::Turnstile *takeTurnstile() {
  guard.lock();

  if (t_queue().empty()) {
    unsigned long long added;
    if (in_use == 0) added = 16;
    else added = in_use;
    for (unsigned long long i = 0; i < added; i++) {
      auto *t = new util::Turnstile();
      t_queue().push(t);
    }
  }

  util::Turnstile *current = t_queue().front();
  t_queue().pop();
  in_use++;
  guard.unlock();
  return current;
}

void returnTurnstile(util::Turnstile *t) {
  guard.lock();

  if (t_queue().size() > 16 && t_queue().size() > 3 * in_use) {
    size_t overall = t_queue().size();
    for (size_t i = 0; i < overall / 2; i++) {
      delete (t_queue().front());
      t_queue().pop();
    }
  }

  t_queue().push(t);
  in_use--;
  guard.unlock();
}

Mutex::Mutex() {
  turnstile = nullptr;
}

std::mutex &getGuard(Mutex *ptr) {
  std::hash<Mutex *> hash_fnc;
  size_t hash = hash_fnc(ptr) % 257;

  return internal_guards[hash];
}

void Mutex::lock() {
  std::unique_lock<std::mutex> internal_guard(getGuard(this));

  if (turnstile == nullptr) {
    turnstile = dummy;
  } else {
    if (turnstile == dummy) {
      turnstile = takeTurnstile();
    }

    turnstile->on_hold++;

    internal_guard.unlock();

    std::unique_lock<std::mutex> lk(turnstile->m);
    turnstile->cv.wait(lk, [&] { return turnstile->ready; });

    internal_guard.lock();

    turnstile->ready = false;
    turnstile->on_hold--;

    if (turnstile->on_hold == 0) {
      returnTurnstile(turnstile);
      turnstile = dummy;
    }
  }

  internal_guard.unlock();
}

void Mutex::unlock() {
  std::unique_lock<std::mutex> internal_guard(getGuard(this));
  if (turnstile == dummy) {
    turnstile = nullptr;
  } else {
    std::unique_lock<std::mutex> lk(turnstile->m);
    turnstile->ready = true;
    turnstile->cv.notify_one();
  }
}
