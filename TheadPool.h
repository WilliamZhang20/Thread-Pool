#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <stdexcept>
#include "LockFreeQueue.h"

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t numThreads, size_t queueCapacity = 1024)
        : stopFlag(false) 
    {
        if ((queueCapacity & (queueCapacity - 1)) != 0) {
            throw std::runtime_error("Queue capacity must be power of 2");
        }

        queues.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            queues.emplace_back(queueCapacity);
        }

        // spawn worker threads
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i]() { this->workerLoop(i); });
        }
    }

    ~ThreadPool() {
        stop();
    }

    // submit a task: pick a queue (round robin here)
    void submit(Task task) {
        size_t idx = rrIndex.fetch_add(1, std::memory_order_relaxed) % queues.size();
        while (!queues[idx].enqueue(std::move(task))) {
            // queue full → spin or yield
            std::this_thread::yield();
        }
    }

    void stop() {
        bool expected = false;
        if (!stopFlag.compare_exchange_strong(expected, true)) {
            return; // already stopped
        }

        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::vector<LockFreeQueue<Task>> queues;
    std::atomic<bool> stopFlag;
    std::atomic<size_t> rrIndex{0};

    void workerLoop(size_t i) {
        auto& q = queues[i];
        Task task;
        while (!stopFlag.load(std::memory_order_acquire)) {
            if (q.dequeue(task)) {
                task();
            } else {
                // queue empty → yield
                std::this_thread::yield();
            }
        }
        // drain remaining tasks if any
        while (q.dequeue(task)) {
            task();
        }
    }
};
