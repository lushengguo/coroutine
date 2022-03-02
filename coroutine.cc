#include "coroutine.h"
#include <assert.h>
#include <iostream>
#include <string.h>
using std::cout;
using std::endl;

namespace co {
using RunnerSignature = Coroutine::RunnerSignature;
enum class State { kReady, kRunning, kSuspend, kDead };
using CoroutineId = Coroutine::CoroutineId;

class SchedulerImpl {
public:
  SchedulerImpl(Scheduler *s)
      : shared_stack_(SHARED_STACK_SIZE, 0), outer_wrapper_(s) {}

  CoroutineId create(RunnerSignature func);

  void resume(CoroutineId id);

  void yield();

public:
  void erase(CoroutineId id) {
    if (cos_.count(id) == 0)
      return;
    delete cos_.at(id);
    cos_.erase(id);
  }

  void *ret(CoroutineId id) {
    if (cos_.count(id) == 0)
      return nullptr;
    return cos_.at(id)->ret_;
  }

private:
  CoroutineId generate_id() const;

private:
  struct CoroutineContext {
  public:
    CoroutineContext(SchedulerImpl *p, CoroutineId id, RunnerSignature func)
        : state_(State::kReady), p_(p), id_(id), func_(func), ret_(nullptr) {}

    State state_;
    ucontext_t context_;
    SchedulerImpl *p_;
    std::vector<char> stack_;
    CoroutineId id_;
    RunnerSignature func_;
    void *ret_;
  };

  static constexpr size_t SHARED_STACK_SIZE = 1024 * 1024;

  CoroutineId running_;
  ucontext_t main_;
  std::map<CoroutineId, CoroutineContext *> cos_;
  std::vector<char> shared_stack_;
  Scheduler *outer_wrapper_;
};

CoroutineId SchedulerImpl::generate_id() const {
  CoroutineId id = (cos_.empty() ? 1 : cos_.crbegin()->first + 1);
  return id;
}

CoroutineId SchedulerImpl::create(RunnerSignature func) {
  CoroutineId id = generate_id();
  cos_[id] = new CoroutineContext(this, id, func);
  return id;
}

void SchedulerImpl::yield() {
  char dummy;
  size_t stack_size = shared_stack_.data() + shared_stack_.size() - &dummy;
  CoroutineContext *co = cos_.at(running_);
  running_ = 0;

  if (stack_size > co->stack_.size()) {
    co->stack_.resize(stack_size);
  }
  co->state_ = State::kSuspend;
  memcpy(co->stack_.data(), &dummy, stack_size);
  swapcontext(&co->context_, &main_);
}

void SchedulerImpl::resume(CoroutineId id) {
  if (cos_.count(id) != 1)
    return;

  CoroutineContext *co = cos_.at(id);
  switch (co->state_) {
  case State::kReady: {
    void (*func)(SchedulerImpl *) = [](SchedulerImpl *s) {
      CoroutineContext *co = s->cos_.at(s->running_);
      co->ret_ = co->func_(new Coroutine(s->outer_wrapper_, co->id_));
      co->state_ = State::kDead;
      swapcontext(&co->context_, &s->main_);
    };
    int ret = getcontext(&co->context_);
    if (ret) {
      perror("getcontext");
    }
    co->context_.uc_stack.ss_sp = shared_stack_.data();
    co->context_.uc_stack.ss_size = shared_stack_.size();
    co->context_.uc_link = &main_;
    co->state_ = State::kRunning;
    running_ = id;
    makecontext(&co->context_, (void (*)())func, 1, this);
    swapcontext(&main_, &co->context_);
    if (ret) {
      perror("swapcontext");
    }
    return;
  }
  case State::kSuspend:
    memcpy(shared_stack_.data() + SHARED_STACK_SIZE - co->stack_.size(),
           co->stack_.data(), co->stack_.size());
    running_ = id;
    co->state_ = State::kRunning;
    swapcontext(&main_, &co->context_);
    break;
  case State::kRunning:
  case State::kDead:
  default:
    return;
  }
}

Scheduler::Scheduler() : wrapper_(new SchedulerImpl(this)) {}

Scheduler::CoroutineId Scheduler::create(RunnerSignature func) {
  return wrapper_->create(func);
}

void Scheduler::resume(CoroutineId id) { wrapper_->resume(id); }

void Coroutine::destroy() {
  p_->wrapper_->erase(id_);
  delete this;
}

void Coroutine::yield() { return p_->wrapper_->yield(); }

void *Coroutine::ret() { return p_->wrapper_->ret(id_); }
} // namespace co