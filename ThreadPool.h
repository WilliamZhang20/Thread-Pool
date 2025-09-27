#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <future>
#include "LockFreeQueue.h"

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency(),
                    size_t queueCapacity = 1024)
        : stopFlag(false) 
    {
        if (numThreads == 0) {
            numThreads = 1; // fallback in case hardware_concurrency() returns 0
        }

        if ((queueCapacity & (queueCapacity - 1)) != 0) {
            throw std::runtime_error("Queue capacity must be power of 2");
        }

        queues.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            queues.emplace_back(queueCapacity);
        }

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i]() { this->workerLoop(i); });
        }
    }

    ~ThreadPool() {
        stop();
    }

    // submit a task: pick a queue (round robin here)
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();

        size_t idx = rrIndex.fetch_add(1, std::memory_order_relaxed) % queues.size();
        while (!queues[idx].enqueue([task]() { (*task)(); })) {
            std::this_thread::yield();
        }

        return res;
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
