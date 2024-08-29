#include <iostream>
#include "FastMatrix.h"
#include <chrono>

ThreadPool threads;
std::mutex m;

void myFunction(void) {
    std::cout << "Just started\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "It's done\n";
}

int parallel_fib(int n) {
    if(n <= 1) {
        return n;
    }
    else {
        auto x = threads.enqueue(parallel_fib, n-1);
        int y = parallel_fib(n-2);
        int x_res = x.get();
        m.lock();
        std::cout << "finished for " << n << std::endl;
        m.unlock();
        return x_res + y;
    }
}

int main() {
  	// Enqueueing works just like regular std::thread library, just different methods

    int n = 0;
    std::cin >> n;
    auto x = threads.enqueue(parallel_fib, n);
    std::cout << x.get() << std::endl;
    
    return 0;
}
