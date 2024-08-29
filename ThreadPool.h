#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <future>
#include <stack>
#include <functional>
#include <condition_variable>

class ThreadPool {
private:
	int threads;
	std::vector<std::thread> workers;
	std::stack<std::function<void()>> tasks;

	std::mutex queueLock;
	std::condition_variable cv;
	bool terminate;
public:
	static ThreadPool* getInstance();

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
		using returnType = typename std::result_of<F(Args...)>::type;

		auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::future<returnType> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queueLock);

			// Don't allow enqueueing after stopping / destructing the pool
			if (terminate)
				throw std::runtime_error("enqueue on stopped ThreadPool");


			tasks.emplace([task]() { (*task)(); });
		}
		cv.notify_one();
		return res;
	}

	ThreadPool();
	void finish();
	~ThreadPool();
};
