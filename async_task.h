#pragma once
#pragma once
#include <coroutine>
#include <iostream>
#include <optional>

template<typename T>
struct AsyncTask {
    struct promise_type {
        T value;
        std::exception_ptr exception;

        AsyncTask get_return_object() {
            return AsyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T v) { value = v; }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> coro;

    AsyncTask(std::coroutine_handle<promise_type> h) : coro(h) {}
    AsyncTask(AsyncTask&& other) noexcept : coro(other.coro) { other.coro = nullptr; }
    ~AsyncTask() { if (coro) coro.destroy(); }

    T get() {
        if (coro.promise().exception) std::rethrow_exception(coro.promise().exception);
        return coro.promise().value;
    }

    bool is_ready() const { return coro.done(); }

    auto operator co_await() {
        struct Awaiter {
            std::coroutine_handle<promise_type> coro;

            bool await_ready() const noexcept { return coro.done(); }
            void await_suspend(std::coroutine_handle<> awaiting) noexcept { coro.resume(); }
            T await_resume() {
                if (coro.promise().exception) std::rethrow_exception(coro.promise().exception);
                return coro.promise().value;
            }
        };
        return Awaiter{coro};
    }
};