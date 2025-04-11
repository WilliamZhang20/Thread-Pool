#pragma once

#include <vector>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop_(false) {
        workers_.resize(numThreads);
        queues_.resize(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            workers_[i] = std::thread([this, i] {
                while (true) {
                    std::function<void()> task;
                    if (pop_task(i, task)) {
                        task();
                    } else {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] { return stop_; });
                        if (stop_) break;
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        auto wrapper = [task]() { (*task)(); };
        
        size_t i = std::hash<std::thread::id>{}(std::this_thread::get_id()) % queues_.size();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
			if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");
            queues_[i].push_back(std::move(wrapper));
        }
        condition_.notify_one();
        return task->get_future();
    }

private:
    std::vector<std::thread> workers_;
    std::vector<std::deque<std::function<void()>>> queues_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;

    bool pop_task(size_t idx, std::function<void()>& task) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        // 1. Try own queue
        if (!queues_[idx].empty()) {
            task = std::move(queues_[idx].front());
            queues_[idx].pop_front();
            return true;
        }

        // 2. Try to steal from others
        for (size_t i = 0; i < queues_.size(); ++i) {
            size_t victim = (idx + i) % queues_.size();
            if (!queues_[victim].empty()) {
                task = std::move(queues_[victim].back());
                queues_[victim].pop_back();
                return true;
            }
        }

        return false;
    }
};
