#include <concepts>
#include <coroutine>
#include <exception>
#include <iostream>

struct ReturnObject {
  struct promise_type { // return object of coroutine...
    ReturnObject get_return_object() { 
        std::cout << "get_return_object called\n";
        return {}; }
    std::suspend_never initial_suspend() { 
        std::cout << "initial_suspend called\n";
        return {}; }
    std::suspend_never final_suspend() noexcept { 
        std::cout << "final_suspend called\n";
        return {}; }
    void unhandled_exception() {
        std::cout << "unhandled_exception called\n";
    }
  };
};

struct Awaiter {
  std::coroutine_handle<> *hp_;
  constexpr bool await_ready() const noexcept { 
    return false; }
  void await_suspend(std::coroutine_handle<> h) { 
    std::cout << "await_suspend called\n"; // called after co_await
    *hp_ = h; }
  constexpr void await_resume() const noexcept {
  }
};

ReturnObject counter(std::coroutine_handle<> *continuation_out)
{
  Awaiter a{continuation_out};
  for (unsigned i = 0;; ++i) {
    std::cout << "before wait\n";
    co_await a;
    std::cout << "counter: " << i << std::endl;
  }
}

int main()
{
  std::coroutine_handle<> h;
  counter(&h);
  for (int i = 0; i < 3; ++i) {
    std::cout << "In main1 function\n";
    h(); // calls coroutine handle to resume the attached function
  }
  h.destroy();
}