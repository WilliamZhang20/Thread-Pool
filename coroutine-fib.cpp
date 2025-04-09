#include "ThreadPool.h"
#include "async_task.h"
#include <iostream>

AsyncTask<int> fib(int n) {
    if (n <= 1)
        co_return n;

    auto fut1 = fib(n - 1);
    auto fut2 = fib(n - 2);

    int a = co_await fut1;
    int b = co_await fut2;

    co_return a + b;
}

int main() {
    auto task = fib(10);
    std::cout << "fib(10) = " << task.get() << "\n";
}