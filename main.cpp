#include <iostream>
#include "FastMatrix.h"

long long fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    ThreadPool pool(std::thread::hardware_concurrency()); // Use available cores
    int n = 40; // Fibonacci number to calculate
    std::future<long long> result = pool.enqueue(fibonacci, n);

    std::cout << "Fibonacci(" << n << ") = " << result.get() << std::endl;

    //Example of multiple fibonacci calculations.
    std::vector<std::future<long long>> results;
    for(int i = 30; i < 40; ++i){
        results.push_back(pool.enqueue(fibonacci, i));
    }
    for(int i = 0; i < results.size(); ++i){
        std::cout << "Fibonacci("<<i+30<<") = " << results[i].get() << std::endl;
    }
    return 0;
}