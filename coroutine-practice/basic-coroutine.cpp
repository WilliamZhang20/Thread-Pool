#include <coroutine>
#include <iostream>

class BasicCoroutine {
public:
    struct Promise {
        BasicCoroutine get_return_object() { 
            std::cout << "get_return_object called\n";
            return BasicCoroutine {}; }

        void unhandled_exception() noexcept { }

        void return_void() noexcept { 
            std::cout << "return void called\n";
        }

        std::suspend_never initial_suspend() noexcept { 
            std::cout << "initial_suspend called\n";
            return {}; }
        std::suspend_never final_suspend() noexcept { 
            std::cout << "final_suspend called\n";
            return {}; }
    };
    using promise_type = Promise;
};

BasicCoroutine coro()
{
    co_return;
}

int main()
{
    coro();
}