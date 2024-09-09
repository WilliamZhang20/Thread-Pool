#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <future>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>

struct Compare {
    bool operator()(const std::pair<int, std::function<void()>>& lhs,
                    const std::pair<int, std::function<void()>>& rhs) const {
        // Min-heap: prioritize lower int values
        return lhs.first > rhs.first;
    }
};

class ThreadPool {
private:
	int threads;
	std::vector<std::thread> workers;
	 std::priority_queue<
        std::pair<int, std::function<void()>>,
        std::vector<std::pair<int, std::function<void()>>>,
		Compare
    > tasks;

	std::mutex queueLock;
	std::condition_variable cv;
    std::condition_variable startupCv;
	bool terminate;
	bool start;
public:
	static ThreadPool* getInstance();

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args, int priority = 1) -> std::future<typename std::result_of<F(Args...)>::type> {
		using returnType = typename std::result_of<F(Args...)>::type;
		// std::cout << "enqueueing priority: " << priority << "\n";
		auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::future<returnType> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queueLock);

			// Don't allow enqueueing after stopping / destructing the pool
			if (terminate)
				throw std::runtime_error("enqueue on stopped ThreadPool");
			
        	tasks.emplace(priority, [task]() { (*task)(); });

			// If task with priority 1 is enqueued, signal the threads to start
            if (priority <= 2) {
                start = true;
                startupCv.notify_all(); // notify all waiting workers (which are doing nothing till this point)
            }
		}
		cv.notify_one();
		return res;
	}

	ThreadPool();
	void finish();
	~ThreadPool();
};
