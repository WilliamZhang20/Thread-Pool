#include <iostream>
#include "FastMatrix.h"
#include <chrono>

ThreadPool threads;

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
        auto x = threads.enqueue(std::bind(parallel_fib, n - 1), n - 1);
        int y = parallel_fib(n-2);
        int x_res = x.get();
        return x_res + y;
    }
}

int main() {
  	// Enqueueing works just like regular std::thread library, just different methods

    int n = 0;
    std::cin >> n;
    std::cout << parallel_fib(n) << std::endl;
    
    return 0;
}
