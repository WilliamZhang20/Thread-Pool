#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

class ThreadPool {
public:
	ThreadPool(size_t numThreads) : stop_(false) {
		for (size_t i = 0; i < numThreads; ++i) {
			workers_.emplace_back([this] {
				while (true) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(queueMutex_);
						condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
						if (stop_ && tasks_.empty()) {
							return;
						}
						task = std::move(tasks_.front());
						tasks_.pop();
					}
					task();
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
		for (std::thread& worker : workers_) {
			worker.join();
		}
	}

	template <typename F, typename... Args>
	auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
		using return_type = std::invoke_result_t<F, Args...>;
	
		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			if (stop_) {
				throw std::runtime_error("enqueue on stopped ThreadPool");
			}
			tasks_.emplace([task]() { (*task)(); });
		}
		condition_.notify_one();
		return res;
	}

private:
	std::vector<std::thread> workers_;
	std::queue<std::function<void()>> tasks_;
	std::mutex queueMutex_;
	std::condition_variable condition_;
	bool stop_;
};