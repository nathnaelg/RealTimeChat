#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdlib>

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    void enqueueJob(std::function<void()> job);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;

    std::mutex jobsMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;

    void workerThread();
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] { this->workerThread(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(jobsMutex);
        stop.store(true);
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueueJob(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(jobsMutex);
        jobs.push(std::move(job));
    }
    condition.notify_one();
}

void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(jobsMutex);
            condition.wait(lock, [this] { return stop.load() || !jobs.empty(); });
            if (stop.load() && jobs.empty()) {
                return;
            }
            job = std::move(jobs.front());
            jobs.pop();
        }
        job();
    }
}
