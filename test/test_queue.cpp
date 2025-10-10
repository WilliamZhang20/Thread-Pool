#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <cassert>
#include "../LockFreeQueue.h"

// --- Benchmark Harness ---
int main() {
    constexpr size_t capacity = 1 << 12; // power-of-two ring size
    constexpr size_t iterations = 10'000'000; // ops to measure
    LockFreeQueue<int> q(capacity);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]() {
        for (size_t i = 0; i < iterations; i++) {
            while (!q.enqueue(static_cast<int>(i))) {
                // busy spin if full
            }
        }
    });

    std::thread consumer([&]() {
        int value;
        for (size_t i = 0; i < iterations; i++) {
            while (!q.dequeue(value)) {
                // busy spin if empty
            }
            // sanity check
            assert(value == static_cast<int>(i));
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end - start).count();

    double throughput = iterations / duration;
    double latency_ns = (duration * 1e9) / iterations;

    std::cout << "Total ops: " << iterations << "\n";
    std::cout << "Elapsed: " << duration << " s\n";
    std::cout << "Throughput: " << throughput << " ops/sec\n";
    std::cout << "Latency: " << latency_ns << " ns/op\n";
}
