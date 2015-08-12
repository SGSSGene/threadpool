#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "blockingQueue.h"

#include <thread>
#include <atomic>
#include <pthread.h>

namespace threadPool {

template<typename T>
class ThreadPool final {
private:
	BlockingQueue<T> blockingQueue;
	std::mutex ctMutex;
	int        ct;
	std::condition_variable queueIsEmpty;
	std::mutex threadCtMutex;
	int        threadCt;
	std::atomic_bool finish;
	std::condition_variable threadIsEmpty;

	std::vector<std::unique_ptr<std::thread>> threadList;

public:
	ThreadPool()
		: ct(0)
		, threadCt(0)
		, finish(false)
	{}

	/*
	 * Will abort all threads and open jobs
	 */
	~ThreadPool() {
		std::unique_lock<std::mutex> lock(threadCtMutex);
		finish = true;
		blockingQueue.forceFinish();
		if (threadCt > 0) {
			threadIsEmpty.wait(lock);
		}
	}

	/**
	 * This will queue a new object, but will not block
	 */
	void queue(T const& t) {
		std::unique_lock<std::mutex> lock(ctMutex);
		++ct;
		blockingQueue.queue(t);
	}

	/**
	 * This will queue a new object, but will not block
	 */
	void queue(T&& t) {
		std::unique_lock<std::mutex> lock(ctMutex);
		++ct;
		blockingQueue.queue(std::move(t));
	}


	/**
	 * Wait till queue is empty
	 *
	 */
	void wait() {
		std::unique_lock<std::mutex> lock(ctMutex);
		if (ct > 0) {
			queueIsEmpty.wait(lock);
		}
	}

	/**
	 * Spawns new threads that can work on a job
	 * This function should only be called once
	 *
	 * @TODO multiple calls of this function will lead to more and more threads,
	 *       because they never exit.
	 * @TODO This function can only be called when there are no jobs in the queue
	 *
	 * @_f       function that the threads should execute
	 * @threadCt number of threads that should be spawned
	 */
	void spawnThread(std::function<void(T t)> _f, int _threadCt) {
		std::unique_lock<std::mutex> lock(threadCtMutex);
		threadCt += _threadCt;

		threadList.clear();
		for (int i(0); i < _threadCt; ++i) {
			auto p = new std::thread([this, _f]() {
				while (true) {
					T job = blockingQueue.dequeue();
					if (finish) {
						bool threadPoolEmpty;
						// Own scope for lock
						{
							std::unique_lock<std::mutex> lock(threadCtMutex);
							--threadCt;
							threadPoolEmpty = threadCt == 0;
						}
						if (threadPoolEmpty) {
							threadIsEmpty.notify_one();
						}
						return;
					}
					_f(job);
					std::unique_lock<std::mutex> lock(ctMutex);
					--ct;
					if (ct == 0) {
						queueIsEmpty.notify_one();
					}
				}
			});
			threadList.push_back(std::unique_ptr<std::thread>(p));
		}
	}
};

}

#endif
