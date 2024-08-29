#include "ThreadPool.h"

ThreadPool::ThreadPool() {
	this->threads = std::thread::hardware_concurrency();
	this->terminate = false;
	for(int i=0; i<threads; i++) {
		workers.emplace_back([this] {
			while(true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(queueLock);
					cv.wait(lock, [this] { return (terminate || !tasks.empty()); });
					if(terminate || tasks.empty()) {
						return;
					}
					task = std::move(tasks.top());
					tasks.pop();
				}
				task();
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