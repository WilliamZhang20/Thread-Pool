#pragma once
#include <coroutine> // must compile with g++ -fcoroutines
#include <iostream>
#include <optional>
#include "ThreadPool.h" // for submitting tasks to thread pool

ThreadPool global_pool(std::thread::hardware_concurrency()); // only 1 pool should manage all tasks - so it is global

template<typename T>
struct AsyncTask {
    struct promise_type {
        T value;
        std::exception_ptr exception;

        AsyncTask get_return_object() {
            return AsyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // coroutine does not suspend at start, but always at end
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; } 

        void return_value(T v) { value = v; }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> coro;

    AsyncTask(std::coroutine_handle<promise_type> h) : coro(h) {}
    AsyncTask(AsyncTask&& other) noexcept : coro(other.coro) { other.coro = nullptr; }
    ~AsyncTask() { if (coro) coro.destroy(); }

    T get() { // called at waiting for completion of the coroutine
        while (!coro.done()) {
            // yield CPU until ready
            std::this_thread::yield();
        }
        if (coro.promise().exception) std::rethrow_exception(coro.promise().exception);
        return coro.promise().value;
    }

    bool is_ready() const { return coro.done(); }

    // Custom overload of co_await operator when the 
    auto operator co_await() {
        struct Awaiter {
            std::coroutine_handle<promise_type> coro;

            bool await_ready() const noexcept { return coro.done(); }

            void await_suspend(std::coroutine_handle<> awaiting) { // if coroutine is not done push to thread pool for later computation
                global_pool.enqueue([coro = this->coro, awaiting]() mutable {
                    // resume the awaiting coroutine
                    coro.resume();
                    awaiting.resume();
                });
            }

            T await_resume() { // if coroutine is done return the computation result of the promise
                if (coro.promise().exception) std::rethrow_exception(coro.promise().exception);
                return coro.promise().value;
            }
        };
        return Awaiter{coro};
    }
};
