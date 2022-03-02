#pragma once
#include <array>
#include <functional>
#include <map>
#include <ucontext.h>
#include <vector>

namespace co {
class Scheduler;
class CoroutineImpl;
class SchedulerImpl;

class Coroutine {
public:
  typedef size_t CoroutineId;
  typedef std::function<void *(Coroutine *)> RunnerSignature;

  void destroy();

  void yield();

  CoroutineId id() const { return id_; }

  void *ret();

private:
  friend class SchedulerImpl;
  Coroutine(Scheduler *s, CoroutineId id) : p_(s), id_(id) {}
  ~Coroutine() = default;

  CoroutineId id_;
  Scheduler *p_;
};

class Scheduler {
public:
  using CoroutineId = Coroutine::CoroutineId;

public:
  Scheduler();

  CoroutineId create(Coroutine::RunnerSignature func);
  void resume(CoroutineId id);

private:
  friend class Coroutine;
  SchedulerImpl *wrapper_;
};

} // namespace co