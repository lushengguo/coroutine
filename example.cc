#include "coroutine.h"
#include <iostream>
#include <unistd.h>
using std::cout;
using std::endl;
using namespace co;

int main() {
  Scheduler scheduler;
  auto id1 = scheduler.create([](Coroutine *co) {
    for (;;) {
      std::cout << "hello world " << co->id() << std::endl;
      co->yield();
    }
    return nullptr;
  });
  auto id2 = scheduler.create([](Coroutine *co) {
    for (;;) {
      std::cout << "hello world " << co->id() << std::endl;
      co->yield();
    }
    return nullptr;
  });
  auto id3 = scheduler.create([](Coroutine *co) {
    for (;;) {
      std::cout << "hello world " << co->id() << std::endl;
      co->yield();
    }
    return nullptr;
  });
  for (int i = 0; i < 3; i++) {
    scheduler.resume(id1);
    scheduler.resume(id2);
    scheduler.resume(id3);
  }
  cout << "coroutine quit" << endl;
}