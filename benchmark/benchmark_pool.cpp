#include <iostream>
#include <chrono>
#include "../ThreadPool.h"

int main() {
    constexpr size_t NUM_TASKS = 1'000'000; // 1M tasks
    ThreadPool pool; // uses hardware_concurrency threads

    std::vector<std::future<int>> results;
    results.reserve(NUM_TASKS);

    auto start = std::chrono::high_resolution_clock::now();

    // Submit lots of trivial tasks
    for (size_t i = 0; i < NUM_TASKS; ++i) {
        results.push_back(pool.submit([i]() -> int {
            return static_cast<int>(i * i);
        }));
    }

    // Wait for completion
    for (auto& f : results) {
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Processed " << NUM_TASKS 
              << " tasks in " << diff.count() << " seconds\n";
    std::cout << "Throughput: " 
              << static_cast<double>(NUM_TASKS) / diff.count() / 1e6 
              << " M tasks/sec\n";

    pool.stop();
    return 0;
}
