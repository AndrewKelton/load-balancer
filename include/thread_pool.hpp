#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
    
    private:

        std::vector<std::thread> threads; // worker threads
        std::queue<std::function<void()>> tasks_; // queue of tasks for workers
        std::mutex queue_mtx_;
        std::condition_variable cv_;
        std::atomic<bool> stop_ = false;
    
    public:

        size_t num_threads; // number of threads

        ThreadPool(size_t num_threads = (std::thread::hardware_concurrency() * 2)) {
            
            this->num_threads = num_threads;

            for (size_t i = 0; i < num_threads; ++i) {
                threads.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mtx_);
                            cv_.wait(lock, [this] { return !tasks_.empty() || stop_.load(); });
                        
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
            
            stop_.store(true);
            cv_.notify_all();

            for (auto& thread : threads) {
                thread.join();
            }
        }

        void enqueue(std::function<void()> task) {
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                tasks_.emplace(std::move(task));
            }
            cv_.notify_one();
        }
};

#endif