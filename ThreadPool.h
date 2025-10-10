#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <future>
#include "LockFreeQueue.h"  // Include your LockFreeQueue class header

class ThreadPool {
public:
    using Task = std::function<void()>;  // Define Task type as a callable

    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency(),
                        size_t queueCapacity = 1024)
        : stopFlag(false) 
    {
        if (numThreads == 0) {
            numThreads = 1;  // Default to one thread if hardware_concurrency() returns 0
        }

        if ((queueCapacity & (queueCapacity - 1)) != 0) {
            throw std::runtime_error("Queue capacity must be a power of 2");
        }

        // Create an SPSC queue for each worker thread
        queues.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            queues.emplace_back(queueCapacity); // SPSC queues
        }

        // Create and start the worker threads
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i]() { this->workerLoop(i); });
        }
    }

    ~ThreadPool() {
        stop();
    }

    // Submit a task to the thread pool: round-robin across queues
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();

        // Round-robin index for assigning tasks to queues
        size_t idx = rrIndex.fetch_add(1, std::memory_order_relaxed) % queues.size();

        // Enqueue the task to the selected worker's queue
        while (!queues[idx].enqueue([task]() { (*task)(); })) {
            std::this_thread::yield();  // If the queue is full, yield the current thread
        }

        return res;
    }

    // Stop the thread pool, wait for all threads to finish
    void stop() {
        bool expected = false;
        if (!stopFlag.compare_exchange_strong(expected, true)) {
            return;  // Already stopped
        }

        for (auto& t : workers) {
            if (t.joinable()) t.join();  // Join all worker threads
        }
    }

private:
    std::vector<std::thread> workers;  // Worker threads
    std::vector<LockFreeQueue<Task>> queues;  // SPSC queues for each worker
    std::atomic<bool> stopFlag;  // Flag to stop the pool
    std::atomic<size_t> rrIndex{0};  // Round-robin index for task distribution

    // Worker thread loop: processes tasks from its respective queue
    void workerLoop(size_t i) {
        auto& q = queues[i];  // Get the specific queue for the worker
        Task task;
        while (!stopFlag.load(std::memory_order_acquire)) {
            if (q.dequeue(task)) {
                std::cout << "Executing a task\n";
                task();  // Execute the task
            } else {
                // Queue empty → yield to avoid busy-waiting
                std::this_thread::yield();
            }
        }

        // Drain remaining tasks if any
        while (q.dequeue(task)) {
            task();
        }
    }
};
