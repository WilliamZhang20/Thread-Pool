#include "ThreadPool.h"

ThreadPool::ThreadPool() {
	this->threads = std::thread::hardware_concurrency();
	this->terminate = false;
	for(int i=0; i<threads; i++) {
		workers.emplace_back([this] {
			std::unique_lock<std::mutex> lock(queueLock);

			startupCv.wait(lock, [this] { return start || terminate; }); // unless told to stop, or the whole process to terminate, wait!

			// keep continuously running up the queue priority list
			while (true) {
                std::function<void()> task;
                cv.wait(lock, [this] { return terminate || !tasks.empty(); });
                if (terminate && tasks.empty()) {
                    return;
                }
                task = std::move(tasks.top().second);
                tasks.pop();
                lock.unlock();
                task();
                lock.lock();
            }
		});
	}
}

ThreadPool* getInstance() {
	static ThreadPool pool;
	return &pool;
}

void ThreadPool::finish() {
	cv.notify_all();
	for(std::thread &worker: workers)
		worker.join();
}

ThreadPool::~ThreadPool() {
	{
		std::unique_lock<std::mutex> lock(queueLock);
		terminate = true;
	}
	finish();
}